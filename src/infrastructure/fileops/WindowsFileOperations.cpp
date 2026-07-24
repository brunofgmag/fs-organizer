#include "infrastructure/fileops/WindowsFileOperations.h"

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

bool WindowsFileOperations::EntryExistsWithoutFollowingLinks(const std::filesystem::path& path) const
{
    return AttributesWithoutFollowingLinks(path) != INVALID_FILE_ATTRIBUTES;
}

bool WindowsFileOperations::TargetDirectoryExists(const std::filesystem::path& path) const
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

std::vector<std::filesystem::path> WindowsFileOperations::ChildDirectories(
    const std::filesystem::path& path) const
{
    std::vector<std::filesystem::path> children;

    std::error_code error;
    const std::filesystem::directory_iterator entries(
        path, std::filesystem::directory_options::skip_permission_denied, error);

    for (const std::filesystem::directory_entry& entry: entries)
    {
        const DWORD attributes = AttributesWithoutFollowingLinks(entry.path());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            children.push_back(entry.path());
        }
    }

    return children;
}

bool WindowsFileOperations::VolumeIsAvailable(const std::filesystem::path& path) const
{
    const std::filesystem::path root = path.root_path();
    if (root.empty())
    {
        return false;
    }

    return GetDriveTypeW(NativePath(root).c_str()) != DRIVE_NO_ROOT_DIR;
}
