#include "viewmodel/OptionsViewModel.h"

#include <algorithm>
#include <memory>

#include <QtCore/QCoreApplication>

#include "domain/profile/ProfileEdits.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "viewmodel/SimulatorText.h"

OptionsViewModel::OptionsViewModel(Session& session,
                                   ProfileService& service,
                                   BackgroundRunner& runner,
                                   const SessionNotifier& notifier,
                                   QObject* parent)
    : QObject(parent), session_(session), service_(service), registering_(runner)
{
    connect(&notifier, &SessionNotifier::ScanFinished, this, &OptionsViewModel::Changed);
}

std::vector<ProfileLine> OptionsViewModel::Profiles() const
{
    const AppSettings& settings = session_.Settings();
    const std::string& loaded = session_.Profile().id;

    std::vector<ProfileLine> lines;
    lines.reserve(settings.profiles.size());

    for (const SimulatorProfile& profile : settings.profiles)
    {
        lines.push_back(ProfileLine{.id = profile.id,
                                    .label = NameOf(profile.variant),
                                    .destinations = profile.destinations.size(),
                                    .libraries = profile.libraries.size(),
                                    .active = profile.id == loaded});
    }

    return lines;
}

void OptionsViewModel::ShowProfile(const std::string& profileId)
{
    shown_ = profileId;

    emit Changed();
}

SimulatorProfile OptionsViewModel::ProfileShown() const
{
    if (shown_.empty() || shown_ == session_.Profile().id)
    {
        return session_.Profile();
    }

    const AppSettings& settings = session_.Settings();
    const auto found = std::ranges::find_if(settings.profiles,
                                            [this](const SimulatorProfile& profile)
                                            {
                                                return profile.id == shown_;
                                            });

    return found == settings.profiles.end() ? session_.Profile() : *found;
}

bool OptionsViewModel::ShowsTheProfileInUse() const
{
    return ProfileShown().id == session_.Profile().id;
}

bool OptionsViewModel::Busy() const
{
    return registering_.Busy();
}

void OptionsViewModel::RunInTheBackground(std::function<void()> work, std::function<void()> done)
{
    registering_.Run(
        [this]
        {
            emit BusyChanged();
        },
        std::move(work),
        [this, done = std::move(done)]
        {
            emit BusyChanged();

            done();
        });
}

void OptionsViewModel::RemoveProfile(const std::string& profileId, const bool disablingWhatItLeftBehind)
{
    const QString label = LabelOfProfile(profileId);

    if (!WouldRemoveProfile(profileId))
    {
        emit ProfileNotRemoved(label);
        return;
    }

    if (!disablingWhatItLeftBehind || profileId != session_.Profile().id)
    {
        FinishRemovingProfile(profileId, label);
        return;
    }

    const std::shared_ptr<DisablingWork> work = WorkOnTheProfileInUse();
    for (const TreeNode& library : work->snapshot.libraries)
    {
        work->nodes.push_back(&library);
    }

    DisableThenRemove(work,
                      [this, profileId, label]
                      {
                          FinishRemovingProfile(profileId, label);
                      });
}

bool OptionsViewModel::WouldRemoveProfile(const std::string& profileId) const
{
    std::vector<SimulatorProfile> profiles = session_.Settings().profiles;

    return ::RemoveProfile(profiles, profileId);
}

QString OptionsViewModel::LabelOfProfile(const std::string& profileId) const
{
    const std::vector<ProfileLine> lines = Profiles();
    const auto found = std::ranges::find_if(lines,
                                            [&profileId](const ProfileLine& line)
                                            {
                                                return line.id == profileId;
                                            });

    return found == lines.end() ? QString{} : found->label;
}

QString OptionsViewModel::LabelOfLibrary(const LibraryId& libraryId) const
{
    const std::vector<Library>& libraries = session_.Profile().libraries;
    const auto found = std::ranges::find_if(libraries,
                                            [&libraryId](const Library& library)
                                            {
                                                return library.id == libraryId;
                                            });

    return found == libraries.end() ? QString{} : QString::fromStdString(found->label);
}

std::shared_ptr<OptionsViewModel::DisablingWork> OptionsViewModel::WorkOnTheProfileInUse() const
{
    auto work = std::make_shared<DisablingWork>();
    work->profile = session_.Profile();
    work->snapshot = session_.Snapshot();

    return work;
}

