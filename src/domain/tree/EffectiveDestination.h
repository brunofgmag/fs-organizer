#ifndef FS_ORGANIZER_DOMAIN_TREE_EFFECTIVE_DESTINATION_H
#define FS_ORGANIZER_DOMAIN_TREE_EFFECTIVE_DESTINATION_H

#include <filesystem>

#include "domain/model/LibraryId.h"
#include "domain/model/SimulatorProfile.h"

[[nodiscard]] std::filesystem::path EffectiveDestination(const SimulatorProfile& profile,
                                                         const LibraryId& libraryId,
                                                         const std::filesystem::path& relativePath);

[[nodiscard]] std::filesystem::path EffectiveDestination(const SimulatorProfile& profile,
                                                         const std::filesystem::path& addonFolder);

[[nodiscard]] std::filesystem::path PlannedLinkPath(const SimulatorProfile& profile,
                                                    const std::filesystem::path& addonFolder);

#endif // FS_ORGANIZER_DOMAIN_TREE_EFFECTIVE_DESTINATION_H
