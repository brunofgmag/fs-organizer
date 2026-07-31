#include "view/delegates/FittedText.h"

#include <QtGui/QFontMetrics>

namespace
{
    constexpr int kMostTextsWorthRemembering = 4096;
}

QString FittedText::In(const QString& text, const QFont& font, const Qt::TextElideMode mode, const int width) const
{
    if (mode == Qt::ElideNone || width <= 0)
    {
        return text;
    }

    if (measuredWith_ != font)
    {
        remembered_.clear();
        measuredWith_ = font;
    }

    QHash<QString, QString>& atThisWidth = remembered_[width];

    if (const auto found = atThisWidth.constFind(text); found != atThisWidth.constEnd())
    {
        return *found;
    }

    if (atThisWidth.size() >= kMostTextsWorthRemembering)
    {
        atThisWidth.clear();
    }

    return *atThisWidth.insert(text, QFontMetrics(font).elidedText(text, mode, width));
}
