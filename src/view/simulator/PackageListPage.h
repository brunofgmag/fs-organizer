#ifndef FS_ORGANIZER_VIEW_SIMULATOR_PACKAGE_LIST_PAGE_H
#define FS_ORGANIZER_VIEW_SIMULATOR_PACKAGE_LIST_PAGE_H

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include "viewmodel/CoverageViewModel.h"

class EmptyState;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

class PackageListPage final : public QWidget
{
    Q_OBJECT

public:
    explicit PackageListPage(CoverageViewModel& viewModel, QWidget* parent = nullptr);

signals:
    void SummaryChanged(const QString& summary);

    void StatusChanged(const QString& message);

protected:
    void changeEvent(QEvent* event) override;

private:
    enum SecondHalf : int
    {
        ThePackagesYouTurnedOff = 0,
        LeftAlone = 1,
    };

    [[nodiscard]] QWidget* CreateToolbar();

    [[nodiscard]] QWidget* CreateHalves();

    void RetranslateUi() const;

    void ShowWhatTheListSays();

    void FillTheConflicts() const;

    void FillTheTurnedOff() const;

    void DressTheToolbar() const;

    void DressTheActions() const;

    [[nodiscard]] const CoverageLine* TheChosenConflict() const;

    void TurnTheSimulatorsOneOff();

    void LetThemCoexist();

    void TurnItBackOn();

    [[nodiscard]] bool TheSimulatorIsInTheWay();

    void Report(FileResult result, const QString& done);

    CoverageViewModel& viewModel_;
    QStackedWidget* secondHalf_ = nullptr;
    QTreeWidget* conflicts_ = nullptr;
    QTreeWidget* turnedOff_ = nullptr;
    QLabel* conflictsHeading_ = nullptr;
    QLabel* conflictsPromise_ = nullptr;
    QLabel* turnedOffHeading_ = nullptr;
    QLabel* readAt_ = nullptr;
    QPushButton* turnOff_ = nullptr;
    QPushButton* coexist_ = nullptr;
    QPushButton* turnBackOn_ = nullptr;
    QPushButton* leaveAlone_ = nullptr;
    QPushButton* turnOn_ = nullptr;
    EmptyState* leftAlone_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_SIMULATOR_PACKAGE_LIST_PAGE_H
