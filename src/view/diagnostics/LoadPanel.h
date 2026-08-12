#ifndef FS_ORGANIZER_VIEW_DIAGNOSTICS_LOAD_PANEL_H
#define FS_ORGANIZER_VIEW_DIAGNOSTICS_LOAD_PANEL_H

#include <QtWidgets/QWidget>

#include "application/LoadReport.h"

class QLabel;
class QStackedWidget;
class QTreeWidget;

class LoadPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit LoadPanel(QWidget* parent = nullptr);

    void Show(const LoadDiagnostics& load);

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    void ShowWhatTheReportAttributes() const;

    QLabel* refusal_ = nullptr;
    QLabel* registered_ = nullptr;
    QStackedWidget* body_ = nullptr;
    QLabel* empty_ = nullptr;
    QTreeWidget* modules_ = nullptr;
    LoadDiagnostics load_{};
};

#endif // FS_ORGANIZER_VIEW_DIAGNOSTICS_LOAD_PANEL_H
