#include "../src/drive_model.h"

#include <cstdlib>
#include <iostream>

using namespace huxer_drive;

static void Require(bool condition, const char* message) {
  if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
static Id AddFile(Snapshot& data, Id parent, std::string name, std::uint64_t size) {
  Entry item{.id = data.next_id++, .parent = parent, .name = std::move(name)};
  item.versions.push_back({data.next_blob++, size, Now()});
  data.entries.push_back(item);
  return item.id;
}
int main() {
  Snapshot data;
  Require(CreateFolder(data, 0, "Projects") == Error::None, "Create root folder");
  Require(CreateFolder(data, 1, "Research") == Error::None, "Create nested folder");
  const Id file = AddFile(data, 2, "Notes.txt", 120);
  Require(Validate(data), "Initial tree validates");
  Require(CreateFolder(data, 1, "research") == Error::Conflict, "Case-insensitive duplicate rejected");
  Require(CreateFolder(data, 1, "../escape") == Error::InvalidName, "Traversal name rejected");
  Require(CreateFolder(data, file, "Invalid") == Error::InvalidDestination, "File cannot parent a folder");
  Require(Rename(data, file, " notes.txt") == Error::InvalidName, "Leading whitespace rejected");
  Require(Relocate(data, {1}, 2, false) == Error::InvalidDestination, "Move into descendant rejected");
  Require(Relocate(data, {1}, 2, true) == Error::InvalidDestination, "Copy into descendant rejected");
  Require(TopLevelSelection(data, {1, 2, file, 1}).size() == 1, "Nested bulk selection normalized");
  const auto bytes = StoredBytes(data);
  Require(Relocate(data, {2, file}, 0, true) == Error::None, "Copy selected subtree");
  Require(data.entries.size() == 5, "Nested selection copied only once");
  Require(StoredBytes(data) == bytes, "Copy shares immutable version content");
  Require(Validate(data), "Copied tree validates");
  auto* original = Find(data, file);
  original->share = data.next_share++; original->code = "123456"; original->expires = Now() + 86400;
  Require(Trash(data, {1}, false) == Error::None, "Trash parent");
  Require(InTrash(data, file) && Find(data, file)->share == 0, "Descendant is trashed and share is revoked");
  Require(CreateFolder(data, 0, "Projects") == Error::None, "Trashed name can be reused");
  Require(Trash(data, {1}, true) == Error::None, "Restore folder");
  Require(Find(data, 1)->name == "Projects (2)", "Restore resolves name conflict");
  Require(!InTrash(data, file), "Restoring parent exposes descendants");
  Require(Purge(data, {1}) == Error::InvalidDestination, "Cannot permanently delete active files");
  Require(Trash(data, {1}, false) == Error::None && Purge(data, {1}) == Error::None, "Purge trashed subtree");
  Require(!Find(data, file) && data.entries.size() == 3, "Purge removes exact subtree");
  Require(StoredBytes(data) == bytes, "Remaining copy retains content");
  Require(Validate(data), "Final tree validates");
  const auto encoded = Serialize(data);
  Require(Deserialize(encoded) == data, "Index round trip");
  for (std::size_t cut = 0; cut < encoded.size(); ++cut)
    Require(!Deserialize(encoded.substr(0, cut)), "Truncated index rejected");
  std::string broken = encoded; broken.back() ^= 1;
  Require(!Deserialize(broken), "Checksum rejects corrupted index");
  Snapshot cycle; CreateFolder(cycle, 0, "A"); CreateFolder(cycle, 1, "B"); Find(cycle, 1)->parent = 2;
  Require(!Validate(cycle), "Cyclic hierarchy rejected");
  Snapshot duplicate = data; duplicate.entries.push_back(duplicate.entries.front());
  Require(!Validate(duplicate), "Duplicate identities rejected");
  Snapshot malicious = data; malicious.entries.front().name = "../../outside";
  Require(!Deserialize(Serialize(malicious)), "Index cannot inject a path traversal");
  Snapshot collision; CreateFolder(collision, 0, "A"); CreateFolder(collision, 0, "B");
  Id one = AddFile(collision, 1, "same.txt", 1), two = AddFile(collision, 2, "same.txt", 1);
  Require(Relocate(collision, {one, two}, 0, false) == Error::Conflict, "Bulk same-name move rejected before mutation");
  Require(Find(collision, one)->parent == 1 && Find(collision, two)->parent == 2, "Failed bulk move is unchanged");
  std::cout << "Drive model scenarios passed\n";
}
