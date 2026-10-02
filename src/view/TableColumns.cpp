#include "view/TableColumns.h"

#include <algorithm>
#include <vector>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QEvent>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>

namespace
{
    constexpr int kMostAColumnGivesBack = 6;

    [[nodiscard]] std::vector<int> SpreadOver(const std::vector<int>& offered, int owed)
    {
        std::vector<int> given(offered.size(), 0);

        for (std::size_t column = 0; owed > 0; column = (column + 1) % offered.size())
        {
            if (given[column] < offered[column])
            {
                ++given[column];
                --owed;
            }
        }

        return given;
    }

    class WidthKeeper final : public QObject
    {
    public:
        WidthKeeper(QTableView* table, const int columnThatTakesTheSlack)
            : QObject(table), table_(table), wanted_(columnThatTakesTheSlack)
        {
            table_->horizontalHeader()->setStretchLastSection(false);
            table_->viewport()->installEventFilter(this);
            LetEveryColumnButTheLastBeDragged();

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
                    [this](const int column, const int was, const int now)
                    {
                        if (applying_)
                        {
                            return;
                        }

                        theirs_ = true;
                        TakeTheDragOutOfWhatFollows(column, now - was);
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
        [[nodiscard]] int NarrowestFor(const int column) const
        {
            return table_->horizontalHeader()->sectionSizeHint(column);
        }

        [[nodiscard]] int LastColumn() const
        {
            const QHeaderView* header = table_->horizontalHeader();

            for (int column = header->count() - 1; column >= 0; --column)
            {
                if (!header->isSectionHidden(column))
                {
                    return column;
                }
            }

            return -1;
        }

        void LetEveryColumnButTheLastBeDragged() const
        {
            QHeaderView* header = table_->horizontalHeader();
            const int last = LastColumn();

            for (int column = 0; column < header->count(); ++column)
            {
                header->setSectionResizeMode(column, column == last ? QHeaderView::Fixed : QHeaderView::Interactive);
            }
        }

        void TakeTheDragOutOfWhatFollows(const int dragged, const int delta)
        {
            QHeaderView* header = table_->horizontalHeader();
            int owed = delta;

            applying_ = true;

            if (const int floor = NarrowestFor(dragged); header->sectionSize(dragged) < floor)
            {
                owed += floor - header->sectionSize(dragged);
                header->resizeSection(dragged, floor);
            }

            for (int column = dragged + 1; column < header->count() && owed != 0; ++column)
            {
                if (header->isSectionHidden(column))
                {
                    continue;
                }

                const int was = header->sectionSize(column);
                const int now = std::max(NarrowestFor(column), was - owed);

                header->resizeSection(column, now);
                owed -= was - now;
            }

            if (owed != 0)
            {
                header->resizeSection(dragged, std::max(NarrowestFor(dragged), header->sectionSize(dragged) - owed));
            }

            applying_ = false;
        }

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

            measured_.assign(header->count(), -1);

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
                measured_[column] = measured;
            }
            applying_ = false;

            LetEveryColumnButTheLastBeDragged();
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

            applying_ = true;
            if (!theirs_)
            {
                SizeTheMeasuredColumnsForTheSlack(slack);
            }

            int taken = 0;
            for (int column = 0; column < header->count(); ++column)
            {
                taken += column == slack || header->isSectionHidden(column) ? 0 : header->sectionSize(column);
            }

            header->resizeSection(slack, std::max(NarrowestFor(slack), table_->viewport()->width() - taken));
            applying_ = false;
        }

        [[nodiscard]] bool WasMeasured(const int column) const
        {
            return column < static_cast<int>(measured_.size()) && measured_[column] >= 0;
        }

        [[nodiscard]] int WhatItAsks(const int column) const
        {
            return WasMeasured(column) ? measured_[column] : table_->horizontalHeader()->sectionSize(column);
        }

        [[nodiscard]] int CanGiveBack(const int column) const
        {
            return WasMeasured(column) ? std::clamp(measured_[column] - NarrowestFor(column), 0, kMostAColumnGivesBack)
                                       : 0;
        }

        [[nodiscard]] bool SizedByContent(const int column, const int slack) const
        {
            return column != slack && !table_->horizontalHeader()->isSectionHidden(column);
        }

        [[nodiscard]] std::vector<int> WhatEachColumnGivesBack(const int slack) const
        {
            const int count = table_->horizontalHeader()->count();
            std::vector<int> offered(count, 0);

            int owed = NarrowestFor(slack) - table_->viewport()->width();
            int room = 0;

            for (int column = 0; column < count; ++column)
            {
                if (SizedByContent(column, slack))
                {
                    owed += WhatItAsks(column);
                    offered[column] = CanGiveBack(column);
                    room += offered[column];
                }
            }

            if (owed <= 0 || owed > room)
            {
                return std::vector<int>(count, 0);
            }

            return SpreadOver(offered, owed);
        }

        void SizeTheMeasuredColumnsForTheSlack(const int slack)
        {
            QHeaderView* header = table_->horizontalHeader();
            const std::vector<int> given = WhatEachColumnGivesBack(slack);

            for (int column = 0; column < header->count(); ++column)
            {
                if (SizedByContent(column, slack) && WasMeasured(column))
                {
                    header->resizeSection(column, measured_[column] - given[column]);
                }
            }
        }

        QTableView* table_;
        std::vector<int> measured_;
        int wanted_ = -1;
        bool dying_ = false;
        bool theirs_ = false;
        bool applying_ = false;
        bool waiting_ = false;
    };

    class ColumnFollower final : public QObject
    {
    public:
        ColumnFollower(QTableView* follower, QTableView* followed)
            : QObject(follower), mine_(follower->horizontalHeader()), theirs_(followed->horizontalHeader())
        {
            mine_->setStretchLastSection(true);

            connect(theirs_, &QHeaderView::sectionResized, this,
                    [this]
                    {
                        CopyEveryWidth();
                    });

            connect(theirs_, &QObject::destroyed, this,
                    [this]
                    {
                        theirs_ = nullptr;
                    });

            CopyEveryWidth();
        }

    private:
        void CopyEveryWidth() const
        {
            if (theirs_ == nullptr)
            {
                return;
            }

            const int columns = std::min(mine_->count(), theirs_->count());

            for (int column = 0; column < columns; ++column)
            {
                mine_->setSectionResizeMode(column, QHeaderView::Fixed);
                mine_->setSectionHidden(column, theirs_->isSectionHidden(column));

                if (!theirs_->isSectionHidden(column))
                {
                    mine_->resizeSection(column, theirs_->sectionSize(column));
                }
            }
        }

        QHeaderView* mine_;
        QHeaderView* theirs_;
    };
}

void LetTheColumnsBeDraggedAndStillFillTheTable(QTableView* table, const int columnThatTakesTheSlack)
{
    new WidthKeeper(table, columnThatTakesTheSlack);
}

void LetTheColumnsFollowThoseOf(QTableView* follower, QTableView* followed)
{
    new ColumnFollower(follower, followed);
}
