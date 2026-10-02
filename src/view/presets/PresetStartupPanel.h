#ifndef FS_ORGANIZER_VIEW_PRESETS_PRESET_STARTUP_PANEL_H
#define FS_ORGANIZER_VIEW_PRESETS_PRESET_STARTUP_PANEL_H

#include <filesystem>

#include <QtWidgets/QWidget>

#include "viewmodel/PresetViewModel.h"

class EmptyState;
class QAction;
class QCheckBox;
class QHBoxLayout;
class QMenu;
class QPoint;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

struct PresetStartupState
{
    bool holdsOne = false;
    bool governs = false;
    bool readOnly = false;
    bool fileHoldsEntries = false;
    QList<PresetStartupRow> rows{};
    QList<PresetStartupCandidate> candidates{};
};

class PresetStartupPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PresetStartupPanel(QWidget* parent = nullptr);

    void Show(const PresetStartupState& state);

signals:
    void GovernToggled(bool governs);

    void RecaptureRequested();

    void ActionToggled(int row, PresetAction wanted);

    void EntryAddRequested(const std::filesystem::path& path);

    void RowTakeOutRequested(int row);

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    [[nodiscard]] QWidget* CreateTheLiveHalf();

    [[nodiscard]] QHBoxLayout* CreateTheGestures();

    [[nodiscard]] QTableWidgetItem* TargetCellOf(const PresetStartupRow& row) const;

    void FillTheRows(const QList<PresetStartupRow>& rows);

    void FillTheCandidateMenu();

    void RowChanged(const QTableWidgetItem* item);

    void OfferTheCandidates();

    void ChooseACandidate(const QAction* chosen);

    void OfferToTakeTheRowOut(const QPoint& where);

    void TakeTheSelectedRowOut();

    void ShowWhatTheGesturesCanDo();

    [[nodiscard]] int SelectedRow() const;

    QCheckBox* governs_ = nullptr;
    QStackedWidget* body_ = nullptr;
    EmptyState* empty_ = nullptr;
    QTableWidget* entries_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* takeOut_ = nullptr;
    QPushButton* update_ = nullptr;
    QMenu* candidatesMenu_ = nullptr;
    QMenu* rowMenu_ = nullptr;
    QAction* takeOutAction_ = nullptr;
    QList<PresetStartupCandidate> candidates_{};
    bool readOnly_ = false;
    bool fileHoldsEntries_ = false;
    bool populating_ = false;
};

#endif // FS_ORGANIZER_VIEW_PRESETS_PRESET_STARTUP_PANEL_H
