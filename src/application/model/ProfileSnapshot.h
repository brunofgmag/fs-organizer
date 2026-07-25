#ifndef FS_ORGANIZER_APPLICATION_MODEL_PROFILE_SNAPSHOT_H
#define FS_ORGANIZER_APPLICATION_MODEL_PROFILE_SNAPSHOT_H

#include <vector>

#include "domain/model/DestinationEntry.h"
#include "domain/model/EnabledAddons.h"
#include "domain/model/TreeNode.h"

struct ProfileSnapshot
{
    std::vector<TreeNode> libraries;
    std::vector<DestinationEntry> entries;
    EnabledAddons enabled;
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_PROFILE_SNAPSHOT_H
