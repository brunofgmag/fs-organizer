#include "application/Session.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "domain/importing/CopyConflicts.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/profile/ProfileEdits.h"
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
                 const LibraryOrganizer& organizer,
                 SettingsRepository& settings,
                 const ProcessProbe& probe,
                 BackgroundRunner& runner,
                 SessionObserver& observer)
    : service_(service), organizer_(organizer), settings_(settings), probe_(probe), runner_(runner), observer_(observer)
{
}

void Session::NoteLinkResults(const std::vector<LinkOperationResult>& results)
{
    const bool changed = std::ranges::any_of(results,
                                             [](const LinkOperationResult& result)
                                             {
                                                 return result.outcome.Succeeded();
                                             });

    if (!changed || !probe_.SimulatorIsRunning())
    {
        return;
    }

    if (!warnedAboutSimulator_)
    {
        warnedAboutSimulator_ = true;
        observer_.OnSimulatorIsRunning();
    }

    if (!restartPending_)
    {
        restartPending_ = true;
        observer_.OnRestartPendingChanged(true);
    }
}

void Session::ShowActiveProfile()
{
    const AppSettings settings = settings_.Load().value_or(AppSettings{});
    const SimulatorProfile* active = ProfileById(settings, settings.activeProfileId);

    Scan(active != nullptr ? *active : (settings.profiles.empty() ? SimulatorProfile{} : settings.profiles.front()));
}

void Session::ChooseProfile(const std::string& profileId)
{
    const std::optional<AppSettings> loaded = settings_.Load();
    if (!loaded.has_value())
    {
        observer_.OnSettingsCouldNotBeSaved();
        return;
    }

    AppSettings settings = *loaded;
    settings.activeProfileId = profileId;

    if (!settings_.Save(settings))
    {
        observer_.OnSettingsCouldNotBeSaved();
        return;
    }

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

void Session::UnregisterLibrary(const LibraryId& libraryId)
{
    SimulatorProfile next = profile_;
    ::UnregisterLibrary(next, libraryId);

    service_.ForgetUndo();
    Save(next);
    Scan(std::move(next));
}

void Session::RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to)
{
    SimulatorProfile next = profile_;
    ::RepointDestination(next, from, to);

    service_.ForgetUndo();
    Save(next);
    Scan(std::move(next));
}

bool Session::RememberTheDestination(const TreeNode& node, const std::filesystem::path& destination)
{
    const Library* library = LibraryContaining(profile_, node.path);
    if (library == nullptr)
    {
        return false;
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

    return true;
}

void Session::OverrideDestination(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& destination)
{
    bool remembered = false;

    for (const TreeNode* node : nodes)
    {
        remembered = RememberTheDestination(*node, destination) || remembered;
    }

    if (!remembered)
    {
        return;
    }

    service_.ForgetUndo();
    Save(profile_);

    observer_.OnRefreshed();
}

FileOperationResult Session::CreateCategory(const std::filesystem::path& parent, const std::string& name)
{
    const FileOperationResult result = organizer_.CreateCategory(profile_, parent, name);

    if (Succeeded(result.result))
    {
        service_.ForgetUndo();
        Scan(profile_);
    }

    return result;
}

FileOperationResult Session::RenameCategory(const std::filesystem::path& category, const std::string& name)
{
    const FileOperationResult result = organizer_.RenameCategory(profile_, category, name);

    if (TheFolderLanded(result.result))
    {
        service_.ForgetUndo();
        Save(profile_);
        Scan(profile_);
    }

    return result;
}

FileOperationResult Session::RemoveCategory(const std::filesystem::path& category)
{
    const FileOperationResult result = organizer_.RemoveCategory(profile_, category);

    if (Succeeded(result.result))
    {
        service_.ForgetUndo();
        Save(profile_);
        Scan(profile_);
    }

    return result;
}

std::vector<FileOperationResult> Session::MoveAddons(const std::vector<AddonMove>& moves)
{
    std::vector<FileOperationResult> results = organizer_.Move(profile_, moves);

    if (std::ranges::any_of(results,
                            [](const FileOperationResult& result)
                            {
                                return TheFolderLanded(result.result);
                            }))
    {
        service_.ForgetUndo();
        Save(profile_);
        Scan(profile_);
    }

    return results;
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

    if (restartPending_ && !probe_.SimulatorIsRunning())
    {
        restartPending_ = false;
        observer_.OnRestartPendingChanged(false);
    }

    observer_.OnScanFinished();
}

void Session::Save(const SimulatorProfile& profile) const
{
    const std::optional<AppSettings> loaded = settings_.Load();
    if (!loaded.has_value())
    {
        observer_.OnSettingsCouldNotBeSaved();
        return;
    }

    AppSettings settings = *loaded;

    for (SimulatorProfile& stored : settings.profiles)
    {
        if (stored.id == profile.id)
        {
            stored = profile;
        }
    }

    if (!settings_.Save(settings))
    {
        observer_.OnSettingsCouldNotBeSaved();
    }
}
