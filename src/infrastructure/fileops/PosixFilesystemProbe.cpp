#include "infrastructure/fileops/PosixFilesystemProbe.h"

bool PosixFilesystemProbe::EntryExistsWithoutFollowingLinks(const std::filesystem::path&) const
{
    return false;
}

bool PosixFilesystemProbe::TargetDirectoryExists(const std::filesystem::path&) const
{
    return false;
}

bool PosixFilesystemProbe::IsReparsePoint(const std::filesystem::path&) const
{
    return false;
}

bool PosixFilesystemProbe::PhysicalDirectoryExists(const std::filesystem::path&) const
{
    return false;
}

std::vector<std::filesystem::path> PosixFilesystemProbe::ChildDirectories(const std::filesystem::path&) const
{
    return {};
}

std::vector<std::filesystem::path> PosixFilesystemProbe::ChildFiles(const std::filesystem::path&) const
{
    return {};
}

bool PosixFilesystemProbe::VolumeIsAvailable(const std::filesystem::path&) const
{
    return false;
}

WriteAccess PosixFilesystemProbe::ProbeWritable(const std::filesystem::path&) const
{
    return WriteAccess::ItRefusedForAnotherReason;
}

bool PosixFilesystemProbe::SomethingIsHoldingItOpen(const std::filesystem::path&) const
{
    return true;
}

std::optional<std::uintmax_t> PosixFilesystemProbe::FreeSpaceOn(const std::filesystem::path&) const
{
    return std::nullopt;
}

std::optional<RecycleBinRoom> PosixFilesystemProbe::RecycleBinOn(const std::filesystem::path&) const
{
    return std::nullopt;
}

std::optional<std::string> PosixFilesystemProbe::ContentsOf(const std::filesystem::path&) const
{
    return std::nullopt;
}

std::optional<std::string> PosixFilesystemProbe::FirstBytesOf(const std::filesystem::path&, std::size_t) const
{
    return std::nullopt;
}

std::vector<std::optional<std::string>>
PosixFilesystemProbe::HashesOf(const std::filesystem::path&, const std::vector<std::filesystem::path>& below) const
{
    return std::vector<std::optional<std::string>>(below.size());
}

std::optional<TreeFingerprint> PosixFilesystemProbe::FingerprintTree(const std::filesystem::path&) const
{
    return std::nullopt;
}

std::optional<std::chrono::system_clock::time_point>
PosixFilesystemProbe::LastWriteTime(const std::filesystem::path&) const
{
    return std::nullopt;
}
