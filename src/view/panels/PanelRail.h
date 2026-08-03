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

    void ShowTitle(const QString& title, bool alarming);

signals:
    void ExpandRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi() const;

private:
    QToolButton* expand_ = nullptr;
    QString title_;
    bool alarming_ = false;
};

#endif // FS_ORGANIZER_VIEW_PANELS_PANEL_RAIL_H
