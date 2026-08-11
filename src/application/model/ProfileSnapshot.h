#ifndef FS_ORGANIZER_APPLICATION_MODEL_PROFILE_SNAPSHOT_H
#define FS_ORGANIZER_APPLICATION_MODEL_PROFILE_SNAPSHOT_H

#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/importing/CopyConflicts.h"
#include "domain/model/DestinationEntry.h"
#include "domain/model/EnabledAddons.h"
#include "domain/model/TreeNode.h"

struct ProfileSnapshot
{
    std::vector<TreeNode> libraries{};
    std::vector<DestinationEntry> entries{};
    EnabledAddons enabled{};
    CopyConflicts conflicts{};
    std::vector<StartupEntry> startupEntries{};
    bool complete = true;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_PROFILE_SNAPSHOT_H
