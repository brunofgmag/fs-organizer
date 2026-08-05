#include "infrastructure/link/WindowsLinkService.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winioctl.h>

#include <cstring>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/fileops/ExtendedPaths.h"

namespace
{
    constexpr DWORD kReparseHeaderBytes = 8;
    constexpr DWORD kMountPointFieldBytes = 8;
    constexpr std::wstring_view kSubstitutePrefix = LR"(\??\)";

    struct ReparseDataBuffer
    {
        ULONG ReparseTag;
        USHORT ReparseDataLength;
        USHORT Reserved;

        union
        {
            struct
            {
                USHORT SubstituteNameOffset;
                USHORT SubstituteNameLength;
                USHORT PrintNameOffset;
                USHORT PrintNameLength;
                ULONG Flags;
                WCHAR PathBuffer[1];
            } SymbolicLink;

            struct
            {
                USHORT SubstituteNameOffset;
                USHORT SubstituteNameLength;
                USHORT PrintNameOffset;
                USHORT PrintNameLength;
                WCHAR PathBuffer[1];
            } MountPoint;
        };
    };

    std::wstring NativeLinkPath(const std::filesystem::path& path)
    {
        return WithExtendedPrefix(path).wstring();
    }

    std::wstring NativeTarget(const std::filesystem::path& path)
    {
        std::filesystem::path native = path.lexically_normal();
        native.make_preferred();
        return native.wstring();
    }

    bool WriteMountPoint(HANDLE handle, const std::wstring& target)
    {
        const std::wstring substituteName = std::wstring(kSubstitutePrefix) + target;

        const std::size_t substituteBytes = substituteName.size() * sizeof(WCHAR);
        const std::size_t printBytes = target.size() * sizeof(WCHAR);
        const std::size_t pathBytes = substituteBytes + printBytes + 2 * sizeof(WCHAR);

        std::vector<char> raw(kReparseHeaderBytes + kMountPointFieldBytes + pathBytes, 0);
        auto* buffer = reinterpret_cast<ReparseDataBuffer*>(raw.data());

        buffer->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
        buffer->ReparseDataLength = static_cast<USHORT>(kMountPointFieldBytes + pathBytes);
        buffer->MountPoint.SubstituteNameOffset = 0;
        buffer->MountPoint.SubstituteNameLength = static_cast<USHORT>(substituteBytes);
        buffer->MountPoint.PrintNameOffset = static_cast<USHORT>(substituteBytes + sizeof(WCHAR));
        buffer->MountPoint.PrintNameLength = static_cast<USHORT>(printBytes);

        auto* pathBuffer = reinterpret_cast<char*>(buffer->MountPoint.PathBuffer);
        std::memcpy(pathBuffer, substituteName.c_str(), substituteBytes + sizeof(WCHAR));
        std::memcpy(pathBuffer + substituteBytes + sizeof(WCHAR), target.c_str(), printBytes + sizeof(WCHAR));

        DWORD returned = 0;
        return DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, raw.data(), static_cast<DWORD>(raw.size()), nullptr, 0,
                               &returned, nullptr)
            != FALSE;
    }

    bool CreateJunction(const std::wstring& linkPath, const std::wstring& target)
    {
        if (CreateDirectoryW(linkPath.c_str(), nullptr) == FALSE)
        {
            return false;
        }

        const HANDLE handle = CreateFileW(linkPath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            RemoveDirectoryW(linkPath.c_str());
            return false;
        }

        const bool written = WriteMountPoint(handle, target);
        CloseHandle(handle);

        if (!written)
        {
            RemoveDirectoryW(linkPath.c_str());
        }

        return written;
    }

    LinkFailure CreateSymbolicDirectoryLink(const std::wstring& linkPath, const std::wstring& target)
    {
        constexpr DWORD flags = SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

        if (CreateSymbolicLinkW(linkPath.c_str(), target.c_str(), flags) != FALSE)
        {
            return LinkFailure::None;
        }

        return GetLastError() == ERROR_PRIVILEGE_NOT_HELD ? LinkFailure::PrivilegeNotHeld
                                                          : LinkFailure::CouldNotCreateLink;
    }
}

LinkFailure WindowsLinkService::CreateLink(const std::filesystem::path& linkPath,
                                           const std::filesystem::path& target,
                                           LinkType linkType)
{
    const std::wstring nativeLink = NativeLinkPath(linkPath);
    const std::wstring nativeTarget = NativeTarget(target);

    if (linkType == LinkType::Symbolic)
    {
        return CreateSymbolicDirectoryLink(nativeLink, nativeTarget);
    }

    return CreateJunction(nativeLink, nativeTarget) ? LinkFailure::None : LinkFailure::CouldNotCreateLink;
}

bool WindowsLinkService::RemoveReparseNode(const std::filesystem::path& linkPath)
{
    return RemoveDirectoryW(NativeLinkPath(linkPath).c_str()) != FALSE;
}

std::optional<std::filesystem::path> WindowsLinkService::ReadLinkTarget(const std::filesystem::path& path) const
{
    const HANDLE handle = CreateFileW(NativeLinkPath(path).c_str(), FILE_READ_ATTRIBUTES,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }

    std::vector<char> raw(MAXIMUM_REPARSE_DATA_BUFFER_SIZE, 0);
    DWORD returned = 0;
    const BOOL queried = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, raw.data(),
                                         static_cast<DWORD>(raw.size()), &returned, nullptr);
    CloseHandle(handle);

    if (queried == FALSE)
    {
        return std::nullopt;
    }

    const auto* buffer = reinterpret_cast<const ReparseDataBuffer*>(raw.data());
    const char* pathBuffer = nullptr;
    USHORT nameOffset = 0;
    USHORT nameLength = 0;

    if (buffer->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
    {
        pathBuffer = reinterpret_cast<const char*>(buffer->MountPoint.PathBuffer);
        nameOffset = buffer->MountPoint.SubstituteNameOffset;
        nameLength = buffer->MountPoint.SubstituteNameLength;
    }
    else if (buffer->ReparseTag == IO_REPARSE_TAG_SYMLINK)
    {
        pathBuffer = reinterpret_cast<const char*>(buffer->SymbolicLink.PathBuffer);
        nameOffset = buffer->SymbolicLink.SubstituteNameOffset;
        nameLength = buffer->SymbolicLink.SubstituteNameLength;
    }
    else
    {
        return std::nullopt;
    }

    const std::wstring target(reinterpret_cast<const wchar_t*>(pathBuffer + nameOffset), nameLength / sizeof(WCHAR));

    return NormalizeReparseTarget(std::filesystem::path(target));
}
