#ifndef FS_ORGANIZER_APPLICATION_PRESET_SERVICE_H
#define FS_ORGANIZER_APPLICATION_PRESET_SERVICE_H

#include <optional>
#include <string>
#include <vector>

#include "application/ProfileService.h"
#include "application/model/LinkOperationResult.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/PresetRepository.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/preset/PresetPlan.h"

struct PresetApplyReport
{
    std::vector<LinkOperationResult> results;
    std::vector<AddonId> unresolved;
};

class PresetService
{
public:
    PresetService(PresetRepository& presets, ProfileService& profiles);

    [[nodiscard]] std::vector<PresetListing> List(const std::string& profileId) const;

    [[nodiscard]] std::optional<Preset> Load(const std::string& profileId, const std::string& name) const;

    [[nodiscard]] bool
    Create(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const std::string& name) const;

    [[nodiscard]] bool
    Update(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const std::string& name) const;

    [[nodiscard]] bool SetAction(const std::string& profileId,
                                 const std::string& name,
                                 std::size_t index,
                                 const AddonId& expected,
                                 PresetAction action) const;

    [[nodiscard]] bool Rename(const std::string& profileId, const std::string& from, const std::string& to) const;

    void Remove(const std::string& profileId, const std::string& name) const;

    [[nodiscard]] PresetApplyReport
    Apply(const SimulatorProfile& profile, const ProfileSnapshot& snapshot, const Preset& preset, ApplyMode mode) const;

private:
    PresetRepository& presets_;
    ProfileService& profiles_;
};

#endif // FS_ORGANIZER_APPLICATION_PRESET_SERVICE_H
