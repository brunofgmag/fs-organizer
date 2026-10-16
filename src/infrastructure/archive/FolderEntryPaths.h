#ifndef FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_FOLDER_ENTRY_PATHS_H
#define FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_FOLDER_ENTRY_PATHS_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "domain/model/ArchiveEntry.h"

[[nodiscard]] std::optional<std::filesystem::path> BelowTheFolder(const std::string& entry, const std::string& name);

[[nodiscard]] std::optional<std::vector<std::filesystem::path>> RelativePathsOf(const std::vector<ArchiveEntry>& listed,
                                                                                const std::string& name);

[[nodiscard]] std::uintmax_t BytesOfTheFiles(const std::vector<ArchiveEntry>& listed);

#endif // FS_ORGANIZER_INFRASTRUCTURE_ARCHIVE_FOLDER_ENTRY_PATHS_H
