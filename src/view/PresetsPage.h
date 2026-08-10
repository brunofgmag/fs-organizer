#ifndef FS_ORGANIZER_VIEW_PRESETS_PAGE_H
#define FS_ORGANIZER_VIEW_PRESETS_PAGE_H

#include <optional>

#include <QtWidgets/QWidget>

#include "viewmodel/PresetViewModel.h"
#include "viewmodel/SessionNotifier.h"

class EmptyState;
class QButtonGroup;
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

    [[nodiscard]] QWidget* CreateThePlanFields();

    [[nodiscard]] QWidget* CreateTheModeRow();

    [[nodiscard]] QWidget* CreateThePlanHalf();

    [[nodiscard]] QWidget* CreateTheTwoHalves();

    [[nodiscard]] QWidget* CreateTheStartupSection();

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

    void ActionToggled(const QTableWidgetItem* item);

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
    QLabel* planFor_ = nullptr;
    QTableWidget* names_ = nullptr;
    QTableWidget* return_ = nullptr;
    QFrame* returnRule_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QButtonGroup* modes_ = nullptr;
    QLabel* modeExplained_ = nullptr;
    QTableWidget* entries_ = nullptr;
    QLabel* planTitle_ = nullptr;
    QLabel* toEnableName_ = nullptr;
    QLabel* toEnable_ = nullptr;
    QLabel* toDisableName_ = nullptr;
    QLabel* toDisable_ = nullptr;
    QLabel* alreadyName_ = nullptr;
    QLabel* already_ = nullptr;
    QLabel* unresolvedName_ = nullptr;
    QLabel* unresolved_ = nullptr;
    QLabel* notNamedName_ = nullptr;
    QLabel* notNamed_ = nullptr;
    QLabel* notAppliedName_ = nullptr;
    QLabel* notApplied_ = nullptr;
    QLabel* omittedNote_ = nullptr;
    QPushButton* showOmitted_ = nullptr;
    QCheckBox* governsStartup_ = nullptr;
    QWidget* startupSection_ = nullptr;
    QLabel* startupSaid_ = nullptr;
    QPushButton* apply_ = nullptr;
    QPushButton* update_ = nullptr;
    QPushButton* rename_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* create_ = nullptr;
    QPushButton* goBack_ = nullptr;
    QLabel* applyAs_ = nullptr;
    EmptyState* nothing_ = nullptr;
    QPushButton* nothingAction_ = nullptr;
    std::optional<Preset> selected_;
    bool showingReturn_ = false;
    bool populating_ = false;
};

#endif // FS_ORGANIZER_VIEW_PRESETS_PAGE_H
