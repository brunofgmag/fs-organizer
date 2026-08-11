#ifndef FS_ORGANIZER_VIEW_DELEGATES_FITTED_TEXT_H
#define FS_ORGANIZER_VIEW_DELEGATES_FITTED_TEXT_H

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtGui/QFont>

class FittedText
{
public:
    [[nodiscard]] QString In(const QString& text, const QFont& font, Qt::TextElideMode mode, int width) const;

    [[nodiscard]] int AdvanceOf(const QString& text, const QFont& font) const;

    [[nodiscard]] int TimesItAskedTheFont() const;

private:
    mutable QHash<int, QHash<QString, QString>> remembered_;
    mutable QFont measuredWith_;
    mutable int asked_ = 0;
};

#endif // FS_ORGANIZER_VIEW_DELEGATES_FITTED_TEXT_H
