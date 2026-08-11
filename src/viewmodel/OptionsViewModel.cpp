#include "viewmodel/OptionsViewModel.h"

#include <algorithm>
#include <optional>

#include <QtCore/QCoreApplication>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "viewmodel/SimulatorText.h"

OptionsViewModel::OptionsViewModel(Session& session,
                                   ProfileService& service,
                                   const SessionNotifier& notifier,
                                   QObject* parent)
    : QObject(parent), session_(session), service_(service)
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

bool OptionsViewModel::RemoveProfile(const std::string& profileId, const bool disablingWhatItLeftBehind)
{
    if (disablingWhatItLeftBehind && profileId == session_.Profile().id)
    {
        std::vector<const TreeNode*> everything;
        for (const TreeNode& library : session_.Snapshot().libraries)
        {
            everything.push_back(&library);
        }

        const std::vector<LinkOperationResult> results =
            service_.SetEnabled(session_.Profile(), session_.Snapshot(), everything, false).results;

        session_.NoteLinkResults(results);
        emit LinksDisabled(results);
    }

    if (!session_.RemoveProfile(profileId))
    {
        return false;
    }

    shown_.clear();

    emit Changed();

    return true;
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

const TreeNode* OptionsViewModel::TreeOf(const LibraryId& libraryId) const
{
    const SimulatorProfile& profile = session_.Profile();

    const auto known = std::ranges::find_if(profile.libraries,
                                            [&libraryId](const Library& library)
                                            {
                                                return library.id == libraryId;
                                            });

    return known == profile.libraries.end() ? nullptr : LibraryTreeAt(session_.Snapshot().libraries, known->path);
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

bool OptionsViewModel::VerifiesWithHash() const
{
    return session_.Settings().verifyWithHash;
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

LibraryReport OptionsViewModel::RegisterLibrary(const std::filesystem::path& path) const
{
    return session_.RegisterLibrary(path);
}

void OptionsViewModel::UnregisterLibrary(const LibraryId& libraryId, const bool disablingWhatItLeftBehind)
{
    if (disablingWhatItLeftBehind)
    {
        if (const TreeNode* tree = TreeOf(libraryId); tree != nullptr)
        {
            const std::vector<LinkOperationResult> results =
                service_.SetEnabled(session_.Profile(), session_.Snapshot(), {tree}, false).results;

            session_.NoteLinkResults(results);
            emit LinksDisabled(results);
        }
    }

    session_.UnregisterLibrary(libraryId);

    emit Changed();
}
