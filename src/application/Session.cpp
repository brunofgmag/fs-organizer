#include "application/Session.h"

#include <algorithm>
#include <utility>

#include "domain/importing/CopyConflicts.h"
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

    const Library* LibraryNamed(const SimulatorProfile& profile, const LibraryId& libraryId)
    {
        const auto match = std::ranges::find_if(profile.libraries,
                                                [&libraryId](const Library& library)
                                                {
                                                    return library.id == libraryId;
                                                });

        return match == profile.libraries.end() ? nullptr : &*match;
    }

    bool
    RememberTheDestination(SimulatorProfile& profile, const TreeNode& node, const std::filesystem::path& destination)
    {
        const Library* library = LibraryContaining(profile, node.path);
        if (library == nullptr)
        {
            return false;
        }

        const std::filesystem::path relative = RelativeToLibrary(*library, node.path);
        const LibraryId libraryId = library->id;

        std::erase_if(profile.destinationOverrides,
                      [&libraryId, &relative](const DestinationOverride& known)
                      {
                          return known.libraryId == libraryId
                              && ComparablePath(known.relativePath) == ComparablePath(relative);
                      });

        if (!destination.empty())
        {
            profile.destinationOverrides.push_back(
                {.libraryId = libraryId, .relativePath = relative, .destination = destination});
        }

        return true;
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

namespace
{
    bool SomethingChanged(const std::vector<LinkOperationResult>& results)
    {
        return std::ranges::any_of(results,
                                   [](const LinkOperationResult& result)
                                   {
                                       return result.outcome.Succeeded();
                                   });
    }
}

bool Session::SimulatorIsRunningAfter(const std::vector<LinkOperationResult>& results) const
{
    return SomethingChanged(results) && probe_.SimulatorIsRunning();
}

void Session::NoteLinkResults(const std::vector<LinkOperationResult>& results, const bool simulatorIsRunning)
{
    if (!SomethingChanged(results) || !simulatorIsRunning)
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
    ++profileChanges_;

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
    if (readingEntries_)
    {
        readEntriesAgain_ = true;
        return;
    }

    readingEntries_ = true;
    readEntriesAgain_ = false;

    runner_.Run(
        [this, stamp = StampForAnEntriesRead(), libraries = snapshot_.libraries]
        {
            entriesRead_ = service_.ReadEntries(stamp, libraries);
        },
        [this]
        {
            FinishTheRefresh();
        });
}

EntriesStamp Session::StampForAnEntriesRead() const
{
    return {.profile = profile_, .adoptions = snapshotsAdopted_};
}

void Session::AdoptTheEntriesRead(EntriesRead read)
{
    if (!TakeTheEntriesRead(read))
    {
        RefreshEntries();
        return;
    }

    ++snapshotsAdopted_;

    observer_.OnRefreshed();
}

namespace
{
    std::vector<std::filesystem::path> LibraryPathsOf(const SimulatorProfile& profile)
    {
        std::vector<std::filesystem::path> paths;
        paths.reserve(profile.libraries.size());

        for (const Library& library : profile.libraries)
        {
            paths.push_back(library.path);
        }

        std::ranges::sort(paths);

        return paths;
    }

    bool SameExternalOrigins(const std::vector<ExternalOrigin>& left, const std::vector<ExternalOrigin>& right)
    {
        return std::ranges::is_permutation(left, right,
                                           [](const ExternalOrigin& one, const ExternalOrigin& other)
                                           {
                                               return one.libraryId == other.libraryId
                                                   && one.relativePath == other.relativePath
                                                   && one.externalPath == other.externalPath;
                                           });
    }

    bool SameStartupEntries(const std::vector<StartupEntry>& left, const std::vector<StartupEntry>& right)
    {
        return std::ranges::equal(left, right,
                                  [](const StartupEntry& one, const StartupEntry& other)
                                  {
                                      return one.label == other.label && one.path == other.path
                                          && one.enabled == other.enabled;
                                  });
    }

    bool SameEntriesComeOutOf(const SimulatorProfile& left, const SimulatorProfile& right)
    {
        return left.id == right.id && left.destinations == right.destinations
            && LibraryPathsOf(left) == LibraryPathsOf(right)
            && SameExternalOrigins(left.externalOrigins, right.externalOrigins);
    }
}

bool Session::TakeTheEntriesRead(EntriesRead& read)
{
    const bool stillCurrent =
        read.stamp.adoptions == snapshotsAdopted_ && SameEntriesComeOutOf(read.stamp.profile, profile_);

    if (stillCurrent)
    {
        snapshot_.entries = std::move(read.entries);
        snapshot_.enabled = std::move(read.enabled);
        snapshot_.conflicts = std::move(read.conflicts);
        snapshot_.startupEntries = std::move(read.startupEntries);
    }

    return stillCurrent;
}

void Session::FinishTheRefresh()
{
    readingEntries_ = false;

    const bool again = readEntriesAgain_;
    readEntriesAgain_ = false;

    EntriesRead read = std::exchange(entriesRead_, {});

    if (TakeTheEntriesRead(read))
    {
        observer_.OnRefreshed();
    }

    if (again)
    {
        RefreshEntries();
    }
}

void Session::RefreshStartupEntries()
{
    std::vector<StartupEntry> entries = service_.StartupEntriesNow();

    if (SameStartupEntries(entries, snapshot_.startupEntries))
    {
        return;
    }

    snapshot_.startupEntries = std::move(entries);
    ++snapshotsAdopted_;

    observer_.OnRefreshed();
}

SimulatorProfile Session::LatestProfile() const
{
    const SimulatorProfile* saved = ProfileById(settings_, profile_.id);

    return saved != nullptr ? *saved : profile_;
}

bool Session::WouldAcceptLibrary(const std::filesystem::path& path) const
{
    return LibraryContaining(profile_, path) == nullptr;
}

Session::LibraryRegistration Session::BeginRegistration() const
{
    return {.profile = LatestProfile(), .report = {}, .profileChanges = profileChanges_};
}

Session::LibraryRegistration Session::RegisterLibraryOn(LibraryRegistration started,
                                                        const std::filesystem::path& path) const
{
    started.report = service_.RegisterLibrary(started.profile, path);

    if (started.report.Accepted())
    {
        const auto registered = std::ranges::find_if(started.profile.libraries,
                                                     [&path](const Library& library)
                                                     {
                                                         return ComparablePath(library.path) == ComparablePath(path);
                                                     });

        if (registered != started.profile.libraries.end())
        {
            static_cast<void>(organizer_.AdoptTheStructure(started.profile, *registered));
        }
    }

    return started;
}

bool Session::AdoptTheRegistration(LibraryRegistration registered)
{
    if (registered.profileChanges != profileChanges_)
    {
        return false;
    }

    if (registered.report.Accepted())
    {
        Save(registered.profile);
        Scan(std::move(registered.profile));
    }

    return true;
}

LibraryGrouping Session::HowTheLibraryIsGrouped(const LibraryId& libraryId) const
{
    const Library* library = LibraryNamed(profile_, libraryId);

    return library == nullptr ? LibraryGrouping{} : organizer_.HowItIsGrouped(*library);
}

std::vector<FileOperationResult> Session::AdoptTheStructureOf(const LibraryId& libraryId)
{
    SimulatorProfile latest = LatestProfile();

    const Library* library = LibraryNamed(latest, libraryId);
    if (library == nullptr)
    {
        return {};
    }

    std::vector<FileOperationResult> results = organizer_.AdoptTheStructure(latest, *library);
    Scan(std::move(latest));

    return results;
}

std::vector<FileOperationResult> Session::TakeBackTheMarkersOf(const LibraryId& libraryId)
{
    SimulatorProfile latest = LatestProfile();

    const Library* library = LibraryNamed(latest, libraryId);
    if (library == nullptr)
    {
        return {};
    }

    std::vector<FileOperationResult> results = organizer_.TakeBackEveryMarkerItWrote(latest, *library);
    Scan(std::move(latest));

    return results;
}

void Session::RememberWhatCameFromAnotherProgram(const std::vector<ImportOperationResult>& results)
{
    SimulatorProfile next = LatestProfile();
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

    Keep(std::move(next));
}

void Session::ForgetWhatCameFromAnotherProgram(const std::vector<std::filesystem::path>& addonFolders)
{
    if (addonFolders.empty())
    {
        return;
    }

    SimulatorProfile next = LatestProfile();

    for (const std::filesystem::path& addonFolder : addonFolders)
    {
        ForgetWhereItCameFrom(next, addonFolder);
    }

    Keep(std::move(next));
}

Session::LegacyImport Session::BeginLegacyImport() const
{
    return {.profile = LatestProfile(), .report = {}, .profileChanges = profileChanges_};
}

Session::LegacyImport Session::ImportLegacyOn(LegacyImport started, const LegacyImportRequest& request) const
{
    SimulatorProfile& profile = started.profile;
    LegacyImportReport& report = started.report;

    for (const std::filesystem::path& root : request.libraryRoots)
    {
        if (service_.RegisterLibrary(profile, root).Accepted())
        {
            ++report.librariesRegistered;
            continue;
        }

        report.refused.push_back(root);
    }

    for (const std::filesystem::path& category : request.categories)
    {
        if (Succeeded(organizer_.DeclareCategory(profile, category).result))
        {
            ++report.categoriesDeclared;
            continue;
        }

        report.refused.push_back(category);
    }

    return started;
}

bool Session::AdoptTheLegacyImport(LegacyImport imported)
{
    if (imported.profileChanges != profileChanges_)
    {
        return false;
    }

    if (imported.report.librariesRegistered == 0 && imported.report.categoriesDeclared == 0)
    {
        return true;
    }

    service_.ForgetUndo();
    Save(imported.profile);
    Scan(std::move(imported.profile));

    return true;
}

void Session::UnregisterLibrary(const LibraryId& libraryId)
{
    SimulatorProfile next = LatestProfile();
    ::UnregisterLibrary(next, libraryId);

    service_.ForgetUndo();
    Save(next);
    Scan(std::move(next));
}

void Session::RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to)
{
    SimulatorProfile next = LatestProfile();
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
    SimulatorProfile next = LatestProfile();
    const std::size_t before = next.destinationOverrides.size();
    ::DropOverridesPointingNowhere(next);

    if (next.destinationOverrides.size() == before)
    {
        return;
    }

    service_.ForgetUndo();
    Keep(std::move(next));
    RefreshEntries();
}

