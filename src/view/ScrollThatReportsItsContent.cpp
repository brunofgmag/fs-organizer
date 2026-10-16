#include "view/ScrollThatReportsItsContent.h"

#include <algorithm>

#include <QtWidgets/QScrollBar>
#include <QtWidgets/QWidget>

ScrollThatReportsItsContent::ScrollThatReportsItsContent(QWidget* parent) : QScrollArea(parent)
{
    setSizeAdjustPolicy(AdjustToContents);
}

void ScrollThatReportsItsContent::MeasureTheContentAt(const int wide)
{
    wide_ = wide;
}

QSize ScrollThatReportsItsContent::sizeHint() const
{
    if (widget() == nullptr)
    {
        return QScrollArea::sizeHint();
    }

    const int frame = 2 * frameWidth();
    const QSize hint = widget()->sizeHint();

    if (wide_ <= frame || !widget()->hasHeightForWidth())
    {
        return hint + QSize(frame, frame);
    }

    const int room = wide_ - frame;
    const int barred = room - verticalScrollBar()->sizeHint().width();
    const int tall = std::max(widget()->heightForWidth(room), barred > 0 ? widget()->heightForWidth(barred) : 0);

    return {hint.width() + frame, tall + frame};
}
