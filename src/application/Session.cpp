#include "application/Session.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "domain/importing/CopyConflicts.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    const SimulatorProfile* ProfileById(const AppSettings& settings, const std::string& id)
    {
        const auto match = std::ranges::find_if(settings.profiles,
                                                [&id](const SimulatorProfile& profile)
                                                {
                                                    return profile.id == id;
                                                });

        return match == settings.profiles.end() ? nullptr : &*match;
    }
}

Session::Session(ProfileService& service,
                 SettingsRepository& settings,
                 BackgroundRunner& runner,
                 SessionObserver& observer)
    : service_(service), settings_(settings), runner_(runner), observer_(observer)
{
}

void Session::ShowActiveProfile()
{
    const AppSettings settings = settings_.Load();
    const SimulatorProfile* active = ProfileById(settings, settings.activeProfileId);

    Scan(active != nullptr ? *active : (settings.profiles.empty() ? SimulatorProfile{} : settings.profiles.front()));
}

void Session::ChooseProfile(const std::string& profileId)
{
    AppSettings settings = settings_.Load();
    settings.activeProfileId = profileId;
    settings_.Save(settings);

    ShowActiveProfile();
}

const SimulatorProfile& Session::Profile() const
{
    return profile_;
}

const ProfileSnapshot& Session::Snapshot() const
{
    return snapshot_;
}

bool Session::Scanning() const
{
    return running_;
}

void Session::RefreshEntries()
{
    snapshot_.entries = service_.ResolveEntries(profile_);
    snapshot_.enabled = EnabledAddons(EnabledAddonFolders(snapshot_.entries));
    snapshot_.conflicts = FindCopyConflicts(snapshot_.entries, snapshot_.libraries);

    observer_.OnRefreshed();
}

LibraryReport Session::RegisterLibrary(const std::filesystem::path& path)
{
    SimulatorProfile next = profile_;
    const LibraryReport report = service_.RegisterLibrary(next, path);

    if (report.Accepted())
    {
        Save(next);
        Scan(std::move(next));
    }

    return report;
}

void Session::OverrideDestination(const TreeNode& node, const std::filesystem::path& destination)
{
    const Library* library = LibraryContaining(profile_, node.path);
    if (library == nullptr)
    {
        return;
    }

    const std::filesystem::path relative = RelativeToLibrary(*library, node.path);
    const LibraryId libraryId = library->id;

    std::erase_if(profile_.destinationOverrides,
                  [&libraryId, &relative](const DestinationOverride& known)
                  {
                      return known.libraryId == libraryId
                          && ComparablePath(known.relativePath) == ComparablePath(relative);
                  });

    if (!destination.empty())
    {
        profile_.destinationOverrides.push_back({libraryId, relative, destination});
    }

    Save(profile_);

    observer_.OnRefreshed();
}

void Session::Scan(SimulatorProfile profile)
{
    if (running_)
    {
        queued_ = std::move(profile);
        return;
    }

    running_ = true;
    scanning_ = std::move(profile);

    observer_.OnScanStarted();

    runner_.Run(
        [this]
        {
            scanned_ = service_.Scan(scanning_);
        },
        [this]
        {
            Adopt();
        });
}

void Session::Adopt()
{
    running_ = false;

    if (queued_.has_value())
    {
        SimulatorProfile next = std::move(*queued_);
        queued_.reset();
        scanned_ = {};
        scanning_ = {};

        Scan(std::move(next));
        return;
    }

    profile_ = std::move(scanning_);
    snapshot_ = std::move(scanned_);
    scanned_ = {};
    scanning_ = {};

    observer_.OnScanFinished();
}

void Session::Save(const SimulatorProfile& profile) const
{
    AppSettings settings = settings_.Load();

    for (SimulatorProfile& stored : settings.profiles)
    {
        if (stored.id == profile.id)
        {
            stored = profile;
        }
    }

    settings_.Save(settings);
}
