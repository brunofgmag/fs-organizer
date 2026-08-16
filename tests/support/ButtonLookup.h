#ifndef FS_ORGANIZER_TESTS_SUPPORT_BUTTON_LOOKUP_H
#define FS_ORGANIZER_TESTS_SUPPORT_BUTTON_LOOKUP_H

#include <QtCore/QString>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

template<typename Matches>
[[nodiscard]] QPushButton* TheFirstButtonWhere(const QWidget& widget, Matches matches)
{
    for (QPushButton* button : widget.findChildren<QPushButton*>())
    {
        if (matches(button->text()))
        {
            return button;
        }
    }

    return nullptr;
}

[[nodiscard]] inline QPushButton* ButtonSaying(const QWidget& widget, const QString& text)
{
    return TheFirstButtonWhere(widget,
                               [&text](const QString& said)
                               {
                                   return said == text;
                               });
}

[[nodiscard]] inline QPushButton* ButtonStartingWith(const QWidget& widget, const QString& text)
{
    return TheFirstButtonWhere(widget,
                               [&text](const QString& said)
                               {
                                   return said.startsWith(text);
                               });
}

[[nodiscard]] inline QPushButton* ButtonContaining(const QWidget& widget, const QString& text)
{
    return TheFirstButtonWhere(widget,
                               [&text](const QString& said)
                               {
                                   return said.contains(text);
                               });
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_BUTTON_LOOKUP_H
