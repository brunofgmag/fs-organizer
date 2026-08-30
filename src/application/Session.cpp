#include "application/Session.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "domain/importing/CopyConflicts.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/profile/ExternalOrigins.h"
#include "domain/profile/OrphanOverrides.h"
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
                 SettingsRepository& repository,
                 AppSettings stored,
                 const ProcessProbe& probe,
                 BackgroundRunner& runner,
                 SessionObserver& observer)
    : service_(service),
      organizer_(organizer),
      repository_(repository),
      settings_(std::move(stored)),
      probe_(probe),
      runner_(runner),
      observer_(observer)
{
}

const AppSettings& Session::Settings() const
{
    return settings_;
}

bool Session::Rewrite(const std::function<bool(AppSettings&)>& change)
{
    AppSettings next = settings_;

    return change(next) && Commit(std::move(next));
}

bool Session::Commit(AppSettings next)
{
    if (!repository_.Save(next))
    {
        return false;
    }

    settings_ = std::move(next);

    return true;
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
    const SimulatorProfile* active = ProfileById(settings_, settings_.activeProfileId);

    if (active != nullptr)
    {
        Scan(*active);
        return;
    }

    Scan(settings_.profiles.empty() ? SimulatorProfile{} : settings_.profiles.front());
}

void Session::ChooseProfile(const std::string& profileId)
{
    const bool written = Rewrite(
        [&profileId](AppSettings& settings)
        {
            settings.activeProfileId = profileId;

            return true;
        });

    if (!written)
    {
        observer_.OnSettingsCouldNotBeSaved();
        return;
    }

    ShowActiveProfile();
}

