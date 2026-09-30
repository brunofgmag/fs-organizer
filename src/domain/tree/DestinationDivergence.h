#ifndef FS_ORGANIZER_DOMAIN_TREE_DESTINATION_DIVERGENCE_H
#define FS_ORGANIZER_DOMAIN_TREE_DESTINATION_DIVERGENCE_H

#include <filesystem>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

struct DestinationAgreement
{
    std::filesystem::path destination;
    bool unanimous = false;

    [[nodiscard]] bool Adoptable() const
    {
        return unanimous && !destination.empty();
    }
};

[[nodiscard]] std::filesystem::path DestinationItStrayedTo(const SimulatorProfile& profile,
                                                           const std::vector<DestinationEntry>& entries,
                                                           const std::filesystem::path& addonFolder);

[[nodiscard]] DestinationAgreement WhereTheEnabledAddonsPoint(const TreeNode& category,
                                                              const std::vector<DestinationEntry>& entries);

#endif // FS_ORGANIZER_DOMAIN_TREE_DESTINATION_DIVERGENCE_H
