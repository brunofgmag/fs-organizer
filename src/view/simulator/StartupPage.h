#ifndef FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_PAGE_H
#define FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_PAGE_H

#include <filesystem>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include "viewmodel/StartupViewModel.h"

class EmptyState;
class QLabel;
class QPoint;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class UndoButton;

class StartupPage final : public QWidget
{
    Q_OBJECT

public:
    explicit StartupPage(StartupViewModel& viewModel, QWidget* parent = nullptr);

signals:
    void SummaryChanged(const QString& summary);

    void StatusChanged(const QString& message);

protected:
    void changeEvent(QEvent* event) override;

private:
    enum Pane : int
    {
        TheEntries = 0,
        NothingToShow = 1,
        LeftAlone = 2,
        TheRemoved = 3,
        NothingRemoved = 4,
    };

    struct WhatTheRowAsks
    {
        std::filesystem::path path{};
        bool enabled = false;
        QString label{};
    };

    [[nodiscard]] QWidget* CreateToolbar();

    [[nodiscard]] QWidget* CreateTheViews(QWidget* bar);

    [[nodiscard]] QWidget* CreateEntriesPane();

    [[nodiscard]] QWidget* CreateRemovedPane();

    void CreateShortcuts();

    void RetranslateUi() const;

    void ShowWhatTheFileSays();

    void FillTheTable() const;

    void FillTheRemoved() const;

    void DressTheToolbar() const;

    void DressTheViews() const;

    void DressTheUndo() const;

    void ChooseThePane();

    void Toggle(QTreeWidgetItem* row, int column);

    void Apply(const WhatTheRowAsks& asked);

    void Add();

    void EditSelected();

    void EditTheRowDoubleClicked(QTreeWidgetItem* row);

    void EditTheLine(const StartupLine& line);

    void RemoveSelected();

    void RestoreSelected();

    void DiscardSelected();

    void UndoLast();

    void ShowTheMenu(QTreeWidget* table, const QPoint& at);

    void Settle(const StartupGestureOutcome& outcome, const QString& done);

    [[nodiscard]] bool ConfirmTheRemoval(const StartupLine& line);

    [[nodiscard]] bool ConfirmTheDiscard(const StartupRemovedEntry& removed);

    [[nodiscard]] bool
    Confirmed(const QString& title, const QString& asked, const QString& explained, const QString& go);

    [[nodiscard]] const StartupLine* SelectedLine() const;

    [[nodiscard]] const StartupLine* LineAt(const QString& path) const;

    [[nodiscard]] const StartupRemovedEntry* SelectedRemoved() const;

    [[nodiscard]] bool TheSimulatorIsInTheWay();

    StartupViewModel& viewModel_;
    bool showingTheRemoved_ = false;
    QWidget* toolbar_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    QTreeWidget* entries_ = nullptr;
    QTreeWidget* removed_ = nullptr;
    QPushButton* readAgain_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* edit_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* restore_ = nullptr;
    QPushButton* discard_ = nullptr;
    UndoButton* undo_ = nullptr;
    QToolButton* inTheFile_ = nullptr;
    QToolButton* removedChip_ = nullptr;
    QPushButton* leaveAlone_ = nullptr;
    EmptyState* nothingToShow_ = nullptr;
    EmptyState* nothingRemoved_ = nullptr;
    EmptyState* leftAlone_ = nullptr;
    QPushButton* addTheFirst_ = nullptr;
    QPushButton* turnOn_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_PAGE_H
