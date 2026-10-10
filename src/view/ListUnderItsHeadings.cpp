#include "view/ListUnderItsHeadings.h"

#include <QtWidgets/QFrame>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "view/HeadingsOverTheColumns.h"
#include "view/PaintedRule.h"

namespace
{
    constexpr int kBetweenHeadingsAndRule = 4;

    void LetAShortListBeAsTallAsItsRows(QScrollArea& list)
    {
        const int asked = list.sizeHint().height();

        if (asked < list.minimumSizeHint().height())
        {
            list.setMaximumHeight(asked);
        }
    }
}

QVBoxLayout* ListUnderItsHeadings(HeadingsOverTheColumns& headings, QScrollArea& list, QWidget& owner)
{
    list.setFrameShape(QFrame::NoFrame);

    LetAShortListBeAsTallAsItsRows(list);

    auto* table = new QVBoxLayout;
    table->setContentsMargins(0, 0, 0, 0);
    table->setSpacing(0);
    table->addWidget(&headings);
    table->addSpacing(kBetweenHeadingsAndRule);
    table->addWidget(new PaintedRule(&owner));
    table->addWidget(&list, 1);
    table->addWidget(new PaintedRule(&owner));

    return table;
}