void OptionsViewModel::DisableThenRemove(const std::shared_ptr<DisablingWork>& work, std::function<void()> removal)
{
    RunInTheBackground(
        [this, work]
        {
            work->results = service_.SetEnabled(work->profile, work->snapshot, work->nodes, false).results;
            work->simulatorRunning = session_.SimulatorIsRunningAfter(work->results);
        },
        [this, work, removal = std::move(removal)]
        {
            session_.NoteLinkResults(work->results, work->simulatorRunning);
            emit LinksDisabled(work->results);

            removal();
        });
}

void OptionsViewModel::FinishRemovingProfile(const std::string& profileId, const QString& label)
{
    if (!session_.RemoveProfile(profileId))
    {
        emit Changed();
        return;
    }

    shown_.clear();

    emit Changed();
    emit ProfileRemoved(label);
}

std::size_t OptionsViewModel::AddonsInTheActiveProfile() const
{
    std::size_t addons = 0;

    for (const TreeNode& library : session_.Snapshot().libraries)
    {
        addons += CountAddons(library);
    }

    return addons;
}

std::size_t OptionsViewModel::EnabledInTheProfileInUse() const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const SimulatorProfile& profile = session_.Profile();

    const auto insideOneOfTheLibraries = [&profile](const DestinationEntry& entry)
    {
        return std::ranges::any_of(profile.libraries,
                                   [&entry](const Library& library)
                                   {
                                       return PathIsInside(entry.target, library.path);
                                   });
    };

    return static_cast<std::size_t>(std::ranges::count_if(snapshot.entries,
                                                          [&insideOneOfTheLibraries](const DestinationEntry& entry)
                                                          {
                                                              return CountsAsEnabled(entry.classification)
                                                                  && insideOneOfTheLibraries(entry);
                                                          }));
}

std::vector<DestinationLine> OptionsViewModel::Destinations() const
{
    const SimulatorProfile profile = ProfileShown();

    std::vector<DestinationLine> lines;
    lines.reserve(profile.destinations.size());

    for (const std::filesystem::path& destination : profile.destinations)
    {
        lines.push_back(
            DestinationLine{.path = destination,
                            .isDefault = ComparablePath(destination) == ComparablePath(profile.defaultDestination)});
    }

    return lines;
}

const TreeNode* OptionsViewModel::TreeOf(const LibraryId& libraryId, const std::vector<TreeNode>& libraries) const
{
    const SimulatorProfile& profile = session_.Profile();

    const auto known = std::ranges::find_if(profile.libraries,
                                            [&libraryId](const Library& library)
                                            {
                                                return library.id == libraryId;
                                            });

    return known == profile.libraries.end() ? nullptr : LibraryTreeAt(libraries, known->path);
}

std::vector<LibraryLine> OptionsViewModel::Libraries() const
{
    const SimulatorProfile profile = ProfileShown();
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const bool counted = ShowsTheProfileInUse();

    std::vector<LibraryLine> lines;
    lines.reserve(profile.libraries.size());

    for (const Library& library : profile.libraries)
    {
        LibraryLine line;
        line.id = library.id;
        line.label = QString::fromStdString(library.label);
        line.path = library.path;
        line.counted = counted;

        if (!counted)
        {
            lines.push_back(std::move(line));
            continue;
        }

        if (const TreeNode* tree = LibraryTreeAt(snapshot.libraries, library.path); tree != nullptr)
        {
            line.categories = CountCategoriesInside(*tree);
            line.addons = CountAddons(*tree);
        }

        line.enabled =
            static_cast<std::size_t>(std::ranges::count_if(snapshot.entries,
                                                           [&library](const DestinationEntry& entry)
                                                           {
                                                               return CountsAsEnabled(entry.classification)
                                                                   && PathIsInside(entry.target, library.path);
                                                           }));

        lines.push_back(std::move(line));
    }

    return lines;
}

LinkType OptionsViewModel::TypeOfLink() const
{
    return session_.Settings().linkType;
}

Verification OptionsViewModel::VerificationUsed() const
{
    return session_.Settings().verification;
}

