#ifndef FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
#define FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H

#include <QtCore/QPersistentModelIndex>
#include <QtWidgets/QStyledItemDelegate>

#include "view/delegates/FittedText.h"

class RowDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RowDelegate(QObject* parent = nullptr);

    void KeepRowsAtLeast(int tall);

    bool eventFilter(QObject* watched, QEvent* event) override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool helpEvent(QHelpEvent* event,
                   QAbstractItemView* view,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    [[nodiscard]] int TimesItAskedTheFont() const;

private:
    void PointAt(const QModelIndex& index);

    [[nodiscard]] bool IsPointedAt(const QModelIndex& index) const;

    QPersistentModelIndex pointedAt_;
    FittedText fitted_;
    int shortestRow_ = 0;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