bool Session::RemoveProfile(const std::string& profileId)
{
    AppSettings next = settings_;

    if (!::RemoveProfile(next.profiles, profileId))
    {
        return false;
    }

    const bool itWasInUse = settings_.activeProfileId == profileId;
    if (itWasInUse)
    {
        next.activeProfileId = next.profiles.front().id;
    }

    if (!Commit(std::move(next)))
    {
        observer_.OnSettingsCouldNotBeSaved();
        return false;
    }

    service_.ForgetUndo();

    if (itWasInUse)
    {
        ShowActiveProfile();
    }

    return true;
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

void Session::CancelScan()
{
    if (running_)
    {
        cancelled_ = true;
    }
}

void Session::RefreshEntries()
{
    snapshot_.entries = service_.ResolveEntries(profile_, snapshot_.libraries);
    snapshot_.enabled = EnabledAddons(EnabledAddonFolders(snapshot_.entries));
    snapshot_.conflicts = FindCopyConflicts(snapshot_.entries, snapshot_.libraries);
    snapshot_.startupEntries = service_.StartupEntriesNow();

    observer_.OnRefreshed();
}

void Session::RefreshStartupEntries()
{
    snapshot_.startupEntries = service_.StartupEntriesNow();

    observer_.OnRefreshed();
}

LibraryReport Session::RegisterLibrary(const std::filesystem::path& path)
{
    SimulatorProfile next = profile_;
    const LibraryReport report = service_.RegisterLibrary(next, path);

    if (report.Accepted())
    {
        const auto registered = std::ranges::find_if(next.libraries,
                                                     [&path](const Library& library)
                                                     {
                                                         return ComparablePath(library.path) == ComparablePath(path);
                                                     });

        if (registered != next.libraries.end())
        {
            static_cast<void>(organizer_.AdoptTheStructure(next, *registered));
        }

        Save(next);
        Scan(std::move(next));
    }

    return report;
}

bool Session::WouldAcceptLibrary(const std::filesystem::path& path) const
{
    return LibraryContaining(profile_, path) == nullptr;
}

Session::LibraryRegistration Session::RegisterLibraryOn(SimulatorProfile profile,
                                                        const std::filesystem::path& path) const
{
    const LibraryReport report = service_.RegisterLibrary(profile, path);

    if (report.Accepted())
    {
        const auto registered = std::ranges::find_if(profile.libraries,
                                                     [&path](const Library& library)
                                                     {
                                                         return ComparablePath(library.path) == ComparablePath(path);
                                                     });

        if (registered != profile.libraries.end())
        {
            static_cast<void>(organizer_.AdoptTheStructure(profile, *registered));
        }
    }

    return {.profile = std::move(profile), .report = report};
}

void Session::AdoptTheRegistration(LibraryRegistration registered)
{
    if (!registered.report.Accepted())
    {
        return;
    }

    Save(registered.profile);
    Scan(std::move(registered.profile));
}

const Library* Session::LibraryNamed(const LibraryId& libraryId) const
{
    for (const Library& library : profile_.libraries)
    {
        if (library.id == libraryId)
        {
            return &library;
        }
    }

    return nullptr;
}

LibraryGrouping Session::HowTheLibraryIsGrouped(const LibraryId& libraryId) const
{
    const Library* library = LibraryNamed(libraryId);

    return library == nullptr ? LibraryGrouping{} : organizer_.HowItIsGrouped(*library);
}

std::vector<FileOperationResult> Session::AdoptTheStructureOf(const LibraryId& libraryId)
{
    const Library* library = LibraryNamed(libraryId);
    if (library == nullptr)
    {
        return {};
    }

    std::vector<FileOperationResult> results = organizer_.AdoptTheStructure(profile_, *library);
    Scan(profile_);

    return results;
}

std::vector<FileOperationResult> Session::TakeBackTheMarkersOf(const LibraryId& libraryId)
{
    const Library* library = LibraryNamed(libraryId);
    if (library == nullptr)
    {
        return {};
    }

    std::vector<FileOperationResult> results = organizer_.TakeBackEveryMarkerItWrote(profile_, *library);
    Scan(profile_);

    return results;
}

void Session::RememberWhatCameFromAnotherProgram(const std::vector<ImportOperationResult>& results)
{
    SimulatorProfile next = profile_;
    bool remembered = false;

    for (const ImportOperationResult& result : results)
    {
        if (!Succeeded(result.result) || !result.request.CameFromAnotherProgram())
        {
            continue;
        }

        RememberWhereItCameFrom(next, result.request.Target(), result.request.externalSource);
        remembered = true;
    }

    if (!remembered)
    {
        return;
    }

    Save(next);
    Scan(std::move(next));
}

void Session::ForgetWhatCameFromAnotherProgram(const std::vector<std::filesystem::path>& addonFolders)
{
    if (addonFolders.empty())
    {
        return;
    }

    SimulatorProfile next = profile_;

    for (const std::filesystem::path& addonFolder : addonFolders)
    {
        ForgetWhereItCameFrom(next, addonFolder);
    }

    Save(next);
    Scan(std::move(next));
}

LegacyImportReport Session::ImportLegacy(const LegacyImportRequest& request)
{
    SimulatorProfile next = profile_;
    LegacyImportReport report;

    for (const std::filesystem::path& root : request.libraryRoots)
    {
        if (service_.RegisterLibrary(next, root).Accepted())
        {
            ++report.librariesRegistered;
            continue;
        }

        report.refused.push_back(root);
    }

    for (const std::filesystem::path& category : request.categories)
    {
        if (Succeeded(organizer_.DeclareCategory(next, category).result))
        {
            ++report.categoriesDeclared;
            continue;
        }

        report.refused.push_back(category);
    }

    if (report.librariesRegistered == 0 && report.categoriesDeclared == 0)
    {
        return report;
    }

    service_.ForgetUndo();
    Save(next);
    ScanBeforeReturning(std::move(next));

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

std::vector<DestinationOverride> Session::OverridesPointingNowhere() const
{
    return ::OverridesPointingNowhere(profile_);
}

void Session::DropOverridesPointingNowhere()
{
    SimulatorProfile next = profile_;
    ::DropOverridesPointingNowhere(next);

    if (next.destinationOverrides.size() == profile_.destinationOverrides.size())
    {
        return;
    }

    profile_ = std::move(next);

    service_.ForgetUndo();
    Save(profile_);
    RefreshEntries();
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
        profile_.destinationOverrides.push_back(
            {.libraryId = libraryId, .relativePath = relative, .destination = destination});
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

FileOperationResult Session::CheckRenameCategory(const std::filesystem::path& category, const std::string& name) const
{
    return organizer_.CheckRenameCategory(profile_, category, name);
}

Session::ReorganizedLibrary Session::MoveAddonsOn(SimulatorProfile profile, const std::vector<AddonMove>& moves) const
{
    std::vector<FileOperationResult> results = organizer_.Move(profile, moves);

    return {.profile = std::move(profile), .results = std::move(results)};
}

Session::ReorganizedLibrary Session::RenameCategoryOn(SimulatorProfile profile,
                                                      const std::filesystem::path& category,
                                                      const std::string& name) const
{
    FileOperationResult result = organizer_.RenameCategory(profile, category, name);

    return {.profile = std::move(profile), .results = {std::move(result)}};
}

Session::ReorganizedLibrary Session::RemoveCategoryOn(SimulatorProfile profile,
                                                      const std::filesystem::path& category) const
{
    FileOperationResult result = organizer_.RemoveCategory(profile, category);

    return {.profile = std::move(profile), .results = {std::move(result)}};
}

void Session::AdoptTheReorganization(SimulatorProfile next, const bool landed)
{
    if (!landed)
    {
        return;
    }

    service_.ForgetUndo();
    Save(next);
    Scan(std::move(next));
}

void Session::Scan(SimulatorProfile profile)
{
    if (running_)
    {
        queued_ = std::move(profile);
        cancelled_ = true;

        return;
    }

    cancelled_ = false;
    running_ = true;
    scanning_ = std::move(profile);

    observer_.OnScanStarted();

    runner_.Run(
        [this]
        {
            const ScanGate gate{.keepGoing = [this]
                                {
                                    return !cancelled_;
                                }};

            scanned_ = service_.Scan(scanning_, gate);
        },
        [this]
        {
            Adopt();
        });
}

void Session::ScanBeforeReturning(SimulatorProfile profile)
{
    if (running_)
    {
        Scan(std::move(profile));
        return;
    }

    running_ = true;
    scanning_ = std::move(profile);

    observer_.OnScanStarted();

    scanned_ = service_.Scan(scanning_);

    Adopt();
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

    if (!scanned_.complete)
    {
        scanned_ = {};
        scanning_ = {};

        observer_.OnScanFinished();
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

void Session::Save(const SimulatorProfile& profile)
{
    const bool written = Rewrite(
        [&profile](AppSettings& settings)
        {
            for (SimulatorProfile& stored : settings.profiles)
            {
                if (stored.id == profile.id)
                {
                    stored = profile;
                }
            }

            return true;
        });

    if (!written)
    {
        observer_.OnSettingsCouldNotBeSaved();
    }
}
