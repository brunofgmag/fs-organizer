#ifndef FS_ORGANIZER_VIEW_PRESETS_PAGE_H
#define FS_ORGANIZER_VIEW_PRESETS_PAGE_H

#include <optional>

#include <QtWidgets/QWidget>

#include "viewmodel/PresetViewModel.h"
#include "viewmodel/SessionNotifier.h"

class ContextPanel;
class QButtonGroup;
class QLabel;
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

private:
    [[nodiscard]] QTableWidget* CreateNameTable();

    [[nodiscard]] QString SelectedName() const;

    [[nodiscard]] ApplyMode Mode() const;

    void ReloadNames();

    void ShowSelected();

    void ActionToggled(const QTableWidgetItem* item);

    void RefreshPreview() const;

    void CreateFromWhatIsEnabled();

    void UpdateFromWhatIsEnabled() const;

    void RenameSelected();

    void RemoveSelected();

    void ApplySelected();

    PresetViewModel& viewModel_;
    QStackedWidget* pages_ = nullptr;
    ContextPanel* panel_ = nullptr;
    QTableWidget* names_ = nullptr;
    QButtonGroup* modes_ = nullptr;
    QLabel* modeExplained_ = nullptr;
    QTableWidget* entries_ = nullptr;
    QLabel* preview_ = nullptr;
    QPushButton* apply_ = nullptr;
    QPushButton* update_ = nullptr;
    QPushButton* rename_ = nullptr;
    QPushButton* remove_ = nullptr;
    std::optional<Preset> selected_;
    bool populating_ = false;
};

#endif // FS_ORGANIZER_VIEW_PRESETS_PAGE_H
