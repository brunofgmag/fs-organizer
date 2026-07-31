#ifndef FS_ORGANIZER_VIEW_PANELS_CONTEXT_PANEL_H
#define FS_ORGANIZER_VIEW_PANELS_CONTEXT_PANEL_H

#include <QtWidgets/QWidget>

class PanelRail;
class QLabel;
class QToolButton;
class QVBoxLayout;

class ContextPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ContextPanel(const QString& title, int expandedWidth = 380, QWidget* parent = nullptr);

    void Add(QWidget* widget) const;

    void RestoreCollapsedState();

    void ShowBadge(const QString& text) const;

    void ShowTitle(const QString& title, bool alarming = false) const;

    void Summon(bool summoned);

signals:
    void CloseRequested();

private:
    void SetCollapsed(bool collapsed);

    QWidget* header_ = nullptr;
    QWidget* body_ = nullptr;
    PanelRail* rail_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* badge_ = nullptr;
    QToolButton* toggle_ = nullptr;
    QToolButton* close_ = nullptr;
    QVBoxLayout* content_ = nullptr;
    QString fallbackTitle_;
    int expandedWidth_ = 380;
    bool collapsed_ = false;
};

#endif // FS_ORGANIZER_VIEW_PANELS_CONTEXT_PANEL_H
