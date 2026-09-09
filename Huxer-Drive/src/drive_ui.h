#pragma once

#include <app_resources.h>
#include "drive_store.h"

namespace huxer_drive {
using namespace huxerui;

struct Route {
  Id id = 0;
  bool preview = false;
  bool operator==(const Route&) const = default;
};
struct Transfer {
  Id id = 0;
  std::string name;
  std::optional<FileReference> source;
  Id parent = 0;
  Id version_of = 0;
  Id download_id = 0;
  Id download_blob = 0;
  std::uint64_t done = 0;
  std::uint64_t total = 0;
  bool running = false;
  bool completed = false;
  bool demonstration = false;
  Error error = Error::None;
  bool operator==(const Transfer& other) const {
    return id == other.id && name == other.name && parent == other.parent && version_of == other.version_of &&
           download_id == other.download_id && download_blob == other.download_blob && done == other.done &&
           total == other.total && running == other.running && completed == other.completed &&
           demonstration == other.demonstration && error == other.error;
  }
};
struct DriveContext {
  State<Snapshot> data;
  State<bool> ready;
  State<bool> busy;
  State<Error> error;
  State<Area> area;
  State<Area> file_area;
  State<NavigationPath<Route>> path;
  State<TextEditingValue> search;
  State<std::string> query;
  State<bool> searching;
  State<int> filter;
  State<int> sort;
  State<bool> reverse;
  State<bool> grid;
  State<bool> dark;
  State<std::vector<Id>> selected;
  State<std::vector<Transfer>> transfers;
  State<bool> transfer_panel;
  State<Conflict> conflict;
  TaskScope tasks;
  std::shared_ptr<DriveStore> store;
  std::shared_ptr<FilePicker> picker;
  std::optional<DialogHandle> dialogs;
  static DriveContext Default() { return {}; }
  bool operator==(const DriveContext& other) const noexcept { return store == other.store; }
};

StringResource ErrorLabel(Error error);
StringResource AreaLabel(Area area);
ImageResource AreaIcon(Area area);
std::string FormatSize(std::uint64_t size);
std::string FormatDate(std::int64_t time);
View Label(StringVariant text, float size = 14, bool muted = false, bool bold = false);
View Glyph(ImageResource icon, float size = 20, std::optional<Color> tint = {});
View QuietButton(StringVariant text, std::function<void()> action, float height = 36);
enum class ActionStyle { Outline, Primary, Selected, Plain, Link };
View ActionButton(std::optional<ImageResource> icon, StringVariant text, std::function<void()> action,
                  ActionStyle style = ActionStyle::Outline, bool chevron = false, float height = 36);
View DialogActions(StringVariant confirm, std::function<void()> cancel, std::function<void()> submit, bool enabled = true);
View FileArtwork(Entry item, float size, bool thumbnail = true);
View DriveSearch();
View Avatar(bool small = false);
View PreferencesButton();
View SettingsPage();
View SingleLine(StringVariant text, float size = 15, bool muted = false, bool bold = false);
View MobileAction(ImageResource icon, StringVariant label, std::function<void()> action, bool destructive = false);
struct SheetAction {
  ImageResource icon;
  StringVariant label;
  std::function<void()> action;
  bool destructive = false;
};
void ShowActions(BottomSheetHandle sheets, StringVariant title, std::vector<SheetAction> actions);
void GoBack(const DriveContext& context);
StringVariant ShortDate(std::int64_t time);
ThemeDefinition DriveTheme(bool dark, bool compact);
View DriveDialog(DriveContext context, View content);
void Navigate(const DriveContext& context, Id id, bool preview);
void GoArea(const DriveContext& context, Area area);
void Run(const DriveContext& context, std::function<Task<Error>()> operation, std::function<void()> success = {});
void Change(const DriveContext& context, std::function<Error(Snapshot&)> change, std::function<void()> success = {});
void Upload(const DriveContext& context, Id parent, Id version_of = 0, bool directory = false);
void ImportFiles(const DriveContext& context, std::vector<FileReference> files, Id parent, Id version_of = 0);
void RetryTransfer(const DriveContext& context, Id id);
void Download(const DriveContext& context, Id id, Id blob = 0);
void DownloadSelection(const DriveContext& context, std::vector<Id> ids);
void RenameDialog(const DriveContext& context, Id id, Id parent, bool create);
void DestinationDialog(const DriveContext& context, std::vector<Id> ids, bool copy);
void ShareDialog(const DriveContext& context, Id id);
void ConfirmPurge(const DriveContext& context, std::vector<Id> ids);
View DriveRoot();
View Workspace(Id folder, std::optional<Area> page_area = {});
View Preview(Id id);
View TransferList(bool compact_panel = false);

} // namespace huxer_drive
