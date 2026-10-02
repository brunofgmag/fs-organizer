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

    void AlignTheCheckWithTheText();
    void LetTheFirstCellLeadTheRow();

    bool eventFilter(QObject* watched, QEvent* event) override;

    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool helpEvent(QHelpEvent* event,
                   QAbstractItemView* view,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    [[nodiscard]] int TimesItAskedTheFont() const;

    [[nodiscard]] bool DoubleClickedTheCheck() const;

private:
    void PointAt(const QModelIndex& index);

    [[nodiscard]] bool IsPointedAt(const QModelIndex& index) const;

    [[nodiscard]] int CheckShiftOf(const QStyleOptionViewItem& item) const;

    [[nodiscard]] bool IsOnTheCheck(const QModelIndex& index, const QPoint& at) const;

    [[nodiscard]] QStyleOptionViewItem ItemAsDrawn(const QStyleOptionViewItem& option, const QModelIndex& index) const;

    QPersistentModelIndex pointedAt_;
    FittedText fitted_;
    int shortestRow_ = 0;
    bool checkAlignedWithText_ = false;
    bool firstCellLeadsTheRow_ = false;
    bool doubleClickedTheCheck_ = false;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_ROW_DELEGATE_H
