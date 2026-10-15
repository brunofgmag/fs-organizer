#include "infrastructure/link/PosixLinkService.h"

LinkFailure PosixLinkService::CreateLink(const std::filesystem::path&, const std::filesystem::path&, LinkType)
{
    return LinkFailure::CouldNotCreateLink;
}

bool PosixLinkService::RemoveReparseNode(const std::filesystem::path&)
{
    return false;
}

std::optional<std::filesystem::path> PosixLinkService::ReadLinkTarget(const std::filesystem::path&) const
{
    return std::nullopt;
}
