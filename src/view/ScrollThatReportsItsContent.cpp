#include "view/ScrollThatReportsItsContent.h"

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

    return {hint.width() + frame, widget()->heightForWidth(wide_ - frame) + frame};
}
