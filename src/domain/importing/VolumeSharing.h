#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_VOLUME_SHARING_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_VOLUME_SHARING_H

#include <filesystem>
#include <optional>
#include <string>

#include "domain/ports/FilesystemProbe.h"

[[nodiscard]] inline bool
ShareAVolume(const FilesystemProbe& probe, const std::filesystem::path& left, const std::filesystem::path& right)
{
    const std::optional<std::string> leftVolume = probe.VolumeIdentityOf(left);

    return leftVolume.has_value() && leftVolume == probe.VolumeIdentityOf(right);
}

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_VOLUME_SHARING_H
