#ifndef FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
#define FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

class RowDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit RowDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
