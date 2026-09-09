#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace huxer_drive {

using Id = std::uint64_t;
enum class Area { Files, Recent, Favorites, Shares, Transfers, Trash, Storage, Settings };
enum class Kind { Folder, Image, Text, Document, Other };
enum class Error { None, Busy, Io, InvalidName, Conflict, InvalidDestination, Missing, TooLarge, InvalidCode, Expired, Corrupt, DemoFailure };
enum class Action { Uploaded, Renamed, Moved, Copied, Trashed, Restored, Shared, Unshared, VersionRestored, Created };
enum class Conflict { KeepBoth, Replace, Skip };

struct Version {
  Id blob = 0;
  std::uint64_t size = 0;
  std::int64_t time = 0;
  bool operator==(const Version&) const = default;
};
struct Activity {
  Action action = Action::Created;
  std::int64_t time = 0;
  bool operator==(const Activity&) const = default;
};
struct Entry {
  Id id = 0;
  Id parent = 0;
  std::string name;
  bool folder = false;
  bool favorite = false;
  bool trashed = false;
  std::int64_t modified = 0;
  std::int64_t accessed = 0;
  Id share = 0;
  std::string code;
  std::int64_t expires = 0;
  std::vector<Version> versions;
  std::vector<Activity> activity;
  bool operator==(const Entry&) const = default;
};
struct Snapshot {
  std::uint64_t revision = 0;
  Id next_id = 1;
  Id next_blob = 1;
  Id next_share = 1;
  std::vector<Entry> entries;
  bool operator==(const Snapshot&) const = default;
};

inline constexpr std::uint64_t kCapacity = 100ULL * 1024 * 1024 * 1024;
inline constexpr std::uint64_t kImportLimit = 128ULL * 1024 * 1024;
std::int64_t Now();
std::string Fold(std::string_view value);
bool ValidName(std::string_view value);
Entry* Find(Snapshot& data, Id id);
const Entry* Find(const Snapshot& data, Id id);
bool Descendant(const Snapshot& data, Id id, Id ancestor);
bool InTrash(const Snapshot& data, Id id);
Id NameConflict(const Snapshot& data, Id parent, std::string_view name, Id except = 0);
std::string AvailableName(const Snapshot& data, Id parent, std::string name);
std::vector<Id> TopLevelSelection(const Snapshot& data, const std::vector<Id>& ids);
Kind FileKind(const Entry& entry);
std::uint64_t StoredBytes(const Snapshot& data);
void Record(Entry& entry, Action action);
Error CreateFolder(Snapshot& data, Id parent, std::string name);
Error Rename(Snapshot& data, Id id, std::string name);
Error Relocate(Snapshot& data, const std::vector<Id>& ids, Id destination, bool copy);
Error Trash(Snapshot& data, const std::vector<Id>& ids, bool restore);
Error Purge(Snapshot& data, const std::vector<Id>& ids);
bool Validate(const Snapshot& data);
std::string Serialize(const Snapshot& data);
std::optional<Snapshot> Deserialize(std::string_view encoded);

} // namespace huxer_drive
