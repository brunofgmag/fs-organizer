#include "application/LibraryOrganizer.h"

#include <map>
#include <string>

#include "domain/importing/ExternalSidecar.h"
#include "domain/importing/WhatTheImporterBrought.h"
#include "domain/linking/DisableLinks.h"
#include "domain/linking/LinksByTarget.h"
#include "domain/model/CategoryMarker.h"
#include "domain/profile/ProfileEdits.h"
#include "domain/support/PathSegment.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "domain/tree/LibraryLookup.h"
#include "domain/tree/LibraryTrees.h"

namespace
{
    const TreeNode* CategoryNamed(const TreeNode& tree, const std::filesystem::path& wanted)
    {
        const std::string key = ComparablePath(wanted);

        for (const TreeNode* candidate : CategoriesUnder(tree))
        {
            if (ComparablePath(candidate->path) == key)
            {
                return candidate;
            }
        }

        return nullptr;
    }

    std::vector<std::filesystem::path>
    EnabledAddonsUnder(const TreeNode& library, const LinksByTarget& links, const std::filesystem::path& category)
    {
        std::vector<std::filesystem::path> enabled;

        for (const TreeNode* addon : AddonsUnder(library))
        {
            if (PathIsInside(addon->path, category) && !links.PointingAt(addon->path).empty())
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

void LibraryOrganizer::UseLinkType(const LinkType linkType)
{
    linkType_ = linkType;
}

void LibraryOrganizer::Record(const OperationKind kind,
                              const AddonId& addon,
                              const std::filesystem::path& source,
                              const std::filesystem::path& target,
                              const FileResult result) const
{
    log_.RecordImport(kind, addon, source, target, result);
}

void LibraryOrganizer::DeclareACategory(const Library& library, const std::filesystem::path& folder) const
{
    if (ComparablePath(folder) == ComparablePath(library.path))
    {
        return;
    }

    static_cast<void>(files_.WriteHiddenFile(CategoryMarkerPathIn(folder)));
}

FileOperationResult LibraryOrganizer::CreateCategory(const SimulatorProfile& profile,
                                                     const std::filesystem::path& parent,
                                                     const std::string& name) const
{
    const std::filesystem::path folder = parent / PathFromUtf8(name);

    if (PathTooLong(name))
    {
        return FileOperationResult{.path = folder, .result = FileResult::ThePathIsTooLong};
    }

    if (processProbe_.SimulatorIsRunning())
    {
        return FileOperationResult{.path = folder, .result = FileResult::TheSimulatorIsRunning};
    }

    if (LibraryContaining(profile, parent) == nullptr)
    {
        return FileOperationResult{.path = folder, .result = FileResult::TheTargetIsNotInALibrary};
    }

    const bool created = files_.CreateFolder(folder) && files_.WriteHiddenFile(CategoryMarkerPathIn(folder));
    const FileResult result = created ? FileResult::Completed : FileResult::CouldNotCreateTheCategory;

    Record(OperationKind::CreateCategory, IdentityOf(profile, folder), {}, folder, result);

    return FileOperationResult{.path = folder, .result = result};
}

FileOperationResult LibraryOrganizer::DeclareCategory(const SimulatorProfile& profile,
                                                      const std::filesystem::path& folder) const
{
    if (processProbe_.SimulatorIsRunning())
    {
        return FileOperationResult{.path = folder, .result = FileResult::TheSimulatorIsRunning};
    }

    const Library* library = LibraryContaining(profile, folder);
    if (library == nullptr || ComparablePath(folder) == ComparablePath(library->path))
    {
        return FileOperationResult{.path = folder, .result = FileResult::TheTargetIsNotInALibrary};
    }

    const bool there = filesystemProbe_.TargetDirectoryExists(folder);
    const bool declared = there && files_.WriteHiddenFile(CategoryMarkerPathIn(folder));
    const FileResult result = declared ? FileResult::Completed : FileResult::CouldNotCreateTheCategory;

    Record(OperationKind::CreateCategory, IdentityOf(profile, folder), {}, folder, result);

    return FileOperationResult{.path = folder, .result = result};
}

FileOperationResult LibraryOrganizer::RemoveCategory(SimulatorProfile& profile,
                                                     const std::filesystem::path& category) const
{
    if (processProbe_.SimulatorIsRunning())
    {
        return FileOperationResult{.path = category, .result = FileResult::TheSimulatorIsRunning};
    }

    const Library* library = LibraryContaining(profile, category);
    if (library == nullptr || ComparablePath(category) == ComparablePath(library->path))
    {
        return FileOperationResult{.path = category, .result = FileResult::TheTargetIsNotInALibrary};
    }

    const std::vector<TreeNode> libraries = LibraryTreesOf(catalog_, profile);
    const TreeNode* tree = LibraryTreeAt(libraries, library->path);
    const TreeNode* scanned = tree == nullptr ? nullptr : CategoryNamed(*tree, category);

    if (scanned == nullptr)
    {
        return FileOperationResult{.path = category, .result = FileResult::TheTargetIsNotInALibrary};
    }

    if (CountAddons(*scanned) > 0)
    {
        return FileOperationResult{.path = category, .result = FileResult::TheCategoryStillHoldsAddons};
    }

    static_cast<void>(files_.RemoveTree(CategoryMarkerPathIn(category)));

    const bool removed = files_.RemoveEmptyFolder(category);
    const FileResult result = removed ? FileResult::Completed : FileResult::CouldNotRemoveTheCategory;

    if (removed)
    {
        ForgetTheFolder(profile, *library, category);
    }

    Record(OperationKind::RemoveCategory, IdentityOf(profile, category), category, {}, result);

    return FileOperationResult{.path = category, .result = result};
}

FileOperationResult LibraryOrganizer::CheckRenameCategory(const SimulatorProfile& profile,
                                                          const std::filesystem::path& category,
                                                          const std::string& name) const
{
    const std::filesystem::path landing = category.parent_path() / PathFromUtf8(name);

    if (processProbe_.SimulatorIsRunning())
    {
        return FileOperationResult{.path = landing, .result = FileResult::TheSimulatorIsRunning};
    }

    if (LibraryContaining(profile, category) == nullptr)
    {
        return FileOperationResult{.path = landing, .result = FileResult::TheTargetIsNotInALibrary};
    }

    if (filesystemProbe_.EntryExistsWithoutFollowingLinks(landing))
    {
        return FileOperationResult{.path = landing, .result = FileResult::CouldNotMoveIntoPlace, .occupant = landing};
    }

    return FileOperationResult{.path = landing, .result = FileResult::Completed};
}

FileOperationResult LibraryOrganizer::RenameCategory(SimulatorProfile& profile,
                                                     const std::filesystem::path& category,
                                                     const std::string& name) const
{
    const FileOperationResult check = CheckRenameCategory(profile, category, name);
    if (!Succeeded(check.result))
    {
        return check;
    }

    const std::filesystem::path& landing = check.path;
    const Library* library = LibraryContaining(profile, category);

    const LinksByTarget links(classifier_.Resolve(profile.destinations, {library->path}));

    const TreeNode scanned = catalog_.Scan(library->path);
    const std::vector<std::filesystem::path> enabled = EnabledAddonsUnder(scanned, links, category);

    for (const std::filesystem::path& addon : enabled)
    {
        if (!DisableEveryLink(linking_, log_, links.PointingAt(addon), IdentityOf(profile, addon), addon))
        {
            return FileOperationResult{.path = addon, .result = FileResult::CouldNotRemoveTheLink};
        }
    }

    const bool moved = files_.Move(category, landing);
    Record(OperationKind::RenameCategory, IdentityOf(profile, landing), category, landing,
           moved ? FileResult::Completed : FileResult::CouldNotMoveIntoPlace);

    if (!moved)
    {
        return FileOperationResult{.path = landing, .result = FileResult::CouldNotMoveIntoPlace};
    }

    CarryTheFolder(profile, *library, category, landing);

    auto result = FileResult::Completed;
    for (const std::filesystem::path& addon : enabled)
    {
        const std::filesystem::path folder = landing / addon.lexically_relative(category);

        if (!Relink(profile, IdentityOf(profile, folder), folder))
        {
            result = FileResult::CouldNotCreateLink;
        }
    }

    return FileOperationResult{.path = landing, .result = result};
}

bool LibraryOrganizer::Relink(const SimulatorProfile& profile,
                              const AddonId& addon,
                              const std::filesystem::path& folder) const
{
    const std::filesystem::path destination = EffectiveDestination(profile, folder);
    const LinkOutcome outcome =
        linking_.Enable(Addon{.folderPath = folder, .manifest = Manifest{}}, destination, linkType_);

    log_.RecordLink(OperationKind::EnableAddon, addon, folder, destination / folder.filename(), outcome.Failure());

    return outcome.Succeeded();
}

FileOperationResult LibraryOrganizer::MoveOne(SimulatorProfile& profile,
                                              const std::vector<TreeNode>& libraries,
                                              const AddonMove& move,
                                              std::map<std::string, LinksByTarget>& linksByLibrary) const
{
    const std::filesystem::path target = move.Target();

    const Library* library = LibraryContaining(profile, move.category);
    if (library == nullptr || LibraryContaining(profile, move.addonFolder) != library)
    {
        return FileOperationResult{.path = move.addonFolder, .result = FileResult::TheTargetIsNotInALibrary};
    }

    if (const TreeNode* occupant = AddonHoldingTheIdentity(libraries, target, move.addonFolder))
    {
        return FileOperationResult{
            .path = move.addonFolder, .result = FileResult::TheIdentityIsTaken, .occupant = occupant->path};
    }

    const AddonId addon = IdentityOf(profile, move.addonFolder);

    auto known = linksByLibrary.find(ComparablePath(library->path));
    if (known == linksByLibrary.end())
    {
        known = linksByLibrary
                    .emplace(ComparablePath(library->path),
                             LinksByTarget(classifier_.Resolve(profile.destinations, {library->path})))
                    .first;
    }

    const std::vector<std::filesystem::path>& links = known->second.PointingAt(move.addonFolder);

    if (!DisableEveryLink(linking_, log_, links, addon, move.addonFolder))
    {
        return FileOperationResult{.path = move.addonFolder, .result = FileResult::CouldNotRemoveTheLink};
    }

    const bool moved = files_.Move(move.addonFolder, target);
    Record(OperationKind::MoveAddon, addon, move.addonFolder, target,
           moved ? FileResult::Completed : FileResult::CouldNotMoveIntoPlace);

    if (!moved)
    {
        return FileOperationResult{.path = move.addonFolder, .result = FileResult::CouldNotMoveIntoPlace};
    }

    DeclareACategory(*library, move.addonFolder.parent_path());
    static_cast<void>(files_.Move(ExternalSidecarPathFor(move.addonFolder), ExternalSidecarPathFor(target)));
    CarryTheFolder(profile, *library, move.addonFolder, target);

    if (links.empty())
    {
        return FileOperationResult{.path = target, .result = FileResult::Completed};
    }

    return FileOperationResult{.path = target,
                               .result = Relink(profile, addon, target) ? FileResult::Completed
                                                                        : FileResult::CouldNotCreateLink};
}

std::vector<std::filesystem::path> LibraryOrganizer::WhatTheImporterBroughtInto(const Library& library) const
{
    std::vector<std::filesystem::path> inside;

    for (const std::filesystem::path& folder : FoldersTheImporterBrought(log_.History()))
    {
        if (PathIsInside(folder, library.path))
        {
            inside.push_back(folder);
        }
    }

    return inside;
}

LibraryGrouping LibraryOrganizer::HowItIsGrouped(const Library& library) const
{
    return HowTheLibraryIsGrouped(filesystemProbe_, library.path, WhatTheImporterBroughtInto(library));
}

std::vector<FileOperationResult> LibraryOrganizer::AdoptTheStructure(const SimulatorProfile& profile,
                                                                     const Library& library) const
{
    std::vector<FileOperationResult> results;

    for (const std::filesystem::path& folder : HowItIsGrouped(library).notYetDeclared)
    {
        const bool declared = files_.WriteHiddenFile(CategoryMarkerPathIn(folder));
        const FileResult result = declared ? FileResult::Completed : FileResult::CouldNotCreateTheCategory;

        Record(OperationKind::CreateCategory, IdentityOf(profile, folder), {}, folder, result);
        results.push_back(FileOperationResult{.path = folder, .result = result});
    }

    return results;
}

std::vector<FileOperationResult> LibraryOrganizer::TakeBackEveryMarkerItWrote(const SimulatorProfile& profile,
                                                                              const Library& library) const
{
    std::vector<FileOperationResult> results;

    for (const std::filesystem::path& folder : HowItIsGrouped(library).alreadyDeclared)
    {
        const bool taken = files_.RemoveTree(CategoryMarkerPathIn(folder));
        const FileResult result = taken ? FileResult::Completed : FileResult::CouldNotRemoveTheCategory;

        Record(OperationKind::TakeBackTheCategoryMarker, IdentityOf(profile, folder), folder, {}, result);
        results.push_back(FileOperationResult{.path = folder, .result = result});
    }

    return results;
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
            results.push_back(
                FileOperationResult{.path = move.addonFolder, .result = FileResult::TheSimulatorIsRunning});
        }

        return results;
    }

    const std::vector<TreeNode> libraries = LibraryTreesOf(catalog_, profile);

    std::map<std::string, LinksByTarget> linksByLibrary;

    for (const AddonMove& move : moves)
    {
        results.push_back(MoveOne(profile, libraries, move, linksByLibrary));
    }

    return results;
}
