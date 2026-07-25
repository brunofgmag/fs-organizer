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
}

ImportService::ImportService(const ImportEngine& engine,
                             const ProcessProbe& processProbe,
                             FileOperations& files,
                             const LinkingEngine& linking,
                             const LinkType linkType)
    : engine_(engine), processProbe_(processProbe), files_(files), linking_(linking),
      linkType_(linkType)
{
}

std::vector<ImportOperationResult> ImportService::Import(
    const SimulatorProfile& profile,
    const std::vector<ImportRequest>& requests,
    const std::function<bool(const CopyProgress&)>& onProgress) const
{
    std::vector<ImportOperationResult> results;
    results.reserve(requests.size());

    const bool blocked = processProbe_.SimulatorIsRunning();

    for (const ImportRequest& request : requests)
    {
        results.push_back(ImportOperationResult{
            request,
            blocked ? ImportResult::TheSimulatorIsRunning
                    : engine_.Import(profile, request, onProgress).Result()});
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

    const bool libraryWins = choice == ConflictChoice::KeepTheLibraryCopy;

    const std::filesystem::path loser = libraryWins ? conflict.destinationPath : conflict.libraryPath;
    const std::filesystem::path quarantine =
        libraryWins ? QuarantineBesideTheDestination(profile, conflict.destinationPath)
                    : QuarantineInsideTheLibrary(profile, conflict.libraryPath);

    if (quarantine.empty())
    {
        return ImportResult::CouldNotQuarantine;
    }

    if (!files_.Move(loser, quarantine))
    {
        return ImportResult::CouldNotQuarantine;
    }

    if (libraryWins
        && !linking_
                .Enable(Addon{conflict.libraryPath},
                        conflict.destinationPath.parent_path(),
                        linkType_)
                .Succeeded())
    {
        return ImportResult::CouldNotCreateLink;
    }

    return ImportResult::Completed;
}
