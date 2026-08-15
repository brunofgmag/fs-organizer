#include "view/delegates/CenteredCheckDelegate.h"

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

void CenteredCheckDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem cell = option;
    initStyleOption(&cell, index);

    QStyle* style = cell.widget != nullptr ? cell.widget->style() : QApplication::style();

    QStyleOptionViewItem background = cell;
    background.features &= ~QStyleOptionViewItem::HasCheckIndicator;
    style->drawControl(QStyle::CE_ItemViewItem, &background, painter, cell.widget);

    QStyleOptionViewItem check = cell;
    check.rect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &cell, cell.widget);
    check.rect.moveCenter(cell.rect.center());
    check.state &= ~QStyle::State_HasFocus;
    check.state |= cell.checkState == Qt::Checked ? QStyle::State_On : QStyle::State_Off;
    style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, painter, cell.widget);
}

bool CenteredCheckDelegate::editorEvent(QEvent* event,
                                        QAbstractItemModel* model,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index)
{
    if (!index.flags().testFlag(Qt::ItemIsUserCheckable) || !index.flags().testFlag(Qt::ItemIsEnabled))
    {
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        const auto* mouse = static_cast<QMouseEvent*>(event);

        if (!option.rect.contains(mouse->position().toPoint()))
        {
            return false;
        }
    }
    else if (event->type() == QEvent::KeyPress)
    {
        const auto* keys = static_cast<QKeyEvent*>(event);

        if (keys->key() != Qt::Key_Space && keys->key() != Qt::Key_Select)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    const Qt::CheckState flipped =
        index.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked;

    return model->setData(index, flipped, Qt::CheckStateRole);
}
