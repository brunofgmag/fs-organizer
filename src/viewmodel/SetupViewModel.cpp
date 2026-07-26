#include "viewmodel/SetupViewModel.h"

#include <algorithm>
#include <ranges>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    constexpr std::string_view kDestinationNamePrefix = "community";

    bool LooksLikeADestination(const std::filesystem::path& path)
    {
        return ComparablePath(path.filename()).starts_with(kDestinationNamePrefix);
    }

    std::filesystem::path DefaultDestination(const std::vector<std::filesystem::path>& destinations)
    {
        const auto shared = std::ranges::find_if(destinations,
                                                 [](const std::filesystem::path& path)
                                                 {
                                                     return ComparablePath(path.filename()) == kDestinationNamePrefix;
                                                 });

        return shared == destinations.end() ? destinations.back() : *shared;
    }

    bool IsInsideARegisteredLibrary(const std::vector<RegisteredLibrary>& libraries, const std::filesystem::path& path)
    {
        std::vector<Library> known;
        for (const RegisteredLibrary& registered : libraries)
        {
            known.push_back(registered.library);
        }

        return LibraryContaining(known, path) != nullptr;
    }

    bool IsTaken(const std::vector<SimulatorProfile>& profiles, const std::string& id)
    {
        return std::ranges::any_of(profiles,
                                   [&id](const SimulatorProfile& profile)
                                   {
                                       return profile.id == id;
                                   });
    }

    std::string ProfileId(const SimulatorVariant variant, const std::vector<SimulatorProfile>& existing)
    {
        const std::string base = variant == SimulatorVariant::MSFS2020 ? "msfs2020" : "msfs2024";

        std::string candidate = base;
        for (int suffix = 2; IsTaken(existing, candidate); ++suffix)
        {
            candidate = base + "-" + std::to_string(suffix);
        }

        return candidate;
    }
}

SetupViewModel::SetupViewModel(const SimulatorLocator& locator,
                               const FilesystemProbe& filesystemProbe,
                               SettingsRepository& settings,
                               const LibraryIdGenerator& identities,
                               const CatalogScanner& catalog,
                               QObject* parent)
    : QObject(parent),
      locator_(locator),
      filesystemProbe_(filesystemProbe),
      settings_(settings),
      identities_(identities),
      catalog_(catalog)
{
}

void SetupViewModel::Detect()
{
    candidates_ = locator_.Locate();
}

std::vector<SimulatorCandidate> SetupViewModel::Candidates() const
{
    return candidates_;
}

DestinationCheck SetupViewModel::CheckDestination(const std::filesystem::path& path) const
{
    if (!filesystemProbe_.TargetDirectoryExists(path))
    {
        return DestinationCheck::RejectedMissing;
    }

    if (!filesystemProbe_.ProbeWritable(path))
    {
        return DestinationCheck::RejectedNotWritable;
    }

    return LooksLikeADestination(path) ? DestinationCheck::Accepted : DestinationCheck::AcceptedButUnfamiliar;
}

void SetupViewModel::AddManualCandidate(const std::filesystem::path& destination, const SimulatorVariant variant)
{
    SimulatorCandidate candidate;
    candidate.variant = variant;
    candidate.packagesPath = destination.parent_path();
    candidate.destinations = {destination};

    candidates_.push_back(candidate);
}

void SetupViewModel::ChooseCandidate(const std::size_t index)
{
    chosen_ = index;
}

LibraryReport SetupViewModel::RegisterLibrary(const std::filesystem::path& path, const std::string& label)
{
    if (IsInsideARegisteredLibrary(libraries_, path))
    {
        return {LibraryCheck::RejectedInsideAnotherLibrary};
    }

    const TreeNode tree = catalog_.Scan(path);

    RegisteredLibrary registered;
    registered.library = Library{identities_.Generate(), path, label};
    registered.categories = tree.children.size();
    registered.addons = CountAddons(tree);

    libraries_.push_back(registered);

    return {LibraryCheck::Accepted, registered.categories, registered.addons};
}

std::vector<RegisteredLibrary> SetupViewModel::Libraries() const
{
    return libraries_;
}

void SetupViewModel::Complete() const
{
    if (chosen_ >= candidates_.size())
    {
        return;
    }

    const SimulatorCandidate& candidate = candidates_[chosen_];
    AppSettings settings = settings_.Load();

    SimulatorProfile profile;
    profile.id = ProfileId(candidate.variant, settings.profiles);
    profile.variant = candidate.variant;
    profile.destinations = candidate.destinations;
    profile.defaultDestination = DefaultDestination(candidate.destinations);

    for (const RegisteredLibrary& registered : libraries_)
    {
        profile.libraries.push_back(registered.library);
    }

    settings.profiles.push_back(profile);
    settings.activeProfileId = profile.id;

    settings_.Save(settings);
}
