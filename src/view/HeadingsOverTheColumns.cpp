#include "view/HeadingsOverTheColumns.h"

#include <QtCore/QEvent>
#include <QtGui/QMoveEvent>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>

#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kBetweenColumns = 12;
    constexpr int kBetweenRows = 6;
    constexpr int kAroundTheRows = 6;
}

HeadingsOverTheColumns::HeadingsOverTheColumns(const QStringList& headings, QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ColumnHeadings"));
    labels_.reserve(static_cast<std::size_t>(headings.size()));

    for (const QString& heading : headings)
    {
        auto* label = new QLabel(heading, this);
        label->setObjectName(QStringLiteral("PanelSubHeading"));
        labels_.push_back(label);
    }

    setFixedHeight(labels_.front()->sizeHint().height());
}

void HeadingsOverTheColumns::ShapeTheGridUnder(QGridLayout& grid) const
{
    grid.setContentsMargins(0, kAroundTheRows, kToolbarGap, kAroundTheRows);
    grid.setHorizontalSpacing(kBetweenColumns);
    grid.setVerticalSpacing(kBetweenRows);

    for (std::size_t column = 0; column < labels_.size(); ++column)
    {
        grid.setColumnMinimumWidth(static_cast<int>(column), labels_[column]->sizeHint().width());
    }
}

void HeadingsOverTheColumns::StandOver(const std::vector<QWidget*>& cells)
{
    cells_.assign(cells.begin(), cells.end());

    for (QWidget* cell : cells)
    {
        cell->installEventFilter(this);
    }

    cells.front()->parentWidget()->installEventFilter(this);

    Follow();
}

bool HeadingsOverTheColumns::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Move || event->type() == QEvent::Resize)
    {
        Follow();
    }

    return QWidget::eventFilter(watched, event);
}

void HeadingsOverTheColumns::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);

    Follow();
}

void HeadingsOverTheColumns::Follow()
{
    const int from = mapToGlobal(QPoint()).x();

    for (std::size_t column = 0; column < cells_.size(); ++column)
    {
        const QWidget* cell = cells_[column];

        if (cell != nullptr)
        {
            labels_[column]->setGeometry(cell->mapToGlobal(QPoint()).x() - from, 0, cell->width(), height());
        }
    }
}
