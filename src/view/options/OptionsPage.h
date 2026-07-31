#ifndef FS_ORGANIZER_VIEW_OPTIONS_OPTIONS_PAGE_H
#define FS_ORGANIZER_VIEW_OPTIONS_OPTIONS_PAGE_H

#include <filesystem>

#include <QtWidgets/QWidget>

#include "viewmodel/OptionsViewModel.h"

class QButtonGroup;
class QListWidget;
class QStackedWidget;
class QVBoxLayout;

class OptionsPage final : public QWidget
{
    Q_OBJECT

public:
    OptionsPage(OptionsViewModel& viewModel, std::filesystem::path settingsFile, QWidget* parent = nullptr);

    void Reload();

signals:
    void StatusChanged(const QString& message);

    void SummaryChanged(const QString& summary);

    void AddProfileRequested();

    void ProfileChosen(const std::string& profileId);

private:
    [[nodiscard]] QWidget* CreateNavigation();

    [[nodiscard]] QWidget* CreateProfilesAndLibraries();

    [[nodiscard]] QWidget* CreateLinks();

    [[nodiscard]] QWidget* Choice(const QString& name, const QString& explanation, QWidget* control, bool follows);

    [[nodiscard]] QWidget* CreateAbout();

    [[nodiscard]] QWidget* CreateWaitingOn(const QString& heading, const QString& explanation);

    void ReloadProfiles();

    void ReloadDestinations();

    void ReloadLibraries();

    void Repoint(const std::filesystem::path& destination);

    void AddLibrary();

    void Unregister(const LibraryLine& library);

    OptionsViewModel& viewModel_;
    std::filesystem::path settingsFile_;
    QListWidget* navigation_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    QVBoxLayout* profiles_ = nullptr;
    QVBoxLayout* destinations_ = nullptr;
    QVBoxLayout* libraries_ = nullptr;
    QButtonGroup* linkTypes_ = nullptr;
    QButtonGroup* profileChoices_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_OPTIONS_OPTIONS_PAGE_H
