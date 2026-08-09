#ifndef FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_PAGE_H
#define FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_PAGE_H

#include <filesystem>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include "viewmodel/StartupViewModel.h"

class EmptyState;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

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
    };

    struct WhatTheRowAsks
    {
        std::filesystem::path path{};
        bool enabled = false;
        QString label{};
    };

    [[nodiscard]] QWidget* CreateToolbar();

    [[nodiscard]] QWidget* CreateEntriesPane();

    void RetranslateUi() const;

    void ShowWhatTheFileSays();

    void FillTheTable() const;

    void DressTheToolbar() const;

    void Toggle(QTreeWidgetItem* row, int column);

    void Apply(const WhatTheRowAsks& asked);

    [[nodiscard]] bool TheSimulatorIsInTheWay();

    StartupViewModel& viewModel_;
    QWidget* toolbar_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    QTreeWidget* entries_ = nullptr;
    QPushButton* readAgain_ = nullptr;
    QPushButton* leaveAlone_ = nullptr;
    QLabel* readAt_ = nullptr;
    EmptyState* nothingToShow_ = nullptr;
    EmptyState* leftAlone_ = nullptr;
    QPushButton* turnOn_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SIMULATOR_STARTUP_PAGE_H
