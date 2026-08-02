#ifndef FS_ORGANIZER_VIEW_SHELL_LANGUAGE_SWITCH_H
#define FS_ORGANIZER_VIEW_SHELL_LANGUAGE_SWITCH_H

#include <array>

#include <QtCore/QString>
#include <QtCore/QTranslator>

class LanguageSwitch
{
public:
    struct Offer
    {
        const char* code;
        const char* name;
        const char* objectName;
    };

    LanguageSwitch() = default;

    LanguageSwitch(const LanguageSwitch&) = delete;
    LanguageSwitch& operator=(const LanguageSwitch&) = delete;

    [[nodiscard]] static const std::array<Offer, 2>& Offered();

    [[nodiscard]] static QString Resolve(const QString& stored);

    [[nodiscard]] bool Use(const QString& language);

    [[nodiscard]] QString InUse() const;

private:
    void Uninstall();

    QTranslator interface_;
    QTranslator nativeWidgets_;
    QString inUse_;
    bool installed_ = false;
};

#endif // FS_ORGANIZER_VIEW_SHELL_LANGUAGE_SWITCH_H
