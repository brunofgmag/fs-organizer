#ifndef FS_ORGANIZER_VIEW_LANGUAGE_SWITCH_H
#define FS_ORGANIZER_VIEW_LANGUAGE_SWITCH_H

#include <string>

#include <QtCore/QString>
#include <QtCore/QTranslator>

class LanguageSwitch
{
public:
    LanguageSwitch() = default;

    LanguageSwitch(const LanguageSwitch&) = delete;
    LanguageSwitch& operator=(const LanguageSwitch&) = delete;

    void Use(const QString& language);

    [[nodiscard]] QString InUse() const;

    [[nodiscard]] static QString Stored(const std::string& stored);

private:
    void Uninstall();

    QTranslator interface_;
    QTranslator nativeWidgets_;
    QString inUse_;
    bool installed_ = false;
};

#endif // FS_ORGANIZER_VIEW_LANGUAGE_SWITCH_H
