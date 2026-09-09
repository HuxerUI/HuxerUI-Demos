#pragma once

#include <functional>
#include <huxerui/huxerui.h>
#include "drive_model.h"

namespace huxer_drive {
using namespace huxerui;

class DriveStore {
public:
  DriveStore(File root, RawAsset sample, RawAsset document, RawAsset archive)
      : root_(std::move(root)), sample_(std::move(sample)), document_(std::move(document)), archive_(std::move(archive)) {}
  Task<Error> Load();
  Task<Error> Commit(Snapshot next);
  Task<Error> Mutate(std::function<Error(Snapshot&)> change);
  Task<Error> Import(FileReference source, Id parent, Conflict policy, Id version_of,
                     std::function<void(std::uint64_t, std::uint64_t)> progress);
  Task<Error> SaveText(Id id, std::string text);
  Task<Error> RestoreVersion(Id id, Id blob);
  Task<Error> CollectGarbage();
  const Snapshot& Data() const { return data_; }
  File Blob(Id id) const { return root_.Child("blobs").Child(std::to_string(id)); }
private:
  Task<Error> Seed();
  Task<Error> AddBytes(Snapshot& next, Id parent, std::string name, Bytes bytes);
  File root_;
  Snapshot data_;
  RawAsset sample_;
  RawAsset document_;
  RawAsset archive_;
};

} // namespace huxer_drive
