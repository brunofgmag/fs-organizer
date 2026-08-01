#ifndef FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PROPOSAL_H
#define FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PROPOSAL_H

#include <filesystem>
#include <vector>

#include "domain/legacy/LegacyInstallation.h"
#include "domain/legacy/ProposedState.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

struct ProposedCategory
{
    std::filesystem::path relativePath;
    ProposedState state = ProposedState::New;
};

struct ProposedLibrary
{
    std::filesystem::path root;
    ProposedState state = ProposedState::New;
    std::vector<ProposedCategory> categories;
    std::vector<std::filesystem::path> refused;
};

[[nodiscard]] std::vector<ProposedLibrary> ProposeLibraries(const LegacyInstallation& installation,
                                                            const SimulatorProfile& current,
                                                            const std::vector<TreeNode>& scanned);

#endif // FS_ORGANIZER_DOMAIN_LEGACY_LEGACY_PROPOSAL_H
