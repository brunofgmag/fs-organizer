#ifndef FS_ORGANIZER_VIEW_PRESETS_PRESET_STARTUP_PANEL_H
#define FS_ORGANIZER_VIEW_PRESETS_PRESET_STARTUP_PANEL_H

#include <QtWidgets/QWidget>

#include "viewmodel/PresetViewModel.h"

class EmptyState;
class QCheckBox;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

struct PresetStartupState
{
    bool holdsOne = false;
    bool governs = false;
    bool readOnly = false;
    QList<PresetStartupRow> rows{};
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

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    [[nodiscard]] QWidget* CreateTheLiveHalf();

    void RowChanged(const QTableWidgetItem* item);

    QCheckBox* governs_ = nullptr;
    QStackedWidget* body_ = nullptr;
    EmptyState* empty_ = nullptr;
    QTableWidget* entries_ = nullptr;
    QPushButton* update_ = nullptr;
    bool populating_ = false;
};

#endif // FS_ORGANIZER_VIEW_PRESETS_PRESET_STARTUP_PANEL_H