void Session::OverrideDestination(const std::vector<const TreeNode*>& nodes, const std::filesystem::path& destination)
{
    SimulatorProfile next = LatestProfile();
    bool remembered = false;

    for (const TreeNode* node : nodes)
    {
        remembered = RememberTheDestination(next, *node, destination) || remembered;
    }

    if (!remembered)
    {
        return;
    }

    service_.ForgetUndo();
    Keep(std::move(next));

    observer_.OnRefreshed();
}

FileOperationResult Session::CreateCategory(const std::filesystem::path& parent, const std::string& name)
{
    SimulatorProfile latest = LatestProfile();
    const FileOperationResult result = organizer_.CreateCategory(latest, parent, name);

    if (Succeeded(result.result))
    {
        service_.ForgetUndo();
        Scan(std::move(latest));
    }

    return result;
}

FileOperationResult Session::CheckRenameCategory(const std::filesystem::path& category, const std::string& name) const
{
    return organizer_.CheckRenameCategory(profile_, category, name);
}

Session::ReorganizedLibrary Session::BeginReorganization() const
{
    return {.profile = LatestProfile(), .results = {}, .carried = {}};
}

Session::ReorganizedLibrary Session::MoveAddonsOn(ReorganizedLibrary started, const std::vector<AddonMove>& moves) const
{
    started.results = organizer_.Move(started.profile, moves);

    for (std::size_t index = 0; index < moves.size() && index < started.results.size(); ++index)
    {
        if (TheFolderLanded(started.results[index].result))
        {
            started.carried.push_back({.from = moves[index].addonFolder, .to = started.results[index].path});
        }
    }

    return started;
}

