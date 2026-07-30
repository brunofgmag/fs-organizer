#include "view/TableColumns.h"

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

namespace
{
    constexpr int kNarrowest = 80;

    class WidthKeeper final : public QObject
    {
    public:
        WidthKeeper(QTableView* table, const int columnThatTakesTheSlack)
            : QObject(table), table_(table), wanted_(columnThatTakesTheSlack)
        {
            table_->horizontalHeader()->setStretchLastSection(false);
            table_->viewport()->installEventFilter(this);

            connect(table_, &QObject::destroyed, this,
                    [this]
                    {
                        dying_ = true;
                    });

            connect(table_->model(), &QAbstractItemModel::modelReset, this,
                    [this]
                    {
                        MeasureTheContentOnce();
                    });

            connect(table_->model(), &QAbstractItemModel::dataChanged, this,
                    [this]
                    {
                        MeasureOnceTheContentSettles();
                    });

            connect(table_->horizontalHeader(), &QHeaderView::sectionResized, this,
                    [this](int, int, int)
                    {
                        if (applying_)
                        {
                            return;
                        }

                        theirs_ = true;
                        FillTheSlack();
                    });

            MeasureTheContentOnce();
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
            {
                FillTheSlack();
            }

            return QObject::eventFilter(watched, event);
        }

    private:
        [[nodiscard]] int SlackColumn() const
        {
            const QHeaderView* header = table_->horizontalHeader();

            if (wanted_ >= 0 && wanted_ < header->count() && !header->isSectionHidden(wanted_))
            {
                return wanted_;
            }

            for (int column = header->count() - 1; column >= 0; --column)
            {
                if (!header->isSectionHidden(column))
                {
                    return column;
                }
            }

            return -1;
        }

        void MeasureOnceTheContentSettles()
        {
            if (waiting_ || theirs_)
            {
                return;
            }

            waiting_ = true;

            QMetaObject::invokeMethod(
                this,
                [this]
                {
                    waiting_ = false;
                    MeasureTheContentOnce();
                },
                Qt::QueuedConnection);
        }

        void MeasureTheContentOnce()
        {
            if (dying_ || theirs_ || table_->model()->rowCount({}) == 0)
            {
                return;
            }

            QHeaderView* header = table_->horizontalHeader();
            const int slack = SlackColumn();

            applying_ = true;
            for (int column = 0; column < header->count(); ++column)
            {
                if (column == slack || header->isSectionHidden(column))
                {
                    continue;
                }

                header->setSectionResizeMode(column, QHeaderView::ResizeToContents);
                const int measured = header->sectionSize(column);

                header->setSectionResizeMode(column, QHeaderView::Interactive);
                header->resizeSection(column, measured);
            }
            applying_ = false;

            FillTheSlack();
        }

        void FillTheSlack()
        {
            if (dying_)
            {
                return;
            }

            QHeaderView* header = table_->horizontalHeader();
            const int slack = SlackColumn();
            if (slack < 0)
            {
                return;
            }

            int taken = 0;
            for (int column = 0; column < header->count(); ++column)
            {
                taken += column == slack || header->isSectionHidden(column) ? 0 : header->sectionSize(column);
            }

            applying_ = true;
            header->resizeSection(slack, std::max(kNarrowest, table_->viewport()->width() - taken));
            applying_ = false;
        }

        QTableView* table_;
        int wanted_ = -1;
        bool dying_ = false;
        bool theirs_ = false;
        bool applying_ = false;
        bool waiting_ = false;
    };
}

void LetTheColumnsBeDraggedAndStillFillTheTable(QTableView* table, const int columnThatTakesTheSlack)
{
    new WidthKeeper(table, columnThatTakesTheSlack);
}
