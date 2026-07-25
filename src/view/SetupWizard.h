#ifndef FS_ORGANIZER_VIEW_SETUP_WIZARD_H
#define FS_ORGANIZER_VIEW_SETUP_WIZARD_H

#include <QtWidgets/QWizard>

#include "viewmodel/SetupViewModel.h"

class QComboBox;
class QListWidget;

class SetupWizard final : public QWizard
{
    Q_OBJECT

public:
    explicit SetupWizard(SetupViewModel& viewModel, QWidget* parent = nullptr);

private:
    [[nodiscard]] QWizardPage* CreateSimulatorPage();
    [[nodiscard]] QWizardPage* CreateLibraryPage();

    void BrowseForDestination();

    [[nodiscard]] bool ConfirmDestination(const std::filesystem::path& path);
    void BrowseForLibrary();
    void RefreshLibraries() const;

    void RefreshCandidates() const;

    SetupViewModel& viewModel_;
    QComboBox* variant_ = nullptr;
    QListWidget* simulators_ = nullptr;
    QListWidget* libraries_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SETUP_WIZARD_H
