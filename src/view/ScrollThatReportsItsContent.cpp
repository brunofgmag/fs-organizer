#include "view/ScrollThatReportsItsContent.h"

#include <QtWidgets/QWidget>

ScrollThatReportsItsContent::ScrollThatReportsItsContent(QWidget* parent) : QScrollArea(parent)
{
    setSizeAdjustPolicy(AdjustToContents);
}

QSize ScrollThatReportsItsContent::sizeHint() const
{
    if (widget() == nullptr)
    {
        return QScrollArea::sizeHint();
    }

    const int frame = 2 * frameWidth();

    return widget()->sizeHint() + QSize(frame, frame);
}
