#ifndef FS_ORGANIZER_VIEW_PAINTED_RULE_H
#define FS_ORGANIZER_VIEW_PAINTED_RULE_H

#include <QtWidgets/QWidget>

class PaintedRule final : public QWidget
{
public:
    explicit PaintedRule(QWidget* parent);

protected:
    void paintEvent(QPaintEvent* event) override;
};

#endif // FS_ORGANIZER_VIEW_PAINTED_RULE_H
