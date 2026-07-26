#include "infrastructure/fileops/WindowsFilesystemProbe.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <system_error>

namespace
{
    std::wstring NativePath(const std::filesystem::path& path)
    {
        std::filesystem::path native = path.lexically_normal();
        native.make_preferred();
        return native.wstring();
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
    return std::filesystem::is_directory(path, error);
}

std::vector<std::filesystem::path> WindowsFilesystemProbe::ChildDirectories(const std::filesystem::path& path) const
{
    std::vector<std::filesystem::path> children;

    std::error_code error;
    const std::filesystem::directory_iterator entries(path, std::filesystem::directory_options::skip_permission_denied,
                                                      error);

    for (const std::filesystem::directory_entry& entry : entries)
    {
        const DWORD attributes = AttributesWithoutFollowingLinks(entry.path());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            children.push_back(entry.path());
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
    const std::filesystem::file_time_type written = std::filesystem::last_write_time(path, error);
    if (error)
    {
        return std::nullopt;
    }

    return std::chrono::system_clock::now()
        + std::chrono::duration_cast<std::chrono::system_clock::duration>(
               written - std::filesystem::file_time_type::clock::now());
}

std::vector<FileFingerprint> WindowsFilesystemProbe::FingerprintTree(const std::filesystem::path& root) const
{
    std::vector<FileFingerprint> files;

    std::error_code error;
    const std::filesystem::recursive_directory_iterator entries(
        root, std::filesystem::directory_options::skip_permission_denied, error);

    for (const std::filesystem::directory_entry& entry : entries)
    {
        if (!entry.is_regular_file(error))
        {
            continue;
        }

        files.push_back(FileFingerprint{entry.path().lexically_relative(root), entry.file_size(error)});
    }

    return files;
}
