#include "infrastructure/fileops/WindowsFilesystemProbe.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <execution>
#include <fstream>
#include <iterator>
#include <numeric>
#include <string>
#include <system_error>

#include <QtCore/QByteArrayView>
#include <QtCore/QCryptographicHash>

#include "domain/support/PathUtils.h"
#include "infrastructure/fileops/ExtendedPaths.h"
#include "support/FileClock.h"

namespace
{
    constexpr std::size_t kBytesReadAtATime = 1024 * 1024;

    std::wstring NativePath(const std::filesystem::path& path)
    {
        return WithExtendedPrefix(path).wstring();
    }

    DWORD AttributesWithoutFollowingLinks(const std::filesystem::path& path)
    {
        return GetFileAttributesW(NativePath(path).c_str());
    }

    WriteAccess WhatTheProbeRanInto(const DWORD error)
    {
        switch (error)
        {
        case ERROR_ACCESS_DENIED:
        case ERROR_NETWORK_ACCESS_DENIED: return WriteAccess::PermissionIsDenied;
        case ERROR_WRITE_PROTECT: return WriteAccess::TheVolumeIsReadOnly;
        default: return WriteAccess::ItRefusedForAnotherReason;
        }
    }

    bool NobodyElseWouldLetItGo(const std::filesystem::path& path)
    {
        const HANDLE asked = CreateFileW(NativePath(path).c_str(), DELETE, 0, nullptr, OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (asked != INVALID_HANDLE_VALUE)
        {
            CloseHandle(asked);

            return false;
        }

        return GetLastError() == ERROR_SHARING_VIOLATION;
    }

    template<typename Wanted>
    std::vector<std::filesystem::path> ChildrenWhoseAttributesPass(const std::filesystem::path& path,
                                                                   const Wanted& wanted)
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
            if (const DWORD attributes = AttributesWithoutFollowingLinks(entry->path());
                attributes != INVALID_FILE_ATTRIBUTES && wanted(attributes))
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

    std::optional<std::wstring> VolumeIdentifierOf(const std::filesystem::path& path)
    {
        std::filesystem::path root = WithoutExtendedPrefix(path).root_path();
        if (root.empty())
        {
            return std::nullopt;
        }

        std::wstring mountPoint = root.wstring();
        if (mountPoint.back() != L'\\')
        {
            mountPoint.push_back(L'\\');
        }

        std::wstring name(64, L'\0');
        if (GetVolumeNameForVolumeMountPointW(mountPoint.c_str(), name.data(), static_cast<DWORD>(name.size())) == 0)
        {
            return std::nullopt;
        }

        name.resize(std::wcslen(name.c_str()));

        const std::size_t opening = name.find(L'{');
        const std::size_t closing = name.find(L'}');
        if (opening == std::wstring::npos || closing == std::wstring::npos || closing < opening)
        {
            return std::nullopt;
        }

        return name.substr(opening, closing - opening + 1);
    }

    std::optional<DWORD> BitBucketValue(const std::wstring& volume, const wchar_t* name)
    {
        const std::wstring key = LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\BitBucket\Volume\)" + volume;

        DWORD value = 0;
        DWORD size = sizeof(value);

        if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_DWORD, nullptr, &value, &size)
            != ERROR_SUCCESS)
        {
            return std::nullopt;
        }

        return value;
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

