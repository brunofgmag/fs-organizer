#ifndef FS_ORGANIZER_VIEW_OPTIONS_OPTIONS_PAGE_H
#define FS_ORGANIZER_VIEW_OPTIONS_OPTIONS_PAGE_H

#include <filesystem>

#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

#include "viewmodel/OptionsViewModel.h"
#include "viewmodel/UpdateViewModel.h"

class QButtonGroup;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class OptionsPage final : public QWidget
{
    Q_OBJECT

public:
    OptionsPage(OptionsViewModel& viewModel,
                UpdateViewModel& updates,
                std::filesystem::path settingsFile,
                QWidget* parent = nullptr);

    void Reload();

signals:
    void StatusChanged(const QString& message);

    void SummaryChanged(const QString& summary);

    void AddProfileRequested();

    void LegacyImportRequested();

    void ProfileChosen(const std::string& profileId);

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    [[nodiscard]] QStringList PaneNames() const;

    void FillPanes();

    [[nodiscard]] QWidget* CreateNavigation();

    [[nodiscard]] QWidget* CreateProfilesAndLibraries();

    [[nodiscard]] QWidget* CreateLinks();

    [[nodiscard]] QWidget* Choice(const QString& name, const QString& explanation, QWidget* control, bool follows);

    [[nodiscard]] QWidget* Choice(const QString& name, QWidget* control, bool follows);

    [[nodiscard]] QWidget* CreateUpdates();

    void ReloadUpdates() const;

    [[nodiscard]] QWidget* CreateLanguage();

    void ReloadLanguage() const;

    [[nodiscard]] QWidget* CreateAbout();

    void ReloadProfiles();

    void ReloadDestinations();

    void ReloadLibraries();

    void Repoint(const std::filesystem::path& destination);

    void AddLibrary();

    void Unregister(const LibraryLine& library);

    void Remove(const ProfileLine& profile);

    OptionsViewModel& viewModel_;
    UpdateViewModel& updates_;
    std::filesystem::path settingsFile_;
    QListWidget* navigation_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    QVBoxLayout* profiles_ = nullptr;
    QVBoxLayout* destinations_ = nullptr;
    QVBoxLayout* libraries_ = nullptr;
    QLabel* destinationsHeading_ = nullptr;
    QLabel* librariesHeading_ = nullptr;
    QLabel* onlyForTheProfileInUse_ = nullptr;
    QPushButton* addLibrary_ = nullptr;
    QPushButton* importLegacy_ = nullptr;
    QButtonGroup* linkTypes_ = nullptr;
    QButtonGroup* verifications_ = nullptr;
    QButtonGroup* profileChoices_ = nullptr;
    QButtonGroup* updateModes_ = nullptr;
    QButtonGroup* languages_ = nullptr;
    QLabel* updateStatus_ = nullptr;
    QPushButton* checkForUpdates_ = nullptr;
    QPushButton* downloadUpdate_ = nullptr;
    QPushButton* applyUpdate_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_OPTIONS_OPTIONS_PAGE_H
