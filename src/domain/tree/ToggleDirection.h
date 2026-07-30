#ifndef FS_ORGANIZER_DOMAIN_TREE_TOGGLE_DIRECTION_H
#define FS_ORGANIZER_DOMAIN_TREE_TOGGLE_DIRECTION_H

#include <filesystem>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/model/EnabledAddons.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

[[nodiscard]] bool DestinationBlocks(const SimulatorProfile& profile,
                                     const std::vector<DestinationEntry>& entries,
                                     const std::filesystem::path& addonFolder);

[[nodiscard]] bool ShouldEnable(const SimulatorProfile& profile,
                                const std::vector<DestinationEntry>& entries,
                                const EnabledAddons& enabled,
                                const std::vector<const TreeNode*>& nodes);

#endif // FS_ORGANIZER_DOMAIN_TREE_TOGGLE_DIRECTION_H
