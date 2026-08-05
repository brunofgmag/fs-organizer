#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_EXTENDED_PATHS_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_EXTENDED_PATHS_H

#include <filesystem>

[[nodiscard]] std::filesystem::path WithExtendedPrefix(const std::filesystem::path& path);

[[nodiscard]] std::filesystem::path WithoutExtendedPrefix(const std::filesystem::path& path);

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_EXTENDED_PATHS_H
