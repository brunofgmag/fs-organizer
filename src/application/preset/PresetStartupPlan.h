#ifndef FS_ORGANIZER_APPLICATION_PRESET_PRESET_STARTUP_PLAN_H
#define FS_ORGANIZER_APPLICATION_PRESET_PRESET_STARTUP_PLAN_H

#include <cstddef>
#include <filesystem>
#include <vector>

#include "application/StartupReport.h"
#include "domain/model/Preset.h"
#include "domain/preset/PresetPlan.h"

struct PresetStartupPlan
{
    std::vector<StartupLine> toTurnOn{};
    std::vector<StartupLine> toTurnOff{};
    std::vector<std::filesystem::path> unresolved{};
    std::size_t asked = 0;
    std::size_t notApplied = 0;
};

[[nodiscard]] PresetStartupPlan
PlanPresetStartup(const Preset& preset, ApplyMode mode, const std::vector<StartupLine>& lines, bool managing);

#endif // FS_ORGANIZER_APPLICATION_PRESET_PRESET_STARTUP_PLAN_H
