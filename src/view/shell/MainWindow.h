#ifndef FS_ORGANIZER_VIEW_SHELL_MAIN_WINDOW_H
#define FS_ORGANIZER_VIEW_SHELL_MAIN_WINDOW_H

#include <cstddef>
#include <string>

#include <QtCore/QHash>
#include <QtWidgets/QMainWindow>

#include "application/model/AppSettings.h"

class PageTab;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QStackedWidget;
class QTimer;
class TriageStrip;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppSettings& settings, QWidget* parent = nullptr);

    void ShowProfiles(const AppSettings& settings);

    PageTab* AddPage(const QString& label, QWidget* page);

    void CarryTriageOn(const QWidget* page);

    void ShowTriage(std::size_t broken, std::size_t conflicts, std::size_t duplicated, std::size_t unmanaged) const;

    void ShowStatus(const QString& message) const;

    void ShowRestartPending(bool pending) const;

    void ShowSummary(const QWidget* page, const QString& summary);

    void ShowAside(const QWidget* page, const QString& aside);

    void ShowMeter(const QWidget* page, int filled, int outOf);

signals:
    void AddProfileRequested();

    void ProfileChosen(const std::string& profileId);

    void PageSelected(QWidget* page);

    void RepairRequested();

    void ResolveRequested();

    void DuplicatesRequested();

    void ImportRequested();

protected:
    void showEvent(QShowEvent* event) override;

private:
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
};

#endif // FS_ORGANIZER_VIEW_SHELL_MAIN_WINDOW_H
