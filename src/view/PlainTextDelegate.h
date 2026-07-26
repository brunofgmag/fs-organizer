#ifndef FS_ORGANIZER_VIEW_PLAIN_TEXT_DELEGATE_H
#define FS_ORGANIZER_VIEW_PLAIN_TEXT_DELEGATE_H

#include <QtCore/QHash>
#include <QtGui/QFont>
#include <QtWidgets/QStyledItemDelegate>

class PlainTextDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit PlainTextDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    [[nodiscard]] QString Fitted(const QString& text, const QStyleOptionViewItem& item, int width) const;

    mutable QHash<int, QHash<QString, QString>> fitted_;
    mutable QFont measuredWith_;
};

#endif // FS_ORGANIZER_VIEW_PLAIN_TEXT_DELEGATE_H
