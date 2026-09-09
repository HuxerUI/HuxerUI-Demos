#include "drive_ui.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace huxer_drive {

StringResource ErrorLabel(Error error) {
  switch (error) {
  case Error::None: return app::strings::success;
  case Error::Busy: return app::strings::busy;
  case Error::InvalidName: return app::strings::invalid_name;
  case Error::Conflict: return app::strings::conflict;
  case Error::InvalidDestination: return app::strings::invalid_destination;
  case Error::Missing: return app::strings::missing;
  case Error::TooLarge: return app::strings::too_large;
  case Error::InvalidCode: return app::strings::invalid_code;
  case Error::Expired: return app::strings::expired;
  case Error::Corrupt: return app::strings::corrupt;
  case Error::DemoFailure: return app::strings::demo_failure;
  default: return app::strings::io_error;
  }
}
StringResource AreaLabel(Area area) {
  switch (area) {
  case Area::Recent: return app::strings::recent;
  case Area::Favorites: return app::strings::favorites;
  case Area::Shares: return app::strings::shares;
  case Area::Transfers: return app::strings::transfers;
  case Area::Trash: return app::strings::trash;
  case Area::Storage: return app::strings::storage;
  case Area::Settings: return app::strings::settings;
  default: return app::strings::files;
  }
}
ImageResource AreaIcon(Area area) {
  switch (area) {
  case Area::Recent: return app::images::clock;
  case Area::Favorites: return app::images::star;
  case Area::Shares: return app::images::share;
  case Area::Transfers: return app::images::transfer;
  case Area::Trash: return app::images::trash;
  case Area::Storage: return app::images::disk;
  case Area::Settings: return app::images::settings;
  default: return app::images::folder;
  }
}
std::string FormatSize(std::uint64_t size) {
  std::ostringstream text;
  if (size >= 1024ULL * 1024 * 1024) text << std::fixed << std::setprecision(1) << size / (1024.0 * 1024 * 1024) << " GB";
  else if (size >= 1024 * 1024) text << std::fixed << std::setprecision(1) << size / (1024.0 * 1024) << " MB";
  else if (size >= 1024) text << std::fixed << std::setprecision(1) << size / 1024.0 << " KB";
  else text << size << " B";
  return text.str();
}
std::string FormatDate(std::int64_t time) {
  const std::time_t value = static_cast<std::time_t>(time);
  const auto* local = std::localtime(&value);
  if (!local) return "—";
  std::ostringstream text;
  text << std::put_time(local, "%Y-%m-%d %H:%M");
  return text.str();
}
void GoBack(const DriveContext& context) {
  context.selected = std::vector<Id>{};
  context.search = TextEditingValue::FromText("");
  context.query = std::string{};
  context.searching = false;
  const auto path = context.path.Get();
  std::vector<Route> routes(path.Routes().begin(), path.Routes().end());
  if (!routes.empty()) routes.pop_back();
  context.path = NavigationPath<Route>{std::move(routes)};
}
void Navigate(const DriveContext& context, Id id, bool preview) {
  context.selected = std::vector<Id>{};
  context.search = TextEditingValue::FromText("");
  context.query = std::string{};
  context.searching = false;
  const auto& current = context.path.Get();
  std::vector<Route> routes(current.Routes().begin(), current.Routes().end());
  routes.push_back({id, preview});
  context.path = NavigationPath<Route>{std::move(routes)};
}
void GoArea(const DriveContext& context, Area area) {
  if (area == context.area.Get() && context.path.Get().Routes().empty()) return;
  if (area == Area::Files || area == Area::Recent || area == Area::Favorites) context.file_area = area;
  context.area = area;
  context.path = NavigationPath<Route>{};
  context.selected = std::vector<Id>{};
  context.search = TextEditingValue::FromText("");
  context.query = std::string{};
  context.searching = false;
  context.filter = 0;
}
void Run(const DriveContext& context, std::function<Task<Error>()> operation, std::function<void()> success) {
  if (context.busy.Get()) { context.error = Error::Busy; return; }
  context.busy = true;
  context.error = Error::None;
  (void)context.tasks.Launch([=]() -> Task<void> {
    Error result = Error::Io;
    try { result = co_await operation(); } catch (...) { result = Error::Io; }
    context.data = context.store->Data();
    context.error = result;
    context.busy = false;
    if (result == Error::None && success) success();
  });
}
void Change(const DriveContext& context, std::function<Error(Snapshot&)> change, std::function<void()> success) {
  Run(context, [=]() { return context.store->Mutate(change); }, success);
}
static void UpdateTransfer(const DriveContext& context, Id id, std::function<void(Transfer&)> update) {
  context.transfers.Update([=](auto& values) {
    for (auto& value : values) if (value.id == id) { update(value); break; }
  });
}
static Task<Error> ExecuteImport(DriveContext context, Transfer transfer) {
  UpdateTransfer(context, transfer.id, [](auto& t) { t.running = true; t.error = Error::None; t.done = 0; });
  Error result = Error::Io;
  try {
    result = co_await context.store->Import(*transfer.source, transfer.parent, context.conflict.Get(), transfer.version_of,
        [=](std::uint64_t done, std::uint64_t total) { UpdateTransfer(context, transfer.id, [=](auto& t) { t.done = done; t.total = total; }); });
  } catch (...) { result = Error::Io; }
  UpdateTransfer(context, transfer.id, [=](auto& t) { t.running = false; t.completed = result == Error::None; t.error = result; });
  context.data = context.store->Data();
  co_return result;
}
void ImportFiles(const DriveContext& context, std::vector<FileReference> files, Id parent, Id version_of) {
  if (files.empty()) return;
  if (context.busy.Get()) { context.error = Error::Busy; return; }
  std::vector<Transfer> pending;
  auto transfers = context.transfers.Get();
  Id next = transfers.empty() ? 1 : transfers.back().id + 1;
  for (const auto& file : files) {
    pending.push_back({.id = next++, .name = file.Name(), .source = file, .parent = parent, .version_of = version_of, .total = file.Size().value_or(0)});
    transfers.push_back(pending.back());
    if (version_of) break;
  }
  context.transfers = std::move(transfers);
  context.transfer_panel = true;
  Run(context, [=]() -> Task<Error> {
    Error error = Error::None;
    for (const auto& transfer : pending) {
      const auto result = co_await ExecuteImport(context, transfer);
      if (result != Error::None) error = result;
    }
    co_return error;
  });
}
static Task<Error> ImportDirectory(DriveContext context, FileReference directory, Id parent, unsigned depth) {
  if (depth > 24 || context.store->Data().entries.size() > 10000) co_return Error::TooLarge;
  const auto children = co_await directory.ListChildrenAsync();
  if (!children.Succeeded()) co_return Error::Io;
  std::string name = AvailableName(context.store->Data(), parent, directory.Name());
  const Id target = context.store->Data().next_id;
  const auto created = co_await context.store->Mutate([=](Snapshot& next) { return CreateFolder(next, parent, name); });
  if (created != Error::None) co_return created;
  context.data = context.store->Data();
  Error error = Error::None;
  for (const auto& child : children.Value()) {
    Error result;
    if (child.Type() == FileType::Directory) result = co_await ImportDirectory(context, child, target, depth + 1);
    else {
      auto list = context.transfers.Get();
      Transfer transfer{.id = list.empty() ? 1 : list.back().id + 1, .name = child.Name(), .source = child, .parent = target, .total = child.Size().value_or(0)};
      list.push_back(transfer); context.transfers = std::move(list);
      result = co_await ExecuteImport(context, transfer);
    }
    if (result != Error::None) error = result;
  }
  co_return error;
}
void Upload(const DriveContext& context, Id parent, Id version_of, bool directory) {
  if (context.busy.Get()) { context.error = Error::Busy; return; }
  (void)context.tasks.Launch([=]() -> Task<void> {
    try {
    if (directory) {
      auto chosen = co_await context.picker->OpenDirectoryAsync(false);
      if (chosen) {
        context.transfer_panel = true;
        Run(context, [=]() { return ImportDirectory(context, *chosen, parent, 0); });
      }
    } else {
      auto chosen = co_await context.picker->OpenFilesAsync();
      ImportFiles(context, std::move(chosen), parent, version_of);
    }
    } catch (...) { context.error = Error::Io; }
  });
}
void Download(const DriveContext& context, Id id, Id blob) {
  const auto* found = Find(context.data.Get(), id);
  if (!found || found->folder || InTrash(context.data.Get(), id)) { context.error = Error::Missing; return; }
  const Entry entry = *found;
  const Id selected = blob ? blob : entry.versions.back().blob;
  auto list = context.transfers.Get();
  Transfer transfer{.id = list.empty() ? 1 : list.back().id + 1, .name = entry.name, .download_id = id, .download_blob = selected, .running = true};
  if (context.busy.Get()) { context.error = Error::Busy; return; }
  list.push_back(transfer); context.transfers = std::move(list);
  context.transfer_panel = true;
  Run(context, [=]() -> Task<Error> {
    bool success = false;
    try { success = co_await context.picker->SaveFileAsync(context.store->Blob(selected), {.suggested_name = entry.name}); }
    catch (...) { success = false; }
    UpdateTransfer(context, transfer.id, [=](auto& t) { t.running = false; t.completed = success; t.error = success ? Error::None : Error::Io; });
    co_return Error::None;
  });
}
void DownloadSelection(const DriveContext& context, std::vector<Id> ids) {
  if (context.busy.Get()) return;
  std::vector<Entry> files;
  for (Id id : ids) if (const auto* item = Find(context.data.Get(), id))
    if (!item->folder && !InTrash(context.data.Get(), id)) files.push_back(*item);
  if (files.empty()) return;
  context.transfer_panel = true;
  Run(context, [=]() -> Task<Error> {
    for (const auto& item : files) {
      auto values = context.transfers.Get();
      const Id transfer_id = values.empty() ? 1 : values.back().id + 1;
      const Id blob = item.versions.back().blob;
      values.push_back({.id = transfer_id, .name = item.name, .download_id = item.id, .download_blob = blob, .running = true});
      context.transfers = std::move(values);
      bool saved = false;
      try { saved = co_await context.picker->SaveFileAsync(context.store->Blob(blob), {.suggested_name = item.name}); }
      catch (...) { saved = false; }
      UpdateTransfer(context, transfer_id, [=](auto& transfer) {
        transfer.running = false; transfer.completed = saved; transfer.error = saved ? Error::None : Error::Io;
      });
      if (!saved) break;
    }
    co_return Error::None;
  });
}
void RetryTransfer(const DriveContext& context, Id id) {
  auto transfers = context.transfers.Get();
  auto found = std::ranges::find(transfers, id, &Transfer::id);
  if (found == transfers.end() || context.busy.Get()) return;
  const Transfer transfer = *found;
  if (transfer.demonstration) {
    Run(context, [=]() -> Task<Error> {
      UpdateTransfer(context, id, [](auto& t) { t.running = true; t.error = Error::None; });
      co_await Delay(500ms);
      UpdateTransfer(context, id, [](auto& t) { t.running = false; t.completed = true; });
      co_return Error::None;
    });
  } else if (transfer.source) Run(context, [=]() { return ExecuteImport(context, transfer); });
  else Run(context, [=]() -> Task<Error> {
    UpdateTransfer(context, id, [](auto& t) { t.running = true; t.error = Error::None; });
    bool success = false;
    try { success = co_await context.picker->SaveFileAsync(context.store->Blob(transfer.download_blob), {.suggested_name = transfer.name}); }
    catch (...) { success = false; }
    UpdateTransfer(context, id, [=](auto& t) { t.running = false; t.completed = success; t.error = success ? Error::None : Error::Io; });
    co_return Error::None;
  });
}

} // namespace huxer_drive
