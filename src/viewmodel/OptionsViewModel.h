#ifndef FS_ORGANIZER_VIEWMODEL_OPTIONS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_OPTIONS_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/ProfileService.h"
#include "application/Session.h"
#include "application/model/UpdateMode.h"
#include "application/ports/BackgroundRunner.h"
#include "viewmodel/GuardedRunner.h"
#include "application/ports/SettingsRepository.h"
#include "domain/model/LibraryId.h"
#include "domain/tree/StructureAdoption.h"
#include "domain/model/LinkType.h"
#include "domain/model/Verification.h"
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
                     BackgroundRunner& runner,
                     const SessionNotifier& notifier,
                     QObject* parent = nullptr);

    [[nodiscard]] std::vector<ProfileLine> Profiles() const;

    void ShowProfile(const std::string& profileId);

    [[nodiscard]] SimulatorProfile ProfileShown() const;

    [[nodiscard]] bool ShowsTheProfileInUse() const;

    void RemoveProfile(const std::string& profileId, bool disablingWhatItLeftBehind);

    [[nodiscard]] std::size_t AddonsInTheActiveProfile() const;

    [[nodiscard]] std::size_t EnabledInTheProfileInUse() const;

    [[nodiscard]] std::vector<DestinationLine> Destinations() const;

    [[nodiscard]] std::vector<LibraryLine> Libraries() const;

    [[nodiscard]] LinkType TypeOfLink() const;

    [[nodiscard]] Verification VerificationUsed() const;

    void ChooseTypeOfLink(LinkType linkType);

    void ChooseVerification(Verification verification);

    void ChooseUpdateMode(UpdateMode mode);

    [[nodiscard]] std::string Language() const;

    void ChooseLanguage(const std::string& language);

    void RepointDestination(const std::filesystem::path& from, const std::filesystem::path& to) const;

    [[nodiscard]] bool WouldAcceptLibrary(const std::filesystem::path& path) const;

    void RegisterLibrary(const std::filesystem::path& path);

    void UnregisterLibrary(const LibraryId& libraryId, bool disablingWhatItLeftBehind);

    [[nodiscard]] LibraryGrouping GroupingOf(const LibraryId& libraryId) const;

    void DeclareTheCategoriesOf(const LibraryId& libraryId);

    void TakeBackTheMarkersOf(const LibraryId& libraryId);

signals:
    void Changed();

    void LibraryRegistered(const std::filesystem::path& path, const LibraryReport& report);

    void LinkTypeChosen(LinkType linkType);

    void VerificationChosen(Verification verification);

    void LanguageChosen(const QString& language);

    void LinksDisabled(const std::vector<LinkOperationResult>& results);

    void ProfileRemoved(const QString& label);

    void ProfileNotRemoved(const QString& label);

    void LibraryUnregistered(const QString& label);

    void SettingsCouldNotBeSaved();

private:
    struct DisablingWork
    {
        SimulatorProfile profile{};
        ProfileSnapshot snapshot{};
        std::vector<const TreeNode*> nodes{};
        std::vector<LinkOperationResult> results{};
        bool simulatorRunning = false;
    };

    bool Rewrite(const std::function<bool(AppSettings&)>& change);
    [[nodiscard]] const TreeNode* TreeOf(const LibraryId& libraryId, const std::vector<TreeNode>& libraries) const;
    [[nodiscard]] std::shared_ptr<DisablingWork> WorkOnTheProfileInUse() const;
    [[nodiscard]] QString LabelOfProfile(const std::string& profileId) const;
    [[nodiscard]] QString LabelOfLibrary(const LibraryId& libraryId) const;
    [[nodiscard]] bool WouldRemoveProfile(const std::string& profileId) const;
    void DisableThenRemove(const std::shared_ptr<DisablingWork>& work, std::function<void()> removal);
    void FinishRemovingProfile(const std::string& profileId, const QString& label);
    void FinishUnregistering(const LibraryId& libraryId, const QString& label);

    Session& session_;
    ProfileService& service_;
    GuardedRunner registering_;
    std::string shown_;
};

#endif // FS_ORGANIZER_VIEWMODEL_OPTIONS_VIEW_MODEL_H
