#ifndef FS_ORGANIZER_VIEW_MAIN_WINDOW_H
#define FS_ORGANIZER_VIEW_MAIN_WINDOW_H

#include <QtWidgets/QMainWindow>

#include "application/model/AppSettings.h"

class QComboBox;
class QStackedWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppSettings& settings, QWidget* parent = nullptr);

    void ShowProfiles(const AppSettings& settings);

signals:
    void AddProfileRequested();

protected:
    void showEvent(QShowEvent* event) override;

private:
    [[nodiscard]] QWidget* CreateHeader();

    void OnProfileActivated(int index);

    AppSettings settings_;
    QComboBox* profiles_ = nullptr;
    QStackedWidget* pages_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_MAIN_WINDOW_H
