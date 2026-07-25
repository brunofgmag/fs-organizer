#include "application/ImportService.h"

#include <algorithm>
#include <ranges>

#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    std::filesystem::path QuarantineBesideTheDestination(const SimulatorProfile& profile,
                                                         const std::filesystem::path& entry)
    {
        const auto destination = std::ranges::find_if(
            profile.destinations, [&entry](const std::filesystem::path& candidate)
            {
                return PathIsInside(entry, candidate);
            });

        return destination == profile.destinations.end()
                   ? std::filesystem::path{}
                   : destination->parent_path() / kQuarantineFolderName / entry.filename();
    }

    std::filesystem::path QuarantineInsideTheLibrary(const SimulatorProfile& profile,
                                                     const std::filesystem::path& addon)
    {
        const Library* library = LibraryContaining(profile, addon);

        return library == nullptr
                   ? std::filesystem::path{}
                   : library->path / kQuarantineFolderName / addon.filename();
    }

    struct Resolution
    {
        std::filesystem::path loser;
        std::filesystem::path quarantine;
        OperationKind kind = OperationKind::QuarantineFromDestination;
        bool relinks = false;
    };

    Resolution ResolutionFor(const SimulatorProfile& profile,
                             const CopyConflict& conflict,
                             const ConflictChoice choice)
    {
        if (choice == ConflictChoice::KeepTheLibraryCopy)
        {
            return {
                conflict.destinationPath,
                QuarantineBesideTheDestination(profile, conflict.destinationPath),
                OperationKind::QuarantineFromDestination, true
            };
        }

        return {
            conflict.libraryPath, QuarantineInsideTheLibrary(profile, conflict.libraryPath),
            OperationKind::QuarantineFromLibrary, false
        };
    }
}

ImportService::ImportService(const ImportEngine& engine,
                             const ProcessProbe& processProbe,
                             FileOperations& files,
                             const LinkingEngine& linking,
                             OperationJournal& journal,
                             const Clock& clock,
                             const LinkType linkType)
    : engine_(engine), processProbe_(processProbe), files_(files), linking_(linking),
      journal_(journal), clock_(clock), linkType_(linkType)
{
}

std::vector<ImportOperationResult> ImportService::Import(
    const SimulatorProfile& profile,
    const std::vector<ImportRequest>& requests,
    const std::function<bool(const CopyProgress &)>& onProgress) const
{
    std::vector<ImportOperationResult> results;
    results.reserve(requests.size());

    const bool blocked = processProbe_.SimulatorIsRunning();

    for (const ImportRequest& request : requests)
    {
        results.push_back(ImportOperationResult{
            request,
            blocked
                ? ImportResult::TheSimulatorIsRunning
                : engine_.Import(profile, request, onProgress).Result()
        });
    }

    return results;
}

ImportResult ImportService::ResolveConflict(const SimulatorProfile& profile,
                                            const CopyConflict& conflict,
                                            const ConflictChoice choice) const
{
    if (processProbe_.SimulatorIsRunning())
    {
        return ImportResult::TheSimulatorIsRunning;
    }

    const Resolution resolution = ResolutionFor(profile, conflict, choice);

    if (resolution.quarantine.empty())
    {
        return ImportResult::CouldNotQuarantine;
    }

    const AddonId addon = IdentityOf(profile, conflict.libraryPath);
    const bool moved = files_.Move(resolution.loser, resolution.quarantine);

    journal_.Append(OperationRecord::OfImport(
        clock_.Now(), resolution.kind, addon, resolution.loser, resolution.quarantine,
        moved ? ImportResult::Completed : ImportResult::CouldNotQuarantine));

    if (!moved)
    {
        return ImportResult::CouldNotQuarantine;
    }

    if (!resolution.relinks)
    {
        return ImportResult::Completed;
    }

    const LinkOutcome link =
        linking_.Enable(Addon{conflict.libraryPath}, conflict.destinationPath.parent_path(), linkType_);

    journal_.Append(OperationRecord::OfLink(clock_.Now(), OperationKind::EnableAddon, addon,
                                            conflict.libraryPath, conflict.destinationPath,
                                            link.Failure()));

    return link.Succeeded() ? ImportResult::Completed : ImportResult::CouldNotCreateLink;
}
