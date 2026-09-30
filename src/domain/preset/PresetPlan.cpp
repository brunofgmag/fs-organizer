#include "domain/preset/PresetPlan.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    using AddonsByFolderName = std::map<std::string, const TreeNode*>;
    using AddonsByLibrary = std::map<std::string, AddonsByFolderName>;

    std::string Lowered(std::string text)
    {
        std::ranges::transform(text, text.begin(),
                               [](const unsigned char character)
                               {
                                   return static_cast<char>(std::tolower(character));
                               });

        return text;
    }

    const TreeNode* AddonAt(const PresetLookup& lookup, const AddonId& addonId)
    {
        const auto library = lookup.addons.find(Lowered(addonId.libraryId));

        if (library == lookup.addons.end())
        {
            return nullptr;
        }

        const auto addon = library->second.find(Lowered(addonId.folderName));

        return addon == library->second.end() ? nullptr : addon->second;
    }

    AddonsByLibrary AddonsOfEveryLibrary(const SimulatorProfile& profile, const std::vector<TreeNode>& libraries)
    {
        AddonsByLibrary index;

        for (const Library& library : profile.libraries)
        {
            const TreeNode* tree = LibraryTreeAt(libraries, library.path);

            if (tree == nullptr)
            {
                continue;
            }

            AddonsByFolderName& folders = index[Lowered(library.id)];

            for (const TreeNode* addon : AddonsUnder(*tree))
            {
                folders.emplace(Lowered(AsUtf8(addon->path.filename())), addon);
            }
        }

        return index;
    }

    std::vector<const TreeNode*> EnabledAmong(const std::vector<TreeNode>& libraries, const EnabledAddons& enabled)
    {
        std::vector<const TreeNode*> among;

        for (const TreeNode& library : libraries)
        {
            for (const TreeNode* addon : AddonsUnder(library))
            {
                if (enabled.Contains(addon->path))
                {
                    among.push_back(addon);
                }
            }
        }

        return among;
    }
}

PresetLookup
BuildPresetLookup(const SimulatorProfile& profile, const std::vector<TreeNode>& libraries, const EnabledAddons& enabled)
{
    PresetLookup lookup{.addons = AddonsOfEveryLibrary(profile, libraries)};

    for (const TreeNode* addon : EnabledAmong(libraries, enabled))
    {
        lookup.enabledAddons.push_back(EnabledAddon{.addon = addon, .comparablePath = ComparablePath(addon->path)});
        lookup.enabledPaths.insert(lookup.enabledAddons.back().comparablePath);
    }

    return lookup;
}

PresetPlan PlanPresetApplication(const Preset& preset, const ApplyMode mode, const PresetLookup& lookup)
{
    PresetPlan plan;
    std::set<std::string> named;

    for (const PresetEntry& entry : preset.entries)
    {
        if (mode == ApplyMode::Disable && entry.action == PresetAction::Disable)
        {
            continue;
        }

        const TreeNode* addon = AddonAt(lookup, entry.addonId);

        if (addon == nullptr)
        {
            plan.unresolved.push_back(entry.addonId);
            continue;
        }

        const std::string comparable = ComparablePath(addon->path);

        named.insert(comparable);

        const bool on = lookup.enabledPaths.contains(comparable);
        const bool wantsOn = entry.action == PresetAction::Enable && mode != ApplyMode::Disable;

        if (on == wantsOn)
        {
            plan.alreadyInPlace.push_back(addon);
        }
        else if (wantsOn)
        {
            plan.toEnable.push_back(addon);
        }
        else
        {
            plan.toDisable.push_back(addon);
        }
    }

    if (mode != ApplyMode::Replace)
    {
        return plan;
    }

    for (const EnabledAddon& enabledAddon : lookup.enabledAddons)
    {
        if (named.contains(enabledAddon.comparablePath))
        {
            continue;
        }

        plan.toDisable.push_back(enabledAddon.addon);
        plan.notNamedByThePreset.push_back(enabledAddon.addon);
    }

    return plan;
}

PresetPlan PlanPresetApplication(const Preset& preset,
                                 const ApplyMode mode,
                                 const SimulatorProfile& profile,
                                 const std::vector<TreeNode>& libraries,
                                 const EnabledAddons& enabled)
{
    return PlanPresetApplication(preset, mode, BuildPresetLookup(profile, libraries, enabled));
}

std::size_t AddonsThatWouldChange(const PresetPlan& plan)
{
    return plan.toEnable.size() + plan.toDisable.size();
}

bool PresetIsSatisfied(const Preset& preset, const PresetLookup& lookup)
{
    return AddonsThatWouldChange(PlanPresetApplication(preset, ApplyMode::Replace, lookup)) == 0;
}

bool PresetIsSatisfied(const Preset& preset,
                       const SimulatorProfile& profile,
                       const std::vector<TreeNode>& libraries,
                       const EnabledAddons& enabled)
{
    return PresetIsSatisfied(preset, BuildPresetLookup(profile, libraries, enabled));
}

PresetContent ContentOf(const Preset& preset, const PresetLookup& lookup)
{
    std::set<std::string> categories;

    for (const PresetEntry& entry : preset.entries)
    {
        if (const TreeNode* addon = AddonAt(lookup, entry.addonId); addon != nullptr)
        {
            categories.insert(ComparablePath(addon->path.parent_path()));
        }
    }

    return {.addons = preset.entries.size(), .categories = categories.size()};
}

PresetContent ContentOf(const Preset& preset, const SimulatorProfile& profile, const std::vector<TreeNode>& libraries)
{
    return ContentOf(preset, BuildPresetLookup(profile, libraries, EnabledAddons{}));
}

std::vector<PresetEntry> EntriesForWhatIsEnabled(const SimulatorProfile& profile,
                                                 const std::vector<TreeNode>& libraries,
                                                 const EnabledAddons& enabled)
{
    std::vector<PresetEntry> entries;

    for (const TreeNode* addon : EnabledAmong(libraries, enabled))
    {
        entries.push_back(PresetEntry{.addonId = IdentityOf(profile, addon->path), .action = PresetAction::Enable});
    }

    return entries;
}
