#include "application/PresetService.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "domain/support/PathUtils.h"

namespace
{
    std::vector<StartupSwitch> SwitchesFor(const PresetStartupPlan& plan)
    {
        std::vector<StartupSwitch> switches;
        switches.reserve(plan.toTurnOff.size() + plan.toTurnOn.size());

        for (const StartupLine& line : plan.toTurnOff)
        {
            switches.push_back(StartupSwitch{.line = line, .enable = false});
        }

        for (const StartupLine& line : plan.toTurnOn)
        {
            switches.push_back(StartupSwitch{.line = line, .enable = true});
        }

        return switches;
    }
}

PresetService::PresetService(PresetRepository& presets, ProfileService& profiles, const StartupService& startup)
    : presets_(presets), profiles_(profiles), startup_(startup)
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

std::vector<PresetStartupEntry> PresetService::StartupEntriesForWhatIsOn(const SimulatorProfile& profile,
                                                                         const ProfileSnapshot& snapshot) const
{
    std::vector<PresetStartupEntry> entries;

    for (const StartupLine& line : startup_.Report(profile, snapshot).lines)
    {
        if (line.enabled)
        {
            entries.push_back(PresetStartupEntry{.path = line.path, .action = PresetAction::Enable});
        }
    }

    return entries;
}

bool PresetService::GovernStartup(const SimulatorProfile& profile,
                                  const ProfileSnapshot& snapshot,
                                  const std::string& name,
                                  const bool governs) const
{
    std::optional<Preset> preset = presets_.Load(profile.id, name);

    if (!preset.has_value())
    {
        return false;
    }

    if (governs && !preset->governsStartup)
    {
        preset->startupEntries = StartupEntriesForWhatIsOn(profile, snapshot);
    }

    preset->governsStartup = governs;

    return presets_.Save(profile.id, *preset);
}

