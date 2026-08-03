#include "domain/legacy/LegacyPreset.h"

#include <algorithm>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    void Enter(std::vector<PresetEntry>& entries, const AddonId& addonId, const PresetAction action)
    {
        const auto known = std::ranges::find_if(entries,
                                                [&addonId](const PresetEntry& entry)
                                                {
                                                    return entry.addonId == addonId;
                                                });

        if (known == entries.end())
        {
            entries.push_back(PresetEntry{.addonId = addonId, .action = action});
            return;
        }

        if (action == PresetAction::Disable)
        {
            known->action = action;
        }
    }

    const TreeNode* FolderAt(const std::vector<TreeNode>& libraries, const std::filesystem::path& folder)
    {
        const std::string wanted = ComparablePath(folder);

        for (const TreeNode& library : libraries)
        {
            for (const TreeNode* node : CategoriesUnder(library))
            {
                if (ComparablePath(node->path) == wanted)
                {
                    return node;
                }
            }
        }

        return nullptr;
    }
}

ImportedPreset ImportLegacyPreset(const LegacyPresetSelection& selection,
                                  const SimulatorProfile& profile,
                                  const std::vector<TreeNode>& libraries)
{
    ImportedPreset imported;
    imported.preset.name = selection.name;

    for (const std::string& name : selection.enabledAddonNames)
    {
        const TreeNode* addon = AddonNamed(libraries, name);

        if (addon == nullptr)
        {
            imported.unresolvedAddonNames.push_back(name);
            continue;
        }

        Enter(imported.preset.entries, IdentityOf(profile, addon->path), PresetAction::Enable);
    }

    for (const std::filesystem::path& folder : selection.enabledFolders)
    {
        const TreeNode* node = FolderAt(libraries, folder);

        if (node == nullptr)
        {
            imported.unresolvedFolders.push_back(folder);
            continue;
        }

        for (const TreeNode* addon : AddonsUnder(*node))
        {
            Enter(imported.preset.entries, IdentityOf(profile, addon->path), PresetAction::Enable);
        }
    }

    for (const std::string& name : selection.disabledAddonNames)
    {
        const TreeNode* addon = AddonNamed(libraries, name);

        if (addon == nullptr)
        {
            imported.unresolvedAddonNames.push_back(name);
            continue;
        }

        Enter(imported.preset.entries, IdentityOf(profile, addon->path), PresetAction::Disable);
    }

    return imported;
}
