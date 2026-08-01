#ifndef FS_ORGANIZER_VIEWMODEL_OPTIONS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_OPTIONS_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/model/UpdateMode.h"
#include "application/ports/SettingsRepository.h"
#include "domain/model/LibraryId.h"
#include "domain/model/LinkType.h"
#include "viewmodel/SessionNotifier.h"

struct ProfileLine
{
    std::string id;
    QString label;
    std::size_t destinations = 0;
    std::size_t libraries = 0;
    bool active = false;
};

struct DestinationLine
{
    std::filesystem::path path;
    bool isDefault = false;
};

struct LibraryLine
{
    LibraryId id;
    QString label;
    std::filesystem::path path;
    std::size_t categories = 0;
    std::size_t addons = 0;
    std::size_t enabled = 0;
    bool counted = false;
};

class OptionsViewModel final : public QObject
{
    Q_OBJECT

public:
    OptionsViewModel(Session& session,
                     ProfileService& service,
                     SettingsRepository& settings,
                     const SessionNotifier& notifier,
                     QObject* parent = nullptr);

    [[nodiscard]] std::vector<ProfileLine> Profiles() const;

    void ShowProfile(const std::string& profileId);

    [[nodiscard]] SimulatorProfile ProfileShown() const;

    [[nodiscard]] bool ShowsTheProfileInUse() const;

    [[nodiscard]] bool RemoveProfile(const std::string& profileId, bool disablingWhatItLeftBehind);

    [[nodiscard]] std::size_t AddonsInTheActiveProfile() const;

    [[nodiscard]] std::size_t EnabledInTheProfileInUse() const;

    [[nodiscard]] std::vector<DestinationLine> Destinations() const;

    [[nodiscard]] std::vector<LibraryLine> Libraries() const;

    [[nodiscard]] LinkType TypeOfLink() const;

    [[nodiscard]] bool VerifiesWithHash() const;

    void ChooseTypeOfLink(LinkType linkType);

    void ChooseUpdateMode(UpdateMode mode);

    void RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to) const;

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path) const;

    void UnregisterLibrary(const LibraryId& libraryId, bool disablingWhatItLeftBehind);

signals:
    void Changed();

    void LinkTypeChosen(LinkType linkType);

    void LinksDisabled(const std::vector<LinkOperationResult>& results);

    void SettingsCouldNotBeSaved();

private:
    [[nodiscard]] const TreeNode* TreeOf(const LibraryId& libraryId) const;

    Session& session_;
    ProfileService& service_;
    SettingsRepository& settings_;
    std::string shown_;
};

#endif // FS_ORGANIZER_VIEWMODEL_OPTIONS_VIEW_MODEL_H
