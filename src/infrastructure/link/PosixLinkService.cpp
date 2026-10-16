#include "infrastructure/link/PosixLinkService.h"

#include <system_error>

LinkFailure
PosixLinkService::CreateLink(const std::filesystem::path& linkPath, const std::filesystem::path& target, LinkType)
{
    std::error_code error;
    std::filesystem::create_directory_symlink(target.lexically_normal(), linkPath, error);

    if (!error)
    {
        return LinkFailure::None;
    }

    return error == std::errc::permission_denied || error == std::errc::operation_not_permitted
        ? LinkFailure::PrivilegeNotHeld
        : LinkFailure::CouldNotCreateLink;
}

bool PosixLinkService::RemoveReparseNode(const std::filesystem::path& linkPath)
{
    std::error_code error;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(linkPath, error)) || error)
    {
        return false;
    }

    return std::filesystem::remove(linkPath, error) && !error;
}

std::optional<std::filesystem::path> PosixLinkService::ReadLinkTarget(const std::filesystem::path& path) const
{
    std::error_code error;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(path, error)) || error)
    {
        return std::nullopt;
    }

    const std::filesystem::path target = std::filesystem::read_symlink(path, error);
    if (error)
    {
        return std::nullopt;
    }

    return target.lexically_normal();
}
