#ifndef FS_ORGANIZER_VIEW_DELEGATES_PLAIN_TEXT_DELEGATE_H
#define FS_ORGANIZER_VIEW_DELEGATES_PLAIN_TEXT_DELEGATE_H

#include <QtWidgets/QStyledItemDelegate>

#include "view/delegates/FittedText.h"

class PlainTextDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit PlainTextDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    FittedText fitted_;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_PLAIN_TEXT_DELEGATE_H