Session::ReorganizedLibrary Session::RenameCategoryOn(ReorganizedLibrary started,
                                                      const std::filesystem::path& category,
                                                      const std::string& name) const
{
    FileOperationResult result = organizer_.RenameCategory(started.profile, category, name);

    if (TheFolderLanded(result.result))
    {
        started.carried.push_back({.from = category, .to = result.path});
    }

    started.results = {std::move(result)};

    return started;
}

Session::ReorganizedLibrary Session::RemoveCategoryOn(ReorganizedLibrary started,
                                                      const std::filesystem::path& category) const
{
    FileOperationResult result = organizer_.RemoveCategory(started.profile, category);

    if (Succeeded(result.result))
    {
        started.carried.push_back({.from = category, .to = {}});
    }

    started.results = {std::move(result)};

    return started;
}

void Session::AdoptTheReorganization(const ReorganizedLibrary& reorganized)
{
    if (reorganized.carried.empty())
    {
        return;
    }

    SimulatorProfile next = LatestProfile();

    for (const FolderCarried& folder : reorganized.carried)
    {
        const Library* library = LibraryContaining(next, folder.from);
        if (library == nullptr)
        {
            continue;
        }

        if (folder.to.empty())
        {
            ForgetTheFolder(next, *library, folder.from);
            continue;
        }

        CarryTheFolder(next, *library, folder.from, folder.to);
    }

    service_.ForgetUndo();
    Save(next);
    Scan(std::move(next));
}

void Session::Keep(SimulatorProfile next)
{
    Save(next);

    if (running_)
    {
        Scan(std::move(next));
        return;
    }

    profile_ = std::move(next);
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

    ++snapshotsAdopted_;

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
    ++profileChanges_;

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