void OptionsViewModel::ChooseTypeOfLink(const LinkType linkType)
{
    if (session_.Settings().linkType == linkType)
    {
        return;
    }

    if (!Rewrite(
            [linkType](AppSettings& settings)
            {
                settings.linkType = linkType;

                return true;
            }))
    {
        emit Changed();
        return;
    }

    service_.UseLinkType(linkType);

    emit LinkTypeChosen(linkType);
    emit Changed();
}

void OptionsViewModel::ChooseVerification(const Verification verification)
{
    if (session_.Settings().verification == verification)
    {
        return;
    }

    if (!Rewrite(
            [verification](AppSettings& settings)
            {
                settings.verification = verification;

                return true;
            }))
    {
        emit Changed();
        return;
    }

    emit VerificationChosen(verification);
    emit Changed();
}

bool OptionsViewModel::Rewrite(const std::function<bool(AppSettings&)>& change)
{
    bool asked = false;

    const bool written = session_.Rewrite(
        [&change, &asked](AppSettings& settings)
        {
            asked = change(settings);

            return asked;
        });

    if (asked && !written)
    {
        emit SettingsCouldNotBeSaved();
    }

    return written;
}

void OptionsViewModel::ChooseUpdateMode(const UpdateMode mode)
{
    static_cast<void>(Rewrite(
        [mode](AppSettings& settings)
        {
            if (settings.updateMode == mode)
            {
                return false;
            }

            settings.updateMode = mode;

            return true;
        }));
}

std::string OptionsViewModel::Language() const
{
    return session_.Settings().language;
}

void OptionsViewModel::ChooseLanguage(const std::string& language)
{
    const bool written = Rewrite(
        [&language](AppSettings& settings)
        {
            if (settings.language == language)
            {
                return false;
            }

            settings.language = language;

            return true;
        });

    if (written)
    {
        emit LanguageChosen(QString::fromStdString(language));
    }
}

void OptionsViewModel::RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to) const
{
    session_.RepointDestination(from, to);
}

LibraryGrouping OptionsViewModel::GroupingOf(const LibraryId& libraryId) const
{
    return session_.HowTheLibraryIsGrouped(libraryId);
}

void OptionsViewModel::DeclareTheCategoriesOf(const LibraryId& libraryId)
{
    static_cast<void>(session_.AdoptTheStructureOf(libraryId));

    emit Changed();
}

void OptionsViewModel::TakeBackTheMarkersOf(const LibraryId& libraryId)
{
    static_cast<void>(session_.TakeBackTheMarkersOf(libraryId));

    emit Changed();
}

bool OptionsViewModel::WouldAcceptLibrary(const std::filesystem::path& path) const
{
    return session_.WouldAcceptLibrary(path);
}

void OptionsViewModel::RegisterLibrary(const std::filesystem::path& path)
{
    auto registration = std::make_shared<Session::LibraryRegistration>(session_.BeginRegistration());

    RunInTheBackground(
        [this, registration, path]
        {
            *registration = session_.RegisterLibraryOn(std::move(*registration), path);
        },
        [this, registration, path]
        {
            const LibraryReport report = registration->report;

            if (!session_.AdoptTheRegistration(std::move(*registration)))
            {
                RegisterLibrary(path);
                return;
            }

            emit LibraryRegistered(path, report);
        });
}

void OptionsViewModel::UnregisterLibrary(const LibraryId& libraryId, const bool disablingWhatItLeftBehind)
{
    const QString label = LabelOfLibrary(libraryId);

    if (!disablingWhatItLeftBehind)
    {
        FinishUnregistering(libraryId, label);
        return;
    }

    const std::shared_ptr<DisablingWork> work = WorkOnTheProfileInUse();
    const TreeNode* tree = TreeOf(libraryId, work->snapshot.libraries);

    if (tree == nullptr)
    {
        FinishUnregistering(libraryId, label);
        return;
    }

    work->nodes = {tree};

    DisableThenRemove(work,
                      [this, libraryId, label]
                      {
                          FinishUnregistering(libraryId, label);
                      });
}

void OptionsViewModel::FinishUnregistering(const LibraryId& libraryId, const QString& label)
{
    session_.UnregisterLibrary(libraryId);

    emit Changed();
    emit LibraryUnregistered(label);
}