bool WindowsFilesystemProbe::PhysicalDirectoryExists(const std::filesystem::path& path) const
{
    const DWORD attributes = AttributesWithoutFollowingLinks(path);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool WindowsFilesystemProbe::TargetDirectoryExists(const std::filesystem::path& path) const
{
    std::error_code error;
    return std::filesystem::is_directory(WithExtendedPrefix(path), error);
}

std::vector<std::filesystem::path> WindowsFilesystemProbe::ChildDirectories(const std::filesystem::path& path) const
{
    return ChildrenWhoseAttributesPass(path,
                                       [](const DWORD attributes)
                                       {
                                           return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                                       });
}

std::vector<std::filesystem::path> WindowsFilesystemProbe::ChildFiles(const std::filesystem::path& path) const
{
    return ChildrenWhoseAttributesPass(path,
                                       [](const DWORD attributes)
                                       {
                                           return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
                                       });
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

WriteAccess WindowsFilesystemProbe::ProbeWritable(const std::filesystem::path& path) const
{
    if (!TargetDirectoryExists(path))
    {
        return WriteAccess::TheFolderIsNotThere;
    }

    const HANDLE probe = CreateFileW(NativePath(path / ".fsorg-write-probe").c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (probe == INVALID_HANDLE_VALUE)
    {
        return WhatTheProbeRanInto(GetLastError());
    }

    CloseHandle(probe);

    return WriteAccess::ItAccepts;
}

bool WindowsFilesystemProbe::SomethingIsHoldingItOpen(const std::filesystem::path& path) const
{
    if (NobodyElseWouldLetItGo(path))
    {
        return true;
    }

    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(
        WithExtendedPrefix(path), std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return false;
    }

    const std::filesystem::recursive_directory_iterator end;

    while (entry != end)
    {
        if (NobodyElseWouldLetItGo(entry->path()))
        {
            return true;
        }

        entry.increment(error);
        if (error)
        {
            return false;
        }
    }

    return false;
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

std::optional<RecycleBinRoom> WindowsFilesystemProbe::RecycleBinOn(const std::filesystem::path& path) const
{
    const std::optional<std::wstring> volume = VolumeIdentifierOf(path);
    if (!volume.has_value())
    {
        return std::nullopt;
    }

    const std::optional<DWORD> megabytes = BitBucketValue(*volume, L"MaxCapacity");
    if (!megabytes.has_value())
    {
        return std::nullopt;
    }

    const std::optional<DWORD> nuke = BitBucketValue(*volume, L"NukeOnDelete");

    return RecycleBinRoom{.quota = static_cast<std::uintmax_t>(*megabytes) * 1024 * 1024,
                          .itRecycles = !nuke.has_value() || *nuke == 0};
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

std::optional<std::string> WindowsFilesystemProbe::FirstBytesOf(const std::filesystem::path& path,
                                                                const std::size_t most) const
{
    std::ifstream file(WithExtendedPrefix(path), std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    std::string bytes(most, '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(most));
    bytes.resize(static_cast<std::size_t>(file.gcount()));

    return bytes;
}

std::optional<std::string> WindowsFilesystemProbe::HashOf(const std::filesystem::path& path) const
{
    std::ifstream file(WithExtendedPrefix(path), std::ios::binary);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    QCryptographicHash digest(QCryptographicHash::Sha256);

    std::string block(kBytesReadAtATime, '\0');
    while (file.read(block.data(), static_cast<std::streamsize>(block.size())) || file.gcount() > 0)
    {
        digest.addData(QByteArrayView(block.data(), static_cast<qsizetype>(file.gcount())));
    }

    return file.bad() ? std::nullopt : std::optional(digest.result().toHex().toStdString());
}

std::vector<std::optional<std::string>>
WindowsFilesystemProbe::HashesOf(const std::filesystem::path& root,
                                 const std::vector<std::filesystem::path>& below) const
{
    std::vector<std::optional<std::string>> digests(below.size());

    std::vector<std::size_t> places(below.size());
    std::iota(places.begin(), places.end(), std::size_t{0});

    std::for_each(std::execution::par, places.begin(), places.end(),
                  [this, &root, &below, &digests](const std::size_t place)
                  {
                      digests[place] = HashOf(PathUnder(root, below[place]));
                  });

    return digests;
}

std::optional<TreeFingerprint> WindowsFilesystemProbe::FingerprintTree(const std::filesystem::path& root) const
{
    const std::filesystem::path reachableRoot = WithExtendedPrefix(root);

    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(
        reachableRoot, std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return std::nullopt;
    }

    TreeFingerprint walked{.longestEntry = WithoutExtendedPrefix(root).wstring().size()};
    const std::filesystem::recursive_directory_iterator end;

    while (entry != end)
    {
        const bool isFile = entry->is_regular_file(error);
        if (error)
        {
            return std::nullopt;
        }

        if (const std::size_t here = WithoutExtendedPrefix(entry->path()).wstring().size(); here > walked.longestEntry)
        {
            walked.longestEntry = here;
        }

        if (isFile)
        {
            const std::uintmax_t size = entry->file_size(error);
            if (error)
            {
                return std::nullopt;
            }

            walked.files.push_back(
                FileFingerprint{.relativePath = entry->path().lexically_relative(reachableRoot), .size = size});
        }

        entry.increment(error);
        if (error)
        {
            return std::nullopt;
        }
    }

    return walked;
}
