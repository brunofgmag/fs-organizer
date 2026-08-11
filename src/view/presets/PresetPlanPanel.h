#ifndef FS_ORGANIZER_VIEW_PRESETS_PRESET_PLAN_PANEL_H
#define FS_ORGANIZER_VIEW_PRESETS_PRESET_PLAN_PANEL_H

#include <QtWidgets/QWidget>

#include "domain/preset/PresetPlan.h"
#include "viewmodel/PresetViewModel.h"

class QButtonGroup;
class QLabel;
class QPushButton;

struct PresetPlanState
{
    QString planFor{};
    PresetPreview preview{};
    bool holdsOne = false;
    bool governsStartup = false;
};

class PresetPlanPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PresetPlanPanel(QWidget* parent = nullptr);

    [[nodiscard]] ApplyMode Mode() const;

    void Show(const PresetPlanState& state);

signals:
    void ModeChanged();

    void ApplyRequested();

    void OmittedRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    [[nodiscard]] QWidget* CreateTheModeRow();

    [[nodiscard]] QWidget* CreateThePlanFields();

    [[nodiscard]] QWidget* CreateTheStartupSection();

    QLabel* planFor_ = nullptr;
    QLabel* applyAs_ = nullptr;
    QButtonGroup* modes_ = nullptr;
    QLabel* modeExplained_ = nullptr;
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
    QWidget* startupSection_ = nullptr;
    QLabel* startupSaid_ = nullptr;
    QPushButton* apply_ = nullptr;
};

[[nodiscard]] QString CountsSentenceFor(const PresetPreview& preview);

#endif // FS_ORGANIZER_VIEW_PRESETS_PRESET_PLAN_PANEL_H
