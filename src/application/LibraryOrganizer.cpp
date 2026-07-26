#include "application/LibraryOrganizer.h"

#include "domain/linking/DisableLinks.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"
#include "domain/tree/LibraryTrees.h"

namespace
{
    void CarryTheOverrides(SimulatorProfile& profile,
                           const Library& library,
                           const std::filesystem::path& from,
                           const std::filesystem::path& to)
    {
        const std::string moved = ComparablePath(RelativeToLibrary(library, from));
        const std::filesystem::path landing = RelativeToLibrary(library, to);

        for (DestinationOverride& known : profile.destinationOverrides)
        {
            if (known.libraryId != library.id)
            {
                continue;
            }

            const std::string key = ComparablePath(known.relativePath);
            if (key == moved)
            {
                known.relativePath = landing;
                continue;
            }

            if (key.size() > moved.size() && key.compare(0, moved.size(), moved) == 0 && key[moved.size()] == '/')
            {
                known.relativePath = landing / key.substr(moved.size() + 1);
            }
        }
    }

    std::vector<std::filesystem::path> EnabledAddonsUnder(const TreeNode& library,
                                                          const std::vector<DestinationEntry>& entries,
                                                          const std::filesystem::path& category)
    {
        std::vector<std::filesystem::path> enabled;

        for (const TreeNode* addon : AddonsUnder(library))
        {
            if (PathIsInside(addon->path, category) && !LinksPointingAt(entries, addon->path).empty())
            {
                enabled.push_back(addon->path);
            }
        }

        return enabled;
    }
}

LibraryOrganizer::LibraryOrganizer(const CatalogScanner& catalog,
                                   const FilesystemProbe& filesystemProbe,
                                   FileOperations& files,
                                   const LinkingEngine& linking,
                                   const EntryClassifier& classifier,
                                   const ProcessProbe& processProbe,
                                   const OperationLog& log,
                                   const LinkType linkType)
    : catalog_(catalog),
      filesystemProbe_(filesystemProbe),
      files_(files),
      linking_(linking),
      classifier_(classifier),
      processProbe_(processProbe),
      log_(log),
      linkType_(linkType)
{
}

void LibraryOrganizer::Record(const OperationKind kind,
                              const AddonId& addon,
                              const std::filesystem::path& source,
                              const std::filesystem::path& target,
                              const ImportResult result) const
{
    log_.RecordImport(kind, addon, source, target, result);
}

FileOperationResult LibraryOrganizer::CreateCategory(const SimulatorProfile& profile,
                                                     const std::filesystem::path& parent,
                                                     const std::string& name) const
{
    const std::filesystem::path folder = parent / name;

    if (processProbe_.SimulatorIsRunning())
    {
        return FileOperationResult{folder, ImportResult::TheSimulatorIsRunning};
    }

    if (LibraryContaining(profile, parent) == nullptr)
    {
        return FileOperationResult{folder, ImportResult::TheTargetIsNotInALibrary};
    }

    const bool created = files_.CreateFolder(folder);
    const ImportResult result = created ? ImportResult::Completed : ImportResult::CouldNotCreateTheCategory;

    Record(OperationKind::CreateCategory, IdentityOf(profile, folder), {}, folder, result);

    return FileOperationResult{folder, result};
}

