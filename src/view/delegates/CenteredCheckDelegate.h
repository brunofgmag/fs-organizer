#ifndef FS_ORGANIZER_VIEW_DELEGATES_CENTERED_CHECK_DELEGATE_H
#define FS_ORGANIZER_VIEW_DELEGATES_CENTERED_CHECK_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

class CenteredCheckDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

protected:
    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_CENTERED_CHECK_DELEGATE_H
