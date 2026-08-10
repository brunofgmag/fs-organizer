#include "application/preset/PresetStartupPlan.h"

#include <set>
#include <string>

#include "domain/support/PathUtils.h"

namespace
{
    const StartupLine* LineAt(const std::vector<StartupLine>& lines, const std::filesystem::path& path)
    {
        const std::string wanted = ComparablePath(path);

        for (const StartupLine& line : lines)
        {
            if (ComparablePath(line.path) == wanted)
            {
                return &line;
            }
        }

        return nullptr;
    }

    void TurnOffWhatThePresetDoesNotName(PresetStartupPlan& plan,
                                         const std::vector<StartupLine>& lines,
                                         const std::set<std::string>& named)
    {
        for (const StartupLine& line : lines)
        {
            if (!line.enabled || named.contains(ComparablePath(line.path)))
            {
                continue;
            }

            plan.toTurnOff.push_back(line);
        }
    }
}

PresetStartupPlan PlanPresetStartup(const Preset& preset,
                                    const ApplyMode mode,
                                    const std::vector<StartupLine>& lines,
                                    const bool managing)
{
    PresetStartupPlan plan;

    if (!preset.governsStartup)
    {
        return plan;
    }

    plan.asked = preset.startupEntries.size();

    if (!managing)
    {
        plan.notApplied = plan.asked;

        return plan;
    }

    std::set<std::string> named;

    for (const PresetStartupEntry& entry : preset.startupEntries)
    {
        if (mode == ApplyMode::Disable && entry.action == PresetAction::Disable)
        {
            continue;
        }

        const StartupLine* line = LineAt(lines, entry.path);

        if (line == nullptr)
        {
            plan.unresolved.push_back(entry.path);
            continue;
        }

        named.insert(ComparablePath(line->path));

        const bool wantsOn = entry.action == PresetAction::Enable && mode != ApplyMode::Disable;

        if (line->enabled == wantsOn)
        {
            continue;
        }

        if (wantsOn)
        {
            plan.toTurnOn.push_back(*line);
        }
        else
        {
            plan.toTurnOff.push_back(*line);
        }
    }

    if (mode == ApplyMode::Replace)
    {
        TurnOffWhatThePresetDoesNotName(plan, lines, named);
    }

    return plan;
}
