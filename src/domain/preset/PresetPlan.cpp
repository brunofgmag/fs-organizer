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

    AddonsByLibrary AddonsOfEveryLibrary(const std::vector<TreeNode>& libraries, const SimulatorProfile& profile)
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
                folders.emplace(Lowered(addon->path.filename().string()), addon);
            }
        }

        return index;
    }

    const TreeNode* AddonAt(const AddonsByLibrary& index, const AddonId& addonId)
    {
        const auto library = index.find(Lowered(addonId.libraryId));

        if (library == index.end())
        {
            return nullptr;
        }

        const auto addon = library->second.find(Lowered(addonId.folderName));

        return addon == library->second.end() ? nullptr : addon->second;
    }
}

PresetPlan PlanPresetApplication(const Preset& preset,
                                 const ApplyMode mode,
                                 const SimulatorProfile& profile,
                                 const std::vector<TreeNode>& libraries,
                                 const EnabledAddons& enabled)
{
    PresetPlan plan;
    std::set<std::string> named;
    const AddonsByLibrary index = AddonsOfEveryLibrary(libraries, profile);

    for (const PresetEntry& entry : preset.entries)
    {
        if (mode == ApplyMode::Disable && entry.action == PresetAction::Disable)
        {
            continue;
        }

        const TreeNode* addon = AddonAt(index, entry.addonId);

        if (addon == nullptr)
        {
            plan.unresolved.push_back(entry.addonId);
            continue;
        }

        named.insert(ComparablePath(addon->path));

        const bool on = enabled.Contains(addon->path);
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

    for (const TreeNode& library : libraries)
    {
        for (const TreeNode* addon : AddonsUnder(library))
        {
            if (named.contains(ComparablePath(addon->path)) || !enabled.Contains(addon->path))
            {
                continue;
            }

            plan.toDisable.push_back(addon);
        }
    }

    return plan;
}

std::vector<PresetEntry> EntriesForWhatIsEnabled(const SimulatorProfile& profile,
                                                 const std::vector<TreeNode>& libraries,
                                                 const EnabledAddons& enabled)
{
    std::vector<PresetEntry> entries;

    for (const TreeNode& library : libraries)
    {
        for (const TreeNode* addon : AddonsUnder(library))
        {
            if (enabled.Contains(addon->path))
            {
                entries.push_back(PresetEntry{IdentityOf(profile, addon->path), PresetAction::Enable});
            }
        }
    }

    return entries;
}
