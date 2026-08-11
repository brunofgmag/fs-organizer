#ifndef FS_ORGANIZER_VIEW_SIMULATOR_SIMULATOR_PAGE_H
#define FS_ORGANIZER_VIEW_SIMULATOR_SIMULATOR_PAGE_H

#include <QtCore/QString>
#include <QtWidgets/QWidget>

class QPushButton;
class QStackedWidget;

class SimulatorPage final : public QWidget
{
    Q_OBJECT

public:
    SimulatorPage(QWidget* startup, QWidget* packages, QWidget* parent = nullptr);

    void ShowTheStartupEntries() const;

    void ShowThePackageList() const;

    void CarrySummaryFrom(const QWidget* panel, const QString& summary);

signals:
    void SummaryChanged(const QString& summary);

    void PanelChanged(QWidget* panel);

protected:
    void changeEvent(QEvent* event) override;

private:
    enum Panel : int
    {
        StartupEntries = 0,
        PackageList = 1,
    };

    void Open(int panel) const;

    void RetranslateUi() const;

    QStackedWidget* panels_ = nullptr;
    QPushButton* startup_ = nullptr;
    QPushButton* packages_ = nullptr;
    QString startupSummary_;
    QString packageSummary_;
};

#endif // FS_ORGANIZER_VIEW_SIMULATOR_SIMULATOR_PAGE_H
