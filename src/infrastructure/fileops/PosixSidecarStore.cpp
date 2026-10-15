#include "infrastructure/fileops/PosixSidecarStore.h"

bool PosixSidecarStore::Write(const std::filesystem::path&, const std::string&)
{
    return false;
}

std::optional<std::string> PosixSidecarStore::Read(const std::filesystem::path&) const
{
    return std::nullopt;
}

bool PosixSidecarStore::Forget(const std::filesystem::path&)
{
    return false;
}

std::vector<std::filesystem::path> PosixSidecarStore::FilesIn(const std::filesystem::path&) const
{
    return {};
}
