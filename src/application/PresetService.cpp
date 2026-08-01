#include "application/PresetService.h"

#include <algorithm>

PresetService::PresetService(PresetRepository& presets, ProfileService& profiles)
    : presets_(presets), profiles_(profiles)
{
}

std::vector<PresetListing> PresetService::List(const std::string& profileId) const
{
    return presets_.List(profileId);
}

std::optional<Preset> PresetService::Load(const std::string& profileId, const std::string& name) const
{
    return presets_.Load(profileId, name);
}

bool PresetService::Create(const SimulatorProfile& profile,
                           const ProfileSnapshot& snapshot,
                           const std::string& name) const
{
    Preset preset;
    preset.name = name;
    preset.entries = EntriesForWhatIsEnabled(profile, snapshot.libraries, snapshot.enabled);

    return presets_.Save(profile.id, preset);
}

bool PresetService::Store(const std::string& profileId, const Preset& preset) const
{
    return presets_.Save(profileId, preset);
}

bool PresetService::Update(const SimulatorProfile& profile,
                           const ProfileSnapshot& snapshot,
                           const std::string& name) const
{
    const std::vector<PresetEntry> enabled = EntriesForWhatIsEnabled(profile, snapshot.libraries, snapshot.enabled);

    Preset preset;
    preset.name = name;

    if (const std::optional<Preset> stored = presets_.Load(profile.id, name); stored.has_value())
    {
        for (const PresetEntry& entry : stored->entries)
        {
            const bool onAgain = std::ranges::any_of(enabled,
                                                     [&entry](const PresetEntry& candidate)
                                                     {
                                                         return candidate.addonId == entry.addonId;
                                                     });

            if (entry.action == PresetAction::Disable && !onAgain)
            {
                preset.entries.push_back(entry);
            }
        }
    }

    preset.entries.insert(preset.entries.end(), enabled.begin(), enabled.end());

    return presets_.Save(profile.id, preset);
}

bool PresetService::SetAction(const std::string& profileId,
                              const std::string& name,
                              const std::size_t index,
                              const AddonId& expected,
                              const PresetAction action) const
{
    std::optional<Preset> preset = presets_.Load(profileId, name);

    if (!preset.has_value() || index >= preset->entries.size() || preset->entries[index].addonId != expected)
    {
        return false;
    }

    preset->entries[index].action = action;

    return presets_.Save(profileId, *preset);
}

bool PresetService::Rename(const std::string& profileId, const std::string& from, const std::string& to) const
{
    return presets_.Rename(profileId, from, to);
}

void PresetService::Remove(const std::string& profileId, const std::string& name) const
{
    presets_.Remove(profileId, name);
}

PresetApplyReport PresetService::Apply(const SimulatorProfile& profile,
                                       const ProfileSnapshot& snapshot,
                                       const Preset& preset,
                                       const ApplyMode mode) const
{
    const PresetPlan plan = PlanPresetApplication(preset, mode, profile, snapshot.libraries, snapshot.enabled);

    return {profiles_.SetEnabled(profile, snapshot, LinkBatch{plan.toDisable, plan.toEnable}), plan.unresolved};
}
