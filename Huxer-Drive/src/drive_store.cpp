#include "drive_store.h"

#include <algorithm>
#include <cstring>
#include <set>

namespace huxer_drive {

Task<Error> DriveStore::Load() {
  co_await Delay(250ms);
  if (!co_await root_.Child("blobs").CreateDirectoriesAsync()) co_return Error::Io;
  std::optional<Snapshot> latest;
  bool existing = false;
  for (int slot = 0; slot < 2; ++slot) {
    File file = root_.Child("index-" + std::to_string(slot));
    const auto stat = co_await file.StatAsync();
    if (!stat.Succeeded()) {
      if (stat.Error().code != IoErrorCode::NotFound) co_return Error::Io;
      continue;
    }
    existing = true;
    if (stat.Value().size > 16 * 1024 * 1024) continue;
    const auto encoded = co_await file.ReadStringAsync();
    if (!encoded.Succeeded()) continue;
    auto parsed = Deserialize(encoded.Value());
    if (parsed && (!latest || parsed->revision > latest->revision)) latest = std::move(parsed);
  }
  if (latest) {
    data_ = std::move(*latest);
    co_return Error::None;
  }
  if (existing) co_return Error::Corrupt;
  co_return co_await Seed();
}
Task<Error> DriveStore::Commit(Snapshot next) {
  if (!Validate(next)) co_return Error::Corrupt;
  next.revision = data_.revision + 1;
  const std::string encoded = Serialize(next);
  const File slot = root_.Child("index-" + std::to_string(next.revision % 2));
  if (!co_await slot.WriteStringAsync(encoded)) co_return Error::Io;
  const auto verified = co_await slot.ReadStringAsync();
  if (!verified.Succeeded() || verified.Value() != encoded) co_return Error::Io;
  data_ = std::move(next);
  co_return Error::None;
}
Task<Error> DriveStore::Mutate(std::function<Error(Snapshot&)> change) {
  co_await Delay(120ms);
  Snapshot next = data_;
  const Error error = change(next);
  if (error != Error::None) co_return error;
  co_return co_await Commit(std::move(next));
}
Task<Error> DriveStore::AddBytes(Snapshot& next, Id parent, std::string name, Bytes bytes) {
  const Id blob = next.next_blob++;
  if (!co_await Blob(blob).WriteBytesAsync(bytes)) co_return Error::Io;
  Entry item{.id = next.next_id++, .parent = parent, .name = std::move(name)};
  item.versions.push_back({blob, bytes.size(), Now()});
  Record(item, Action::Uploaded);
  next.entries.push_back(std::move(item));
  co_return Error::None;
}
Task<Error> DriveStore::Seed() {
  Snapshot next;
  for (const char* folder : {"Projects", "Personal archive", "Photography"}) {
    const auto error = CreateFolder(next, 0, folder);
    if (error != Error::None) co_return error;
  }
  for (const char* folder : {"Aurora launch", "Research", "Deliverables"}) {
    const auto error = CreateFolder(next, 1, folder);
    if (error != Error::None) co_return error;
  }
  struct Sample { Id parent; const char* name; const char* text; };
  const Sample samples[] = {
      {1, "Project brief.txt", "AURORA / PROJECT BRIEF\n\nA personal study of light, space and everyday objects.\n\nObjective\nCreate a cohesive collection of original visual studies for the autumn portfolio.\n\nDeliverables\nCover illustration, process notes, and a concise presentation.\n\nNext review\nRefine the composition and check the final exports.\n"},
      {1, "Budget overview.csv", "Category,Planned,Actual\nMaterials,240,218\nPrinting,180,164\nTravel,120,96\n"},
      {1, "Research notes.txt", "FIELD NOTES\n\nMorning light changes the texture of paper. Explore a warm neutral palette with a single blue accent.\n\nKeep the final series quiet, clear and consistent.\n"},
      {4, "Release checklist.txt", "AURORA CHECKLIST\n\nReview original artwork\nProofread the project introduction\nExport final image files\nCreate a private review share\nArchive the source materials\n"},
      {2, "Travel journal.txt", "COASTAL JOURNAL\n\nDay one\nA long walk along the shore. Pale sand, deep blue water, and a quiet horizon.\n"},
      {5, "Reading list.txt", "READING LIST\n\nNotes on composition\nColor and natural light\nA practical guide to visual rhythm\n"},
  };
  for (const Sample& sample : samples) {
    Bytes bytes(std::strlen(sample.text));
    std::memcpy(bytes.data(), sample.text, bytes.size());
    const auto error = co_await AddBytes(next, sample.parent, sample.name, std::move(bytes));
    if (error != Error::None) co_return error;
  }
  auto input = co_await sample_.OpenReadAsync();
  Bytes artwork;
  while (true) {
    auto chunk = co_await input.ReadAsync(64 * 1024);
    if (!chunk.Succeeded()) co_return Error::Io;
    if (chunk.Value().empty()) break;
    artwork.insert(artwork.end(), chunk.Value().begin(), chunk.Value().end());
  }
  auto image_result = co_await AddBytes(next, 1, "Coastal study.png", artwork);
  if (image_result != Error::None) co_return image_result;
  image_result = co_await AddBytes(next, 3, "Morning light.png", std::move(artwork));
  if (image_result != Error::None) co_return image_result;
  next.entries[0].favorite = true;
  for (const auto& [name, asset] : std::vector<std::pair<std::string, RawAsset>>{
      {"Brand guidelines.pdf", document_}, {"Launch assets.zip", archive_}}) {
    auto reader = co_await asset.OpenReadAsync();
    Bytes bytes;
    while (true) {
      auto chunk = co_await reader.ReadAsync(64 * 1024);
      if (!chunk.Succeeded()) co_return Error::Io;
      if (chunk.Value().empty()) break;
      bytes.insert(bytes.end(), chunk.Value().begin(), chunk.Value().end());
    }
    const auto result = co_await AddBytes(next, 1, name, std::move(bytes));
    if (result != Error::None) co_return result;
  }
  co_return co_await Commit(std::move(next));
}
Task<Error> DriveStore::Import(FileReference source, Id parent, Conflict policy, Id version_of,
                              std::function<void(std::uint64_t, std::uint64_t)> progress) {
  Snapshot next = data_;
  if (parent && (!Find(next, parent) || !Find(next, parent)->folder || InTrash(next, parent))) co_return Error::InvalidDestination;
  if (!ValidName(source.Name())) co_return Error::InvalidName;
  if (source.Size().value_or(0) > kImportLimit) co_return Error::TooLarge;
  Id existing = version_of ? version_of : NameConflict(next, parent, source.Name());
  if (version_of && (!Find(next, version_of) || Find(next, version_of)->folder || InTrash(next, version_of))) co_return Error::Missing;
  if (existing && !version_of && policy == Conflict::Skip) co_return Error::Conflict;
  if (existing && !version_of && policy == Conflict::KeepBoth) existing = 0;
  if (existing && Find(next, existing)->folder) co_return Error::Conflict;
  std::string name = existing ? Find(next, existing)->name : AvailableName(next, parent, source.Name());
  const Id blob = next.next_blob++;
  File partial = root_.Child("blobs").Child(std::to_string(blob) + ".part");
  auto opened = co_await source.OpenReadAsync();
  if (!opened.Succeeded()) co_return Error::Io;
  auto output = co_await partial.OpenWriteAsync();
  if (!output.Succeeded()) co_return Error::Io;
  auto reader = std::move(opened).Value();
  auto writer = std::move(output).Value();
  std::uint64_t bytes = 0;
  while (true) {
    auto chunk = co_await reader.ReadAsync(256 * 1024);
    if (!chunk.Succeeded()) co_return Error::Io;
    if (chunk.Value().empty()) break;
    bytes += chunk.Value().size();
    if (bytes > kImportLimit || StoredBytes(next) + bytes > kCapacity) co_return Error::TooLarge;
    auto written = co_await writer.WriteAsync(std::move(chunk).Value());
    if (!written.Succeeded()) co_return Error::Io;
    progress(bytes, source.Size().value_or(0));
  }
  if (!(co_await writer.CloseAsync()).Succeeded()) co_return Error::Io;
  if (!co_await partial.MoveToAsync(Blob(blob), true)) co_return Error::Io;
  if (existing) {
    auto* item = Find(next, existing);
    item->versions.push_back({blob, bytes, Now()});
    Record(*item, Action::Uploaded);
  } else {
    Entry item{.id = next.next_id++, .parent = parent, .name = std::move(name)};
    item.versions.push_back({blob, bytes, Now()});
    Record(item, Action::Uploaded);
    next.entries.push_back(std::move(item));
  }
  co_return co_await Commit(std::move(next));
}
Task<Error> DriveStore::SaveText(Id id, std::string text) {
  Snapshot next = data_;
  auto* item = Find(next, id);
  if (!item || InTrash(next, id) || item->folder) co_return Error::Missing;
  const Id blob = next.next_blob++;
  if (text.size() > 1024 * 1024 || StoredBytes(next) + text.size() > kCapacity) co_return Error::TooLarge;
  if (!co_await Blob(blob).WriteStringAsync(text)) co_return Error::Io;
  item->versions.push_back({blob, text.size(), Now()});
  Record(*item, Action::Uploaded);
  co_return co_await Commit(std::move(next));
}
Task<Error> DriveStore::RestoreVersion(Id id, Id blob) {
  co_return co_await Mutate([=](Snapshot& next) {
    auto* item = Find(next, id);
    if (!item || InTrash(next, id)) return Error::Missing;
    auto found = std::ranges::find(item->versions, blob, &Version::blob);
    if (found == item->versions.end()) return Error::Missing;
    Version version = *found;
    version.time = Now();
    item->versions.push_back(version);
    Record(*item, Action::VersionRestored);
    return Error::None;
  });
}
Task<Error> DriveStore::CollectGarbage() {
  // Keep both recoverable index generations' blobs until the older index is replaced.
  std::set<std::string> retained;
  for (int slot = 0; slot < 2; ++slot) {
    auto encoded = co_await root_.Child("index-" + std::to_string(slot)).ReadStringAsync();
    if (!encoded.Succeeded()) continue;
    auto parsed = Deserialize(encoded.Value());
    if (!parsed) continue;
    for (const auto& item : parsed->entries) for (const auto& version : item.versions) retained.insert(std::to_string(version.blob));
  }
  const auto children = co_await root_.Child("blobs").ListChildrenAsync();
  if (!children.Succeeded()) co_return Error::Io;
  for (const auto& child : children.Value()) if (!retained.contains(child.Name())) {
    if (!co_await child.DeleteAsync()) co_return Error::Io;
  }
  co_return Error::None;
}

} // namespace huxer_drive
