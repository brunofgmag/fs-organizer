#include "view/shell/ArchiveDropSign.h"

#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistTones.h"

namespace
{
    [[nodiscard]] QRect InDevicePixels(const QRect& box, const qreal ratio)
    {
        return QRectF(box.x() * ratio, box.y() * ratio, box.width() * ratio, box.height() * ratio).toRect();
    }

    void PaintTheOutline(QPainter& painter, const QRect& box, const int thick, const QColor& ink)
    {
        painter.fillRect(QRect(box.left(), box.top(), box.width(), thick), ink);
        painter.fillRect(QRect(box.left(), box.bottom() - thick + 1, box.width(), thick), ink);
        painter.fillRect(QRect(box.left(), box.top(), thick, box.height()), ink);
        painter.fillRect(QRect(box.right() - thick + 1, box.top(), thick, box.height()), ink);
    }

    [[nodiscard]] QFont PlateFont(const QFont& base)
    {
        QFont said = base;
        said.setWeight(QFont::DemiBold);

        return said;
    }
}

ArchiveDropSign::ArchiveDropSign(QWidget* over) : QWidget(over)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    setGeometry(over->rect());
    hide();

    over->installEventFilter(this);
}

void ArchiveDropSign::InviteInto(const QRect& row, const QString& sentence, const QPoint& pointer)
{
    Say(row, sentence, pointer, false);
}

void ArchiveDropSign::Refuse(const QString& sentence, const QPoint& pointer)
{
    Say({}, sentence, pointer, true);
}

void ArchiveDropSign::Say(const QRect& row, const QString& sentence, const QPoint& pointer, const bool refuses)
{
    row_ = row;
    sentence_ = sentence;
    pointer_ = pointer;
    refuses_ = refuses;

    setAccessibleName(sentence_);
    setGeometry(parentWidget()->rect());
    raise();
    show();
    update();
}

void ArchiveDropSign::Leave()
{
    hide();
}

QRect ArchiveDropSign::PlateBox() const
{
    const QFontMetrics measured(PlateFont(font()));

    QRect plate(pointer_ + QPoint(kDropPlateRightOfThePointer, kDropPlateBelowThePointer),
                QSize(measured.horizontalAdvance(sentence_) + 2 * kDropPlateAirAcross,
                      measured.height() + 2 * kDropPlateAirAlong));

    if (plate.right() > width() - kPageGutter)
    {
        plate.moveRight(pointer_.x() - kDropPlateBackOffFromThePointer);
    }

    if (plate.bottom() > height() - kPageGutter)
    {
        plate.moveBottom(pointer_.y() - kDropPlateBackOffFromThePointer);
    }

    plate.moveLeft(qMax(plate.left(), kPageGutter));
    plate.moveTop(qMax(plate.top(), kPageGutter));

    return plate;
}

QRect ArchiveDropSign::Target() const
{
    return row_;
}

QString ArchiveDropSign::Sentence() const
{
    return sentence_;
}

bool ArchiveDropSign::Refuses() const
{
    return refuses_;
}

bool ArchiveDropSign::IsCut() const
{
    return PlateBox().width() > width() - 2 * kPageGutter;
}

bool ArchiveDropSign::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
    {
        setGeometry(parentWidget()->rect());
    }

    return QWidget::eventFilter(watched, event);
}

void ArchiveDropSign::paintEvent(QPaintEvent*)
{
    const ModernistTones tones = TonesOf(CurrentColorScheme());
    const qreal ratio = devicePixelRatioF();
    const QRect plate = PlateBox();

    QPainter painter(this);

    painter.save();
    painter.scale(1.0 / ratio, 1.0 / ratio);

    if (row_.isValid())
    {
        PaintTheOutline(painter, InDevicePixels(row_, ratio), qRound(kDropOutline * ratio), tones.accent);
    }

    const QRect ground = InDevicePixels(plate, ratio);

    painter.fillRect(ground, tones.raised);
    PaintTheOutline(painter, ground, qRound(ratio), refuses_ ? tones.edge : tones.accent);

    painter.restore();

    painter.setFont(PlateFont(font()));
    painter.setPen(tones.text);
    painter.drawText(plate, Qt::AlignCenter, sentence_);
}
