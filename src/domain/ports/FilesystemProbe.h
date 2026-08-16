#ifndef FS_ORGANIZER_DOMAIN_PORTS_FILESYSTEM_PROBE_H
#define FS_ORGANIZER_DOMAIN_PORTS_FILESYSTEM_PROBE_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "domain/model/FileFingerprint.h"
#include "domain/model/RecycleBinRoom.h"
#include "domain/model/WriteAccess.h"

class FilesystemProbe
{
public:
    virtual ~FilesystemProbe() = default;

    [[nodiscard]] virtual bool EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool TargetDirectoryExists(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool IsReparsePoint(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool PhysicalDirectoryExists(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::vector<std::filesystem::path>
    ChildDirectories(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::vector<std::filesystem::path> ChildFiles(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool VolumeIsAvailable(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual WriteAccess ProbeWritable(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool SomethingIsHoldingItOpen(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::optional<std::uintmax_t> FreeSpaceOn(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::optional<RecycleBinRoom> RecycleBinOn(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::optional<std::string> ContentsOf(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual std::optional<std::string> FirstBytesOf(const std::filesystem::path& path,
                                                                  std::size_t most) const = 0;

    [[nodiscard]] virtual std::vector<std::optional<std::string>>
    HashesOf(const std::filesystem::path& root, const std::vector<std::filesystem::path>& below) const = 0;

    [[nodiscard]] virtual std::optional<TreeFingerprint> FingerprintTree(const std::filesystem::path& root) const = 0;

    [[nodiscard]] virtual std::optional<std::chrono::system_clock::time_point>
    LastWriteTime(const std::filesystem::path& path) const = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_FILESYSTEM_PROBE_H
