#ifndef FS_ORGANIZER_VIEW_PRESETS_PAGE_H
#define FS_ORGANIZER_VIEW_PRESETS_PAGE_H

#include <optional>

#include <QtWidgets/QWidget>

#include "viewmodel/PresetViewModel.h"
#include "viewmodel/SessionNotifier.h"

class EmptyState;
class PresetPlanPanel;
class QCheckBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

class PresetsPage final : public QWidget
{
    Q_OBJECT

public:
    PresetsPage(PresetViewModel& viewModel, const SessionNotifier& notifier, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

    void SummaryChanged(const QString& summary);

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    [[nodiscard]] QTableWidget* CreateNameTable();

    [[nodiscard]] QTableWidget* CreateReturnTable();

    [[nodiscard]] QWidget* CreateTheTwoHalves();

    [[nodiscard]] QWidget* CreateTheStartupTab();

    [[nodiscard]] QString SelectedName() const;

    [[nodiscard]] ApplyMode Mode() const;

    void ReloadNames();

    void ShowTheWayBack();

    [[nodiscard]] int HideTheNamesThatDoNotMatch() const;

    void ShowOnlyTheNamesThatMatch() const;

    void LetGoOf(QTableWidget* table);

    void ShowSelected();

    void ShowTheReturnPreset();

    void ShowEntries();

    void ShowStartupTab();

    void ActionToggled(const QTableWidgetItem* item);

    void StartupActionToggled(const QTableWidgetItem* item);

    void RecaptureStartup();

    void GovernStartupToggled(bool governs);

    void RefreshPreview() const;

    void CreateFromWhatIsEnabled();

    void UpdateFromWhatIsEnabled() const;

    void RenameSelected();

    void RemoveSelected();

    void ApplySelected();

    void ListTheOmitted();

    void GoBack();

    PresetViewModel& viewModel_;
    QStackedWidget* pages_ = nullptr;
    QPushButton* content_ = nullptr;
    QPushButton* plan_ = nullptr;
    QTableWidget* names_ = nullptr;
    QTableWidget* return_ = nullptr;
    QFrame* returnRule_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QTableWidget* entries_ = nullptr;
    QPushButton* startup_ = nullptr;
    QCheckBox* governsStartup_ = nullptr;
    QStackedWidget* startupBody_ = nullptr;
    QLabel* startupEmpty_ = nullptr;
    QTableWidget* startupEntries_ = nullptr;
    QPushButton* updateStartup_ = nullptr;
    PresetPlanPanel* planPanel_ = nullptr;
    QPushButton* update_ = nullptr;
    QPushButton* rename_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* create_ = nullptr;
    QPushButton* goBack_ = nullptr;
    EmptyState* nothing_ = nullptr;
    QPushButton* nothingAction_ = nullptr;
    std::optional<Preset> selected_;
    bool showingReturn_ = false;
    bool populating_ = false;
};

#endif // FS_ORGANIZER_VIEW_PRESETS_PAGE_H
