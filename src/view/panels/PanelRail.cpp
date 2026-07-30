#include "view/panels/PanelRail.h"

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "view/theme/ModernistPaint.h"

namespace
{
    constexpr int kRailWidth = 34;
    constexpr int kAboveTheArrow = 7;
    constexpr int kBeforeTheSpine = 12;
}

PanelRail::PanelRail(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("PanelRail"));
    setFixedWidth(kRailWidth);

    expand_ = new QToolButton(this);
    expand_->setObjectName(QStringLiteral("PanelExpand"));
    expand_->setAutoRaise(true);
    expand_->setArrowType(Qt::LeftArrow);
    expand_->setToolTip(tr("Abrir o painel"));
    expand_->setCursor(Qt::PointingHandCursor);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, kAboveTheArrow, 0, 0);
    column->setSpacing(0);
    column->addWidget(expand_, 0, Qt::AlignHCenter);
    column->addStretch();

    connect(expand_, &QToolButton::clicked, this, &PanelRail::ExpandRequested);
}

int PanelRail::Width()
{
    return kRailWidth;
}

void PanelRail::ShowTitle(const QString& title)
{
    title_ = title;
    setToolTip(title);
    update();
}

void PanelRail::paintEvent(QPaintEvent*)
{
    QStyleOption ground;
    ground.initFrom(this);

    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &ground, &painter, this);

    if (title_.isEmpty())
    {
        return;
    }

    const int top = expand_->geometry().bottom() + kBeforeTheSpine;
    const int room = height() - top;
    if (room <= 0)
    {
        return;
    }

    const QFont spine = SpineFont(font());
    const QFontMetrics measured(spine);
    const QString fitted = measured.elidedText(title_, Qt::ElideRight, room);

    painter.setFont(spine);
    painter.setPen(QuietInk());
    painter.translate(width() / 2.0 + measured.height() / 2.0 - measured.descent(), top);
    painter.rotate(90.0);
    painter.drawText(0, 0, room, measured.height(), Qt::AlignLeft | Qt::AlignVCenter, fitted);
}
