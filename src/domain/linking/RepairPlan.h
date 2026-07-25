#ifndef FS_ORGANIZER_DOMAIN_LINKING_REPAIR_PLAN_H
#define FS_ORGANIZER_DOMAIN_LINKING_REPAIR_PLAN_H

#include <filesystem>
#include <optional>
#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

struct RepairCandidate
{
    DestinationEntry entry;
    bool targetsLibrary = false;
    std::optional<std::filesystem::path> repointTo;
};

enum class RepairAction : int
{
    RemoveDeadNode = 0,
    Repoint = 1,
};

struct RepairRequest
{
    RepairCandidate candidate;
    RepairAction action = RepairAction::RemoveDeadNode;
};

[[nodiscard]] std::vector<RepairCandidate> PlanRepairs(const SimulatorProfile& profile,
                                                       const std::vector<DestinationEntry>& entries,
                                                       const std::vector<TreeNode>& libraries);

#endif // FS_ORGANIZER_DOMAIN_LINKING_REPAIR_PLAN_H
