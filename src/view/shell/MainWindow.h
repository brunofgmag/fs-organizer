#ifndef FS_ORGANIZER_VIEW_SHELL_MAIN_WINDOW_H
#define FS_ORGANIZER_VIEW_SHELL_MAIN_WINDOW_H

#include <cstddef>
#include <string>

#include <QtCore/QHash>
#include <QtWidgets/QMainWindow>

#include "application/model/AppSettings.h"
#include "viewmodel/AttentionBreakdown.h"

class PageTab;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QToolButton;
class QStackedWidget;
class QTimer;
class TriageStrip;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppSettings& settings, QWidget* parent = nullptr);

    void ShowProfiles(const AppSettings& settings);

    PageTab* AddPage(const char* label, QWidget* page);

    void CarryOptionsOn(QWidget* page);

    void ShowOptions();

    void LeaveOptions();

    [[nodiscard]] bool ShowingOptions() const;

    void CarryTriageOn(const QWidget* page);

    void ShowTriage(const AttentionBreakdown& breakdown) const;

    void ShowStatus(const QString& message) const;

    void ShowRestartPending(bool pending);

    void WarnTheSimulatorIsOpen();

    void ShowSummary(const QWidget* page, const QString& summary);

    void ShowAside(const QWidget* page, const QString& aside);

    void ShowMeter(const QWidget* page, int filled, int outOf);

signals:
    void OptionsRequested();

    void OptionsLeft();

    void AddProfileRequested();

    void ProfileChosen(const std::string& profileId);

    void PageSelected(QWidget* page);

    void RepairRequested();

    void ResolveRequested();

    void DuplicatesRequested();

    void ImportRequested();

protected:
    void showEvent(QShowEvent* event) override;

    void changeEvent(QEvent* event) override;

private:
    void DressTheBackTab() const;

    void RetranslateUi();

    struct Meter
    {
        int filled = 0;
        int outOf = 0;
    };

    [[nodiscard]] QWidget* CreateHeader();

    [[nodiscard]] QWidget* CreateTabStrip();

    void CreateFooter();

    void LineTheFooterUpWithThePage() const;

    void DressTheFooterFor(const QWidget* page) const;

    void OnProfileActivated(int index);

    AppSettings settings_;
    QComboBox* profiles_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QHBoxLayout* tabs_ = nullptr;
    QToolButton* gear_ = nullptr;
    PageTab* back_ = nullptr;
    QWidget* options_ = nullptr;
    QWidget* behindTheOptions_ = nullptr;
    TriageStrip* triage_ = nullptr;
    QLabel* summary_ = nullptr;
    QLabel* aside_ = nullptr;
    QLabel* restart_ = nullptr;
    QProgressBar* meter_ = nullptr;
    QTimer* statusFades_ = nullptr;
    QHash<const QWidget*, QString> summaries_;
    QHash<const QWidget*, QString> asides_;
    QHash<const QWidget*, Meter> meters_;
    QHash<const QWidget*, bool> triaged_;
    QHash<const QWidget*, PageTab*> tabsByPage_;
    bool restartPending_ = false;
};

#endif // FS_ORGANIZER_VIEW_SHELL_MAIN_WINDOW_H
