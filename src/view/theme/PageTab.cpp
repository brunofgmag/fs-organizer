#include "view/theme/PageTab.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include "view/theme/ModernistTones.h"

namespace
{
    constexpr int kPaddingX = 14;
    constexpr int kAboveText = 9;
    constexpr int kBelowText = 10;
    constexpr int kUnderline = 3;
    constexpr int kBetweenNameAndCount = 6;

    QFont NameFont(const QFont& base, const bool chosen)
    {
        QFont font = base;
        font.setWeight(chosen ? QFont::DemiBold : QFont::Normal);

        return font;
    }
}

PageTab::PageTab(const QString& label, QWidget* parent) : QToolButton(parent), label_(label)
{
    setObjectName(QStringLiteral("PageTab"));
    setText(label);
    setCheckable(true);
    setAutoExclusive(true);
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

void PageTab::Relabel(const QString& label)
{
    label_ = label;
    ShowCount(count_);
}

void PageTab::ShowCount(const std::optional<qsizetype> count)
{
    count_ = count;
    setText(count.has_value() ? QStringLiteral("%1 %2").arg(label_).arg(*count) : label_);
    updateGeometry();
    update();
}

void PageTab::RememberSource(const char* source)
{
    source_ = source;
}

const char* PageTab::Source() const
{
    return source_;
}

QString PageTab::Label() const
{
    return label_;
}

QString PageTab::CountText() const
{
    return count_.has_value() ? QString::number(*count_) : QString();
}

QSize PageTab::sizeHint() const
{
    const QFontMetrics chosen(NameFont(font(), true));

    int width = 2 * kPaddingX + chosen.horizontalAdvance(label_);

    if (const QString count = CountText(); !count.isEmpty())
    {
        width += kBetweenNameAndCount + chosen.horizontalAdvance(count);
    }

    return {width, kAboveText + chosen.height() + kBelowText + kUnderline};
}

QSize PageTab::minimumSizeHint() const
{
    return sizeHint();
}

void PageTab::paintEvent(QPaintEvent*)
{
    const ModernistTones tones = TonesOf(CurrentColorScheme());
    const bool chosen = isChecked();
    const bool warm = underMouse();

    QPainter painter(this);

    if (chosen)
    {
        painter.fillRect(QRect(0, height() - kUnderline, width(), kUnderline), tones.accent);
    }

    const QFont name = NameFont(font(), chosen);
    const QFontMetrics measured(name);
    const int baseline = kAboveText + measured.ascent();

    painter.setFont(name);
    painter.setPen(chosen || warm ? tones.text : tones.secondary);
    painter.drawText(kPaddingX, baseline, label_);

    const QString count = CountText();
    if (count.isEmpty())
    {
        return;
    }

    painter.setFont(font());
    painter.setPen(chosen ? tones.secondary : tones.faint);
    painter.drawText(kPaddingX + measured.horizontalAdvance(label_) + kBetweenNameAndCount, baseline, count);
}
