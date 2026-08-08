#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_EXTENDED_PATHS_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_EXTENDED_PATHS_H

#include <cstddef>
#include <filesystem>
#include <optional>

[[nodiscard]] std::filesystem::path WithExtendedPrefix(const std::filesystem::path& path);

[[nodiscard]] std::filesystem::path WithoutExtendedPrefix(const std::filesystem::path& path);

[[nodiscard]] std::optional<std::size_t> LongestEntryUnder(const std::filesystem::path& root);

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_EXTENDED_PATHS_H
