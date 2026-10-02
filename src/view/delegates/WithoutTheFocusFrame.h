#ifndef FS_ORGANIZER_VIEW_DELEGATES_WITHOUT_THE_FOCUS_FRAME_H
#define FS_ORGANIZER_VIEW_DELEGATES_WITHOUT_THE_FOCUS_FRAME_H

#include <QtWidgets/QStyledItemDelegate>

class WithoutTheFocusFrame final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& line) const override;

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& line) const override;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_WITHOUT_THE_FOCUS_FRAME_H
