#ifndef FS_ORGANIZER_VIEW_DIAGNOSTICS_DIAGNOSTICS_PAGE_H
#define FS_ORGANIZER_VIEW_DIAGNOSTICS_DIAGNOSTICS_PAGE_H

#include <QtWidgets/QWidget>

#include "viewmodel/DiagnosticsViewModel.h"

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTreeWidget;

class DiagnosticsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticsPage(DiagnosticsViewModel& viewModel, QWidget* parent = nullptr);

signals:
    void SummaryChanged(const QString& summary);

    void StatusChanged(const QString& message);

    void QuarantineRequested();

    void RepairRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    enum Section : int
    {
        DestinationEntries = 0,
        BrokenAndUnavailable = 1,
        Quarantine = 2,
        SizeOnDisk = 3,
    };

    [[nodiscard]] QWidget* CreateToolbar();

    [[nodiscard]] QWidget* CreateRail();

    [[nodiscard]] QWidget* CreateCountsPane();

    [[nodiscard]] QWidget* CreateBrokenPane();

    [[nodiscard]] QWidget* CreateQuarantinePane();

    [[nodiscard]] QWidget* CreateSizePane();

    void RetranslateUi() const;

    void OpenSection(int section) const;

    void ShowWhatWasCounted();

    void ShowTheCounts() const;

    void ShowWhatIsTroubled() const;

    void ShowWhatTheQuarantineHolds() const;

    void ShowWhatWasMeasured() const;

    void ShowTheLongestPaths() const;

    void ShowProgress(const QString& folder, int measured, int total) const;

    void DressTheRail() const;

    void DressTheSizeToolbar() const;

    DiagnosticsViewModel& viewModel_;
    QListWidget* rail_ = nullptr;
    QStackedWidget* panes_ = nullptr;
    QPushButton* refresh_ = nullptr;
    QLabel* refreshedAt_ = nullptr;
    QTreeWidget* counts_ = nullptr;
    QTreeWidget* troubled_ = nullptr;
    QLabel* troubledPromise_ = nullptr;
    QPushButton* repair_ = nullptr;
    QLabel* quarantineWeight_ = nullptr;
    QLabel* quarantinePlaces_ = nullptr;
    QPushButton* openQuarantine_ = nullptr;
    QTreeWidget* sizes_ = nullptr;
    QLabel* longestPaths_ = nullptr;
    QLabel* sizeMeasuredAt_ = nullptr;
    QLabel* sizeCost_ = nullptr;
    QLabel* sizeProgress_ = nullptr;
    QProgressBar* sizeMeter_ = nullptr;
    QPushButton* measureAgain_ = nullptr;
    QPushButton* cancel_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_DIAGNOSTICS_DIAGNOSTICS_PAGE_H
