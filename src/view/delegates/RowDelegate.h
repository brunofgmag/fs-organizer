#ifndef FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
#define FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H

#include <QtCore/QPersistentModelIndex>
#include <QtWidgets/QStyledItemDelegate>

class RowDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RowDelegate(QObject* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool helpEvent(QHelpEvent* event,
                   QAbstractItemView* view,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    void PointAt(const QModelIndex& index);

    [[nodiscard]] bool IsPointedAt(const QModelIndex& index) const;

    QPersistentModelIndex pointedAt_;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
