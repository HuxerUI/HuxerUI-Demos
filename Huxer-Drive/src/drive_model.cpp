#include "drive_model.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

namespace huxer_drive {

std::int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string Fold(std::string_view value) {
  std::string result(value);
  for (char& ch : result) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  return result;
}
bool ValidName(std::string_view value) {
  return !value.empty() && value.size() <= 240 && value != "." && value != ".." &&
         value.front() != ' ' && value.back() != ' ' &&
         std::ranges::none_of(value, [](unsigned char c) { return c < 32 || c == 127 || c == '/' || c == '\\'; });
}
Entry* Find(Snapshot& data, Id id) {
  auto found = std::ranges::find(data.entries, id, &Entry::id);
  return found == data.entries.end() ? nullptr : &*found;
}
const Entry* Find(const Snapshot& data, Id id) {
  auto found = std::ranges::find(data.entries, id, &Entry::id);
  return found == data.entries.end() ? nullptr : &*found;
}
bool Descendant(const Snapshot& data, Id id, Id ancestor) {
  for (std::size_t depth = 0; id && depth <= data.entries.size(); ++depth) {
    if (id == ancestor) return true;
    const Entry* item = Find(data, id);
    if (!item) break;
    id = item->parent;
  }
  return false;
}
bool InTrash(const Snapshot& data, Id id) {
  for (std::size_t depth = 0; id && depth <= data.entries.size(); ++depth) {
    const Entry* item = Find(data, id);
    if (!item) return false;
    if (item->trashed) return true;
    id = item->parent;
  }
  return false;
}
Id NameConflict(const Snapshot& data, Id parent, std::string_view name, Id except) {
  const std::string key = Fold(name);
  for (const Entry& item : data.entries)
    if (item.id != except && item.parent == parent && !item.trashed && Fold(item.name) == key) return item.id;
  return 0;
}
std::string AvailableName(const Snapshot& data, Id parent, std::string name) {
  const std::size_t dot = name.find_last_of('.');
  const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
  const std::string suffix = dot == std::string::npos ? "" : name.substr(dot);
  for (int index = 2; NameConflict(data, parent, name); ++index) name = stem + " (" + std::to_string(index) + ")" + suffix;
  return name;
}
std::vector<Id> TopLevelSelection(const Snapshot& data, const std::vector<Id>& ids) {
  std::vector<Id> result;
  for (Id id : ids) {
    if (!Find(data, id) || std::ranges::find(result, id) != result.end()) continue;
    if (std::ranges::none_of(ids, [&](Id other) { return other != id && Descendant(data, id, other); })) result.push_back(id);
  }
  return result;
}
Kind FileKind(const Entry& entry) {
  if (entry.folder) return Kind::Folder;
  const auto dot = entry.name.find_last_of('.');
  const std::string ext = dot == std::string::npos ? "" : Fold(entry.name.substr(dot));
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") return Kind::Image;
  if (ext == ".txt" || ext == ".csv" || ext == ".md") return Kind::Text;
  if (ext == ".pdf" || ext == ".docx" || ext == ".xlsx") return Kind::Document;
  return Kind::Other;
}
std::uint64_t StoredBytes(const Snapshot& data) {
  std::set<Id> seen;
  std::uint64_t total = 0;
  for (const Entry& item : data.entries)
    for (const Version& version : item.versions) if (seen.insert(version.blob).second) total += version.size;
  return total;
}
void Record(Entry& entry, Action action) {
  entry.modified = Now();
  entry.activity.push_back({action, entry.modified});
  if (entry.activity.size() > 80) entry.activity.erase(entry.activity.begin());
}
static bool DestinationValid(const Snapshot& data, Id parent) {
  const Entry* item = Find(data, parent);
  return parent == 0 || (item && item->folder && !InTrash(data, parent));
}
Error CreateFolder(Snapshot& data, Id parent, std::string name) {
  if (!ValidName(name)) return Error::InvalidName;
  if (!DestinationValid(data, parent)) return Error::InvalidDestination;
  if (NameConflict(data, parent, name)) return Error::Conflict;
  Entry item{.id = data.next_id++, .parent = parent, .name = std::move(name), .folder = true};
  Record(item, Action::Created);
  data.entries.push_back(std::move(item));
  return Error::None;
}
Error Rename(Snapshot& data, Id id, std::string name) {
  Entry* item = Find(data, id);
  if (!item || InTrash(data, id)) return Error::Missing;
  if (!ValidName(name)) return Error::InvalidName;
  if (NameConflict(data, item->parent, name, id)) return Error::Conflict;
  item->name = std::move(name);
  Record(*item, Action::Renamed);
  return Error::None;
}
Error Relocate(Snapshot& data, const std::vector<Id>& ids, Id destination, bool copy) {
  if (!DestinationValid(data, destination)) return Error::InvalidDestination;
  const auto roots = TopLevelSelection(data, ids);
  if (roots.empty()) return Error::Missing;
  for (Id id : roots) {
    if (InTrash(data, id) || Descendant(data, destination, id)) return Error::InvalidDestination;
    const Entry* item = Find(data, id);
    if (!copy && NameConflict(data, destination, item->name, id)) return Error::Conflict;
  }
  if (!copy) {
    std::set<std::string> names;
    for (Id id : roots) if (!names.insert(Fold(Find(data, id)->name)).second) return Error::Conflict;
  }
  const Snapshot source = data;
  for (Id id : roots) {
    if (!copy) {
      auto* item = Find(data, id);
      item->parent = destination;
      Record(*item, Action::Moved);
      continue;
    }
    const auto clone = [&](auto&& self, Id source_id, Id parent) -> void {
      Entry item = *Find(source, source_id);
      item.id = data.next_id++;
      item.parent = parent;
      item.name = AvailableName(data, parent, item.name);
      item.favorite = false;
      item.share = 0;
      item.code.clear();
      item.expires = 0;
      item.activity.clear();
      const Id created = item.id;
      Record(item, Action::Copied);
      data.entries.push_back(std::move(item));
      for (const Entry& child : source.entries) if (child.parent == source_id && !child.trashed) self(self, child.id, created);
    };
    clone(clone, id, destination);
  }
  return Error::None;
}
Error Trash(Snapshot& data, const std::vector<Id>& ids, bool restore) {
  for (Id id : TopLevelSelection(data, ids)) {
    Entry* item = Find(data, id);
    if (restore) {
      if (!item->trashed) continue;
      if (InTrash(data, item->parent)) item->parent = 0;
      item->name = AvailableName(data, item->parent, item->name);
    }
    item->trashed = !restore;
    Record(*item, restore ? Action::Restored : Action::Trashed);
    if (!restore) for (Entry& child : data.entries) if (Descendant(data, child.id, id)) { child.share = 0; child.code.clear(); }
  }
  return Error::None;
}
Error Purge(Snapshot& data, const std::vector<Id>& ids) {
  for (Id id : ids) if (!InTrash(data, id)) return Error::InvalidDestination;
  std::vector<Id> remove;
  for (const Entry& item : data.entries)
    if (std::ranges::any_of(ids, [&](Id id) { return Descendant(data, item.id, id); })) remove.push_back(item.id);
  std::erase_if(data.entries, [&](const Entry& item) { return std::ranges::find(remove, item.id) != remove.end(); });
  return Error::None;
}
bool Validate(const Snapshot& data) {
  if (!data.next_id || !data.next_blob || !data.next_share || data.entries.size() > 20000) return false;
  std::set<Id> ids;
  std::map<Id, std::uint64_t> blobs;
  std::set<std::pair<Id, std::string>> names;
  for (const Entry& item : data.entries) {
    if (!item.id || item.id >= data.next_id || !ids.insert(item.id).second || !ValidName(item.name)) return false;
    if (item.parent) {
      const Entry* parent = Find(data, item.parent);
      if (!parent || !parent->folder || Descendant(data, item.parent, item.id)) return false;
    }
    if (!item.trashed && !names.insert({item.parent, Fold(item.name)}).second) return false;
    if (item.share >= data.next_share || item.versions.size() > 1000 || item.activity.size() > 80) return false;
    if (item.folder != item.versions.empty()) return false;
    for (const auto& version : item.versions) {
      if (!version.blob || version.blob >= data.next_blob || version.size > kImportLimit) return false;
      const auto [stored, inserted] = blobs.emplace(version.blob, version.size);
      if (!inserted && stored->second != version.size) return false;
    }
  }
  return StoredBytes(data) <= kCapacity;
}
static std::uint64_t Checksum(std::string_view text) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (unsigned char ch : text) { hash ^= ch; hash *= 1099511628211ULL; }
  return hash;
}
std::string Serialize(const Snapshot& data) {
  std::ostringstream out;
  out << data.revision << ' ' << data.next_id << ' ' << data.next_blob << ' ' << data.next_share << ' ' << data.entries.size() << '\n';
  for (const Entry& item : data.entries) {
    out << item.id << ' ' << item.parent << ' ' << std::quoted(item.name) << ' ' << item.folder << ' ' << item.favorite << ' ' << item.trashed << ' '
        << item.modified << ' ' << item.accessed << ' ' << item.share << ' ' << std::quoted(item.code) << ' ' << item.expires << ' '
        << item.versions.size() << ' ' << item.activity.size() << '\n';
    for (const Version& version : item.versions) out << version.blob << ' ' << version.size << ' ' << version.time << '\n';
    for (const Activity& activity : item.activity) out << static_cast<int>(activity.action) << ' ' << activity.time << '\n';
  }
  const std::string body = out.str();
  return "HUXER_DRIVE_1 " + std::to_string(Checksum(body)) + "\n" + body;
}
std::optional<Snapshot> Deserialize(std::string_view encoded) {
  if (encoded.size() > 16 * 1024 * 1024) return std::nullopt;
  const auto newline = encoded.find('\n');
  if (newline == std::string_view::npos) return std::nullopt;
  std::istringstream header{std::string(encoded.substr(0, newline))};
  std::string magic;
  std::uint64_t checksum = 0;
  if (!(header >> magic >> checksum) || magic != "HUXER_DRIVE_1" || checksum != Checksum(encoded.substr(newline + 1))) return std::nullopt;
  std::istringstream in{std::string(encoded.substr(newline + 1))};
  Snapshot data;
  std::size_t count = 0;
  if (!(in >> data.revision >> data.next_id >> data.next_blob >> data.next_share >> count) || count > 20000) return std::nullopt;
  for (std::size_t index = 0; index < count; ++index) {
    Entry item;
    std::size_t versions = 0, activities = 0;
    if (!(in >> item.id >> item.parent >> std::quoted(item.name) >> item.folder >> item.favorite >> item.trashed >> item.modified >> item.accessed >> item.share >> std::quoted(item.code) >> item.expires >> versions >> activities) || versions > 1000 || activities > 80) return std::nullopt;
    for (std::size_t v = 0; v < versions; ++v) { Version version; if (!(in >> version.blob >> version.size >> version.time)) return std::nullopt; item.versions.push_back(version); }
    for (std::size_t a = 0; a < activities; ++a) {
      int action; Activity activity;
      if (!(in >> action >> activity.time) || action < 0 || action > static_cast<int>(Action::Created)) return std::nullopt;
      activity.action = static_cast<Action>(action); item.activity.push_back(activity);
    }
    data.entries.push_back(std::move(item));
  }
  in >> std::ws;
  if (!in.eof() || !Validate(data)) return std::nullopt;
  return data;
}

} // namespace huxer_drive
