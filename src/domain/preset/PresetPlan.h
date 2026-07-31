#ifndef FS_ORGANIZER_DOMAIN_PRESET_PRESET_PLAN_H
#define FS_ORGANIZER_DOMAIN_PRESET_PRESET_PLAN_H

#include <cstddef>
#include <vector>

#include "domain/model/AddonId.h"
#include "domain/model/EnabledAddons.h"
#include "domain/model/Preset.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

enum class ApplyMode : int
{
    Replace = 0,
    Cumulative = 1,
    Disable = 2,
};

struct PresetPlan
{
    std::vector<const TreeNode*> toDisable;
    std::vector<const TreeNode*> toEnable;
    std::vector<AddonId> unresolved;
    std::vector<const TreeNode*> alreadyInPlace;
};

[[nodiscard]] PresetPlan PlanPresetApplication(const Preset& preset,
                                               ApplyMode mode,
                                               const SimulatorProfile& profile,
                                               const std::vector<TreeNode>& libraries,
                                               const EnabledAddons& enabled);

PresetPlan PlanPresetApplication(const Preset& preset,
                                 ApplyMode mode,
                                 const SimulatorProfile& profile,
                                 std::vector<TreeNode>&& libraries,
                                 const EnabledAddons& enabled) = delete;

struct PresetContent
{
    std::size_t addons = 0;
    std::size_t categories = 0;
};

[[nodiscard]] PresetContent
ContentOf(const Preset& preset, const SimulatorProfile& profile, const std::vector<TreeNode>& libraries);

PresetContent
ContentOf(const Preset& preset, const SimulatorProfile& profile, std::vector<TreeNode>&& libraries) = delete;

[[nodiscard]] std::vector<PresetEntry> EntriesForWhatIsEnabled(const SimulatorProfile& profile,
                                                               const std::vector<TreeNode>& libraries,
                                                               const EnabledAddons& enabled);

#endif // FS_ORGANIZER_DOMAIN_PRESET_PRESET_PLAN_H