bool PresetService::RecaptureStartup(const SimulatorProfile& profile,
                                     const ProfileSnapshot& snapshot,
                                     const std::string& name) const
{
    std::optional<Preset> preset = presets_.Load(profile.id, name);

    if (!preset.has_value())
    {
        return false;
    }

    preset->governsStartup = true;
    preset->startupEntries = StartupEntriesForWhatIsOn(profile, snapshot);

    return presets_.Save(profile.id, *preset);
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
        preset.governsStartup = stored->governsStartup;
        preset.startupEntries =
            stored->governsStartup ? StartupEntriesForWhatIsOn(profile, snapshot) : stored->startupEntries;

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

bool PresetService::SetStartupAction(const std::string& profileId,
                                     const std::string& name,
                                     const std::size_t index,
                                     const std::filesystem::path& expected,
                                     const PresetAction action) const
{
    std::optional<Preset> preset = presets_.Load(profileId, name);

    if (!preset.has_value() || index >= preset->startupEntries.size() || preset->startupEntries[index].path != expected)
    {
        return false;
    }

    preset->startupEntries[index].action = action;

    return presets_.Save(profileId, *preset);
}

bool PresetService::AddStartupEntry(const std::string& profileId,
                                    const std::string& name,
                                    const std::filesystem::path& entryPath) const
{
    std::optional<Preset> preset = presets_.Load(profileId, name);

    if (!preset.has_value() || entryPath.empty())
    {
        return false;
    }

    const std::string wanted = ComparablePath(entryPath);
    const bool alreadyNamed = std::ranges::any_of(preset->startupEntries,
                                                  [&wanted](const PresetStartupEntry& entry)
                                                  {
                                                      return ComparablePath(entry.path) == wanted;
                                                  });

    if (alreadyNamed)
    {
        return false;
    }

    preset->startupEntries.push_back(PresetStartupEntry{.path = entryPath, .action = PresetAction::Enable});

    return presets_.Save(profileId, *preset);
}

bool PresetService::TakeStartupEntryOut(const std::string& profileId,
                                        const std::string& name,
                                        const std::size_t index,
                                        const std::filesystem::path& expected) const
{
    std::optional<Preset> preset = presets_.Load(profileId, name);

    if (!preset.has_value() || index >= preset->startupEntries.size() || preset->startupEntries[index].path != expected)
    {
        return false;
    }

    preset->startupEntries.erase(preset->startupEntries.begin() + static_cast<std::ptrdiff_t>(index));

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

std::optional<Preset> PresetService::ReturnPreset(const std::string& profileId) const
{
    return presets_.LoadReturnPreset(profileId);
}

bool PresetService::IsSatisfied(const SimulatorProfile& profile,
                                const ProfileSnapshot& snapshot,
                                const Preset& preset) const
{
    return PresetIsSatisfied(preset, profile, snapshot.libraries, snapshot.enabled)
        && StartupIsInPlace(snapshot, preset);
}

bool PresetService::IsSatisfied(const ProfileSnapshot& snapshot,
                                const Preset& preset,
                                const PresetPlan& replacePlan) const
{
    return AddonsThatWouldChange(replacePlan) == 0 && StartupIsInPlace(snapshot, preset);
}

bool PresetService::StartupIsInPlace(const ProfileSnapshot& snapshot, const Preset& preset) const
{
    std::vector<StartupLine> lines;
    lines.reserve(snapshot.startupEntries.size());
    for (const StartupEntry& entry : snapshot.startupEntries)
    {
        lines.push_back(StartupLine{.label = entry.label, .path = entry.path, .enabled = entry.enabled});
    }

    const PresetStartupPlan startup = PlanPresetStartup(preset, ApplyMode::Replace, lines, startup_.Managing());

    return startup.toTurnOn.empty() && startup.toTurnOff.empty();
}

Preset PresetService::WhatIsOnRightNow(const SimulatorProfile& profile,
                                       const std::vector<TreeNode>& libraries,
                                       const EnabledAddons& enabled,
                                       const std::vector<StartupLine>& lines,
                                       const bool governsStartup) const
{
    Preset preset;
    preset.entries = EntriesForWhatIsEnabled(profile, libraries, enabled);
    preset.governsStartup = governsStartup;

    if (!governsStartup)
    {
        return preset;
    }

    for (const StartupLine& line : lines)
    {
        if (line.enabled)
        {
            preset.startupEntries.push_back(PresetStartupEntry{.path = line.path, .action = PresetAction::Enable});
        }
    }

    return preset;
}

PresetApplyPlan PresetService::Plan(const SimulatorProfile& profile,
                                    const ProfileSnapshot& snapshot,
                                    const Preset& preset,
                                    const ApplyMode mode) const
{
    return Plan(profile, snapshot, preset, mode, snapshot.enabled);
}

PresetApplyPlan PresetService::Plan(const SimulatorProfile& profile,
                                    const ProfileSnapshot& snapshot,
                                    const Preset& preset,
                                    const ApplyMode mode,
                                    const EnabledAddons& enabled) const
{
    return {.addons = PlanPresetApplication(preset, mode, profile, snapshot.libraries, enabled),
            .startup = PlanPresetStartup(preset, mode, startup_.Report(profile, snapshot).lines, startup_.Managing())};
}

PresetApplyOutcome PresetService::Apply(const EntriesStamp& stamp,
                                        const ProfileSnapshot& snapshot,
                                        const Preset& preset,
                                        const ApplyMode mode) const
{
    return Apply(stamp, snapshot, preset, mode, true);
}

PresetApplyOutcome
PresetService::ApplyTheReturn(const EntriesStamp& stamp, const ProfileSnapshot& snapshot, const Preset& preset) const
{
    return Apply(stamp, snapshot, preset, ApplyMode::Replace, false);
}

PresetApplyOutcome PresetService::Apply(const EntriesStamp& stamp,
                                        const ProfileSnapshot& snapshot,
                                        const Preset& preset,
                                        const ApplyMode mode,
                                        const bool recordReturn) const
{
    const SimulatorProfile& profile = stamp.profile;
    const ProfileService::LinksOnDisk onDisk = profiles_.ReadLinksNow(profile, snapshot.libraries);
    const PresetApplyPlan plan = Plan(profile, snapshot, preset, mode, onDisk.enabled);

    if (recordReturn)
    {
        const std::vector<StartupLine> lines = startup_.Report(profile, snapshot).lines;
        const bool governsStartup = preset.governsStartup && startup_.Managing();

        if (!presets_.SaveReturnPreset(
                profile.id, WhatIsOnRightNow(profile, snapshot.libraries, onDisk.enabled, lines, governsStartup)))
        {
            return {.report = {.refusal = PresetApplyRefusal::TheReturnPresetCouldNotBeWritten}, .read = {}};
        }
    }

    const LinkBatch batch{.toDisable = plan.addons.toDisable,
                          .toEnable = plan.addons.toEnable,
                          .startupEntriesToTurnOff = {},
                          .startupSwitches = SwitchesFor(plan.startup)};

    LinkBatchOutcome written = profiles_.SetEnabled(stamp, snapshot, batch, onDisk);

    return {.report = {.results = std::move(written.report.results),
                       .unresolved = plan.addons.unresolved,
                       .startupUnresolved = plan.startup.unresolved,
                       .startupNotApplied = plan.startup.notApplied,
                       .refusal = PresetApplyRefusal::None},
            .read = std::move(written.read)};
}
