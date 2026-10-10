#ifndef FS_ORGANIZER_VIEW_HEADINGS_OVER_THE_COLUMNS_H
#define FS_ORGANIZER_VIEW_HEADINGS_OVER_THE_COLUMNS_H

#include <vector>

#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>

class QGridLayout;
class QLabel;

class HeadingsOverTheColumns final : public QWidget
{
public:
    HeadingsOverTheColumns(const QStringList& headings, QWidget* parent);

    void ShapeTheGridUnder(QGridLayout& grid) const;

    void StandOver(const std::vector<QWidget*>& cells);

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void moveEvent(QMoveEvent* event) override;

private:
    void Follow();

    std::vector<QLabel*> labels_;
    std::vector<QPointer<QWidget>> cells_;
};

#endif // FS_ORGANIZER_VIEW_HEADINGS_OVER_THE_COLUMNS_H
