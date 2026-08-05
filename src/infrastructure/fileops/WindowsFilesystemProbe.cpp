#include "infrastructure/fileops/WindowsFilesystemProbe.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "infrastructure/fileops/ExtendedPaths.h"
#include "support/FileClock.h"

namespace
{
    std::wstring NativePath(const std::filesystem::path& path)
    {
        return WithExtendedPrefix(path).wstring();
    }

    DWORD AttributesWithoutFollowingLinks(const std::filesystem::path& path)
    {
        return GetFileAttributesW(NativePath(path).c_str());
    }
}

bool WindowsFilesystemProbe::EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const
{
    return AttributesWithoutFollowingLinks(path) != INVALID_FILE_ATTRIBUTES;
}

bool WindowsFilesystemProbe::IsReparsePoint(const std::filesystem::path& path) const
{
    const DWORD attributes = AttributesWithoutFollowingLinks(path);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool WindowsFilesystemProbe::TargetDirectoryExists(const std::filesystem::path& path) const
{
    std::error_code error;
    return std::filesystem::is_directory(WithExtendedPrefix(path), error);
}

std::vector<std::filesystem::path> WindowsFilesystemProbe::ChildDirectories(const std::filesystem::path& path) const
{
    std::error_code error;
    std::filesystem::directory_iterator entry(WithExtendedPrefix(path),
                                              std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return {};
    }

    std::vector<std::filesystem::path> children;
    const std::filesystem::directory_iterator end;

    while (entry != end)
    {
        const DWORD attributes = AttributesWithoutFollowingLinks(entry->path());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            children.push_back(path / entry->path().filename());
        }

        entry.increment(error);
        if (error)
        {
            return {};
        }
    }

    return children;
}

bool WindowsFilesystemProbe::VolumeIsAvailable(const std::filesystem::path& path) const
{
    const std::filesystem::path root = path.root_path();
    if (root.empty())
    {
        return false;
    }

    return GetDriveTypeW(NativePath(root).c_str()) != DRIVE_NO_ROOT_DIR;
}

bool WindowsFilesystemProbe::ProbeWritable(const std::filesystem::path& path) const
{
    if (!TargetDirectoryExists(path))
    {
        return false;
    }

    const HANDLE probe = CreateFileW(NativePath(path / ".fsorg-write-probe").c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (probe == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    CloseHandle(probe);

    return true;
}

std::optional<std::uintmax_t> WindowsFilesystemProbe::FreeSpaceOn(const std::filesystem::path& path) const
{
    ULARGE_INTEGER available{};
    if (GetDiskFreeSpaceExW(NativePath(path).c_str(), &available, nullptr, nullptr) == 0)
    {
        return std::nullopt;
    }

    return available.QuadPart;
}

std::optional<std::chrono::system_clock::time_point>
WindowsFilesystemProbe::LastWriteTime(const std::filesystem::path& path) const
{
    std::error_code error;
    const std::filesystem::file_time_type written = std::filesystem::last_write_time(WithExtendedPrefix(path), error);
    if (error)
    {
        return std::nullopt;
    }

    return SystemTimeOf(written);
}

std::optional<std::string> WindowsFilesystemProbe::ContentsOf(const std::filesystem::path& path) const
{
    std::ifstream file(WithExtendedPrefix(path), std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
}

std::optional<std::vector<FileFingerprint>>
WindowsFilesystemProbe::FingerprintTree(const std::filesystem::path& root) const
{
    const std::filesystem::path reachableRoot = WithExtendedPrefix(root);

    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(
        reachableRoot, std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return std::nullopt;
    }

    std::vector<FileFingerprint> files;
    const std::filesystem::recursive_directory_iterator end;

    while (entry != end)
    {
        const bool isFile = entry->is_regular_file(error);
        if (error)
        {
            return std::nullopt;
        }

        if (isFile)
        {
            const std::uintmax_t size = entry->file_size(error);
            if (error)
            {
                return std::nullopt;
            }

            files.push_back(
                FileFingerprint{.relativePath = entry->path().lexically_relative(reachableRoot), .size = size});
        }

        entry.increment(error);
        if (error)
        {
            return std::nullopt;
        }
    }

    return files;
}
