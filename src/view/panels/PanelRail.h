#ifndef FS_ORGANIZER_VIEW_PANELS_PANEL_RAIL_H
#define FS_ORGANIZER_VIEW_PANELS_PANEL_RAIL_H

#include <QtWidgets/QWidget>

class QToolButton;

class PanelRail final : public QWidget
{
    Q_OBJECT

public:
    explicit PanelRail(QWidget* parent = nullptr);

    [[nodiscard]] static int Width();

    void ShowTitle(const QString& title);

signals:
    void ExpandRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QToolButton* expand_ = nullptr;
    QString title_;
};

#endif // FS_ORGANIZER_VIEW_PANELS_PANEL_RAIL_H
