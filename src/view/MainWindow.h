#ifndef FS_ORGANIZER_VIEW_MAIN_WINDOW_H
#define FS_ORGANIZER_VIEW_MAIN_WINDOW_H

#include <string>

#include <QtWidgets/QMainWindow>

#include "application/model/AppSettings.h"

class QComboBox;
class QLabel;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppSettings& settings, QWidget* parent = nullptr);

    void ShowProfiles(const AppSettings& settings);

    QToolButton* AddPage(const QString& label, QWidget* page);

    void ShowStatus(const QString& message) const;

    void ShowRestartPending(bool pending) const;

signals:
    void AddProfileRequested();

    void ProfileChosen(const std::string& profileId);

    void PageSelected(QWidget* page);

protected:
    void showEvent(QShowEvent* event) override;

private:
    [[nodiscard]] QWidget* CreateHeader();

    void OnProfileActivated(int index);

    AppSettings settings_;
    QComboBox* profiles_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QVBoxLayout* navigation_ = nullptr;
    QLabel* restart_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_MAIN_WINDOW_H