FileOperationResult LibraryOrganizer::RenameCategory(SimulatorProfile& profile,
                                                     const std::filesystem::path& category,
                                                     const std::string& name) const
{
    const std::filesystem::path landing = category.parent_path() / name;

    if (processProbe_.SimulatorIsRunning())
    {
        return FileOperationResult{landing, ImportResult::TheSimulatorIsRunning};
    }

    const Library* library = LibraryContaining(profile, category);
    if (library == nullptr)
    {
        return FileOperationResult{landing, ImportResult::TheTargetIsNotInALibrary};
    }

    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(landing))
    {
        return FileOperationResult{landing, ImportResult::CouldNotMoveIntoPlace, landing};
    }

    const std::vector<DestinationEntry> entries = classifier_.Resolve(profile.destinations, {library->path});

    const TreeNode scanned = catalog_.Scan(library->path);
    const std::vector<std::filesystem::path> enabled = EnabledAddonsUnder(scanned, entries, category);

    for (const std::filesystem::path& addon : enabled)
    {
        if (!DisableEveryLink(linking_, log_, LinksPointingAt(entries, addon), IdentityOf(profile, addon), addon))
        {
            return FileOperationResult{addon, ImportResult::CouldNotRemoveTheLink};
        }
    }

    const bool moved = files_.Move(category, landing);
    Record(OperationKind::RenameCategory, IdentityOf(profile, landing), category, landing,
           moved ? ImportResult::Completed : ImportResult::CouldNotMoveIntoPlace);

    if (!moved)
    {
        return FileOperationResult{landing, ImportResult::CouldNotMoveIntoPlace};
    }

    CarryTheOverrides(profile, *library, category, landing);

    ImportResult result = ImportResult::Completed;
    for (const std::filesystem::path& addon : enabled)
    {
        const std::filesystem::path folder = landing / addon.lexically_relative(category);

        if (!Relink(profile, IdentityOf(profile, folder), folder))
        {
            result = ImportResult::CouldNotCreateLink;
        }
    }

    return FileOperationResult{landing, result};
}

bool LibraryOrganizer::Relink(const SimulatorProfile& profile,
                              const AddonId& addon,
                              const std::filesystem::path& folder) const
{
    const std::filesystem::path destination = EffectiveDestination(profile, folder);
    const LinkOutcome outcome = linking_.Enable(Addon{folder, Manifest{}}, destination, linkType_);

    log_.RecordLink(OperationKind::EnableAddon, addon, folder,
                                            destination / folder.filename(), outcome.Failure());

    return outcome.Succeeded();
}

FileOperationResult LibraryOrganizer::MoveOne(SimulatorProfile& profile,
                                              const std::vector<TreeNode>& libraries,
                                              const AddonMove& move) const
{
    const std::filesystem::path target = move.Target();

    const Library* library = LibraryContaining(profile, move.category);
    if (library == nullptr || LibraryContaining(profile, move.addonFolder) != library)
    {
        return FileOperationResult{move.addonFolder, ImportResult::TheTargetIsNotInALibrary};
    }

    if (const TreeNode* occupant = AddonHoldingTheIdentity(libraries, target, move.addonFolder))
    {
        return FileOperationResult{move.addonFolder, ImportResult::TheIdentityIsTaken, occupant->path};
    }

    const AddonId addon = IdentityOf(profile, move.addonFolder);
    const std::vector<DestinationEntry> entries = classifier_.Resolve(profile.destinations, {library->path});
    const std::vector<std::filesystem::path> links = LinksPointingAt(entries, move.addonFolder);

    if (!DisableEveryLink(linking_, log_, links, addon, move.addonFolder))
    {
        return FileOperationResult{move.addonFolder, ImportResult::CouldNotRemoveTheLink};
    }

    const bool moved = files_.Move(move.addonFolder, target);
    Record(OperationKind::MoveAddon, addon, move.addonFolder, target,
           moved ? ImportResult::Completed : ImportResult::CouldNotMoveIntoPlace);

    if (!moved)
    {
        return FileOperationResult{move.addonFolder, ImportResult::CouldNotMoveIntoPlace};
    }

    CarryTheOverrides(profile, *library, move.addonFolder, target);

    if (links.empty())
    {
        return FileOperationResult{target, ImportResult::Completed};
    }

    return FileOperationResult{
        target, Relink(profile, addon, target) ? ImportResult::Completed : ImportResult::CouldNotCreateLink};
}

std::vector<FileOperationResult> LibraryOrganizer::Move(SimulatorProfile& profile,
                                                        const std::vector<AddonMove>& moves) const
{
    std::vector<FileOperationResult> results;
    results.reserve(moves.size());

    if (processProbe_.SimulatorIsRunning())
    {
        for (const AddonMove& move : moves)
        {
            results.push_back(FileOperationResult{move.addonFolder, ImportResult::TheSimulatorIsRunning});
        }

        return results;
    }

    const std::vector<TreeNode> libraries = LibraryTreesOf(catalog_, profile);

    for (const AddonMove& move : moves)
    {
        results.push_back(MoveOne(profile, libraries, move));
    }

    return results;
}
