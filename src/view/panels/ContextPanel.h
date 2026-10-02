#ifndef FS_ORGANIZER_VIEW_PANELS_CONTEXT_PANEL_H
#define FS_ORGANIZER_VIEW_PANELS_CONTEXT_PANEL_H

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

class PanelRail;
class QHeaderView;
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

    void ShowTitle(const QString& title, bool alarming = false);

    void RenameTheFallback(const QString& title);

    void Summon(bool summoned);

    void LevelWith(QHeaderView* header);

signals:
    void CloseRequested();

protected:
    void changeEvent(QEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void RetranslateUi() const;

    void SetCollapsed(bool collapsed);

    void MatchTheColumnHeader();

    QWidget* header_ = nullptr;
    QWidget* body_ = nullptr;
    PanelRail* rail_ = nullptr;
    QLabel* title_ = nullptr;
    QToolButton* toggle_ = nullptr;
    QToolButton* close_ = nullptr;
    QVBoxLayout* content_ = nullptr;
    QPointer<QHeaderView> levelWith_;
    QString fallbackTitle_;
    bool showingFallback_ = true;
    int expandedWidth_ = 380;
    bool collapsed_ = false;
};

#endif // FS_ORGANIZER_VIEW_PANELS_CONTEXT_PANEL_H
