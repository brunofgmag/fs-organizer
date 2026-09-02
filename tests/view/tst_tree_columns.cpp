#include <QtTest/QtTest>

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QTreeWidget>

#include "support/SizeText.h"
#include "view/TreeColumns.h"
#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class TreeColumnsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AWidenedColumnHoldsItsWidestRowWithoutCuttingIt();
        static void AClosedRowDoesNotWidenTheColumnForWhatItHides();
        static void AColumnLeftDraggableAnswersToBeingDragged();
        static void OpeningARowWidensTheColumnForWhatItJustRevealed();
    };
}

namespace
{
    constexpr auto kWidest = "tfdidesign-aircraft-md-11fge-md-11fpw-fedex-pack";

    struct Tree
    {
        QTreeWidget tree;

        Tree()
        {
            ApplyModernistTheme(*qApp);

            tree.setColumnCount(2);
            tree.setHeaderLabels({QStringLiteral("Category"), QStringLiteral("Size")});
            tree.header()->setStretchLastSection(false);
            tree.header()->setSectionResizeMode(0, QHeaderView::Stretch);
            tree.setItemDelegate(new RowDelegate(&tree));
            LetTheseColumnsBeDragged(&tree, {1});
            tree.resize(600, 200);
            tree.show();
        }

        QTreeWidgetItem* Add(const QString& name, const QString& said)
        {
            auto* row = new QTreeWidgetItem(&tree);
            row->setText(0, name);
            row->setText(1, said);
            row->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

            return row;
        }

        [[nodiscard]] int RoomTheDelegateAsksFor(const QModelIndex& index) const
        {
            QStyleOptionViewItem option;
            option.initFrom(&tree);
            option.widget = &tree;
            option.font = tree.font();
            option.fontMetrics = QFontMetrics(option.font);

            return tree.itemDelegate()->sizeHint(option, index).width();
        }

        [[nodiscard]] int WidthOfTheSizeColumn() const
        {
            return tree.header()->sectionSize(1);
        }
    };
}

void TreeColumnsTest::AWidenedColumnHoldsItsWidestRowWithoutCuttingIt()
{
    Tree built;
    built.Add(QStringLiteral("Sounds"), AsSize(1567664865));
    built.Add(QStringLiteral("Sceneries"), QString::fromLatin1(kWidest));

    WidenTheseColumnsToTheirRows(&built.tree, {1});

    const int wanted = built.RoomTheDelegateAsksFor(built.tree.model()->index(1, 1));

    QVERIFY2(built.WidthOfTheSizeColumn() >= wanted,
             qPrintable(QStringLiteral("the column came out at %1 for a row that asks for %2")
                            .arg(built.WidthOfTheSizeColumn())
                            .arg(wanted)));
}

void TreeColumnsTest::AClosedRowDoesNotWidenTheColumnForWhatItHides()
{
    Tree built;
    QTreeWidgetItem* parent = built.Add(QStringLiteral("Sceneries"), AsSize(1567664865));

    auto* hidden = new QTreeWidgetItem(parent);
    hidden->setText(1, QString::fromLatin1(kWidest));

    parent->setExpanded(false);
    WidenTheseColumnsToTheirRows(&built.tree, {1});
    const int closed = built.WidthOfTheSizeColumn();

    parent->setExpanded(true);
    WidenTheseColumnsToTheirRows(&built.tree, {1});

    QVERIFY2(
        built.WidthOfTheSizeColumn() > closed,
        qPrintable(
            QStringLiteral("closed it came out at %1 and open at %2").arg(closed).arg(built.WidthOfTheSizeColumn())));
}

void TreeColumnsTest::AColumnLeftDraggableAnswersToBeingDragged()
{
    Tree built;
    built.Add(QStringLiteral("Sounds"), QString::fromLatin1(kWidest));

    WidenTheseColumnsToTheirRows(&built.tree, {1});

    QCOMPARE(built.tree.header()->sectionResizeMode(1), QHeaderView::Interactive);

    const int wider = built.WidthOfTheSizeColumn() + 40;
    built.tree.header()->resizeSection(1, wider);

    QCOMPARE(built.WidthOfTheSizeColumn(), wider);
}

void TreeColumnsTest::OpeningARowWidensTheColumnForWhatItJustRevealed()
{
    Tree built;
    QTreeWidgetItem* parent = built.Add(QStringLiteral("Sceneries"), AsSize(1567664865));

    auto* inside = new QTreeWidgetItem(parent);
    inside->setText(1, QString::fromLatin1(kWidest));

    WidenTheseColumnsToTheirRows(&built.tree, {1});
    const int closed = built.WidthOfTheSizeColumn();

    parent->setExpanded(true);

    QVERIFY2(built.WidthOfTheSizeColumn() > closed,
             qPrintable(QStringLiteral("opening the row left the column at %1, where it was %2 closed")
                            .arg(built.WidthOfTheSizeColumn())
                            .arg(closed)));
}

QTEST_MAIN(TreeColumnsTest)

#include "tst_tree_columns.moc"
