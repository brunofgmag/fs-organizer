#ifndef FS_ORGANIZER_APPLICATION_PRESET_SERVICE_H
#define FS_ORGANIZER_APPLICATION_PRESET_SERVICE_H

#include <optional>
#include <string>
#include <vector>

#include "application/ProfileService.h"
#include "application/StartupReport.h"
#include "application/preset/PresetStartupPlan.h"
#include "application/model/LinkOperationResult.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/PresetRepository.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/preset/PresetPlan.h"

struct PresetApplyPlan
{
    PresetPlan addons{};
    PresetStartupPlan startup{};
};

enum class PresetApplyRefusal : int
{
    None = 0,
    TheReturnPresetCouldNotBeWritten = 1,
};

struct PresetApplyReport
{
    std::vector<LinkOperationResult> results{};
    std::vector<AddonId> unresolved{};
    std::vector<std::filesystem::path> startupUnresolved{};
    std::size_t startupNotApplied = 0;
    PresetApplyRefusal refusal = PresetApplyRefusal::None;
};

class PresetService
{
public:
    PresetService(PresetRepository& presets, ProfileService& profiles, const StartupService& startup);

    [[nodiscard]] std::vector<PresetListing> List(const std::string& profileId) const;

    [[nodiscard]] std::optional<Preset> Load(const std::string& profileId, const std::string& name) const;

    [[nodiscard]] bool
    Create(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const std::string& name) const;

    [[nodiscard]] bool Store(const std::string& profileId, const Preset& preset) const;

    [[nodiscard]] bool
    Update(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const std::string& name) const;

    [[nodiscard]] bool GovernStartup(const SimulatorProfile& profile,
                                     const ProfileSnapshot& snapshot,
                                     const std::string& name,
                                     bool governs) const;

    [[nodiscard]] bool SetAction(const std::string& profileId,
                                 const std::string& name,
                                 std::size_t index,
                                 const AddonId& expected,
                                 PresetAction action) const;

    [[nodiscard]] bool Rename(const std::string& profileId, const std::string& from, const std::string& to) const;

    void Remove(const std::string& profileId, const std::string& name) const;

    [[nodiscard]] std::optional<Preset> ReturnPreset(const std::string& profileId) const;

    [[nodiscard]] PresetApplyPlan
    Plan(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const Preset& preset, ApplyMode mode) const;

    [[nodiscard]] PresetApplyReport
    Apply(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const Preset& preset, ApplyMode mode) const;

private:
    [[nodiscard]] PresetApplyPlan Plan(const SimulatorProfile& profile,
                                       const ProfileSnapshot& snapshot,
                                       const Preset& preset,
                                       ApplyMode mode,
                                       const EnabledAddons& enabled) const;

    [[nodiscard]] std::vector<PresetStartupEntry> StartupEntriesForWhatIsOn(const SimulatorProfile& profile,
                                                                            const ProfileSnapshot& snapshot) const;

    [[nodiscard]] Preset WhatIsOnRightNow(const SimulatorProfile& profile,
                                          const std::vector<TreeNode>& libraries,
                                          const EnabledAddons& enabled,
                                          const std::vector<StartupLine>& lines,
                                          bool governsStartup) const;

    PresetRepository& presets_;
    ProfileService& profiles_;
    const StartupService& startup_;
};

#endif // FS_ORGANIZER_APPLICATION_PRESET_SERVICE_H
