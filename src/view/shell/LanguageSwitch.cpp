#include "view/shell/LanguageSwitch.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLocale>

namespace
{
    constexpr auto kSourceLanguage = "en";
}

bool LanguageSwitch::LoadNativeWidgets(QTranslator& translator, const QString& language)
{
    const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    const QString name = QStringLiteral("qt_%1").arg(language);

    return translator.load(name, beside) || translator.load(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath));
}

const std::array<LanguageSwitch::Offer, 2>& LanguageSwitch::Offered()
{
    static const std::array<Offer, 2> offered{
        Offer{.code = kSourceLanguage, .name = "English", .objectName = "EnglishChoice"},
        Offer{.code = "pt_BR", .name = "Português (Brasil)", .objectName = "BrazilianChoice"},
    };

    return offered;
}

bool LanguageSwitch::IsOffered(const QString& language)
{
    for (const Offer& offer : Offered())
    {
        if (language == QLatin1String(offer.code))
        {
            return true;
        }
    }

    return false;
}

QString LanguageSwitch::Resolve(const QString& stored)
{
    if (IsOffered(stored))
    {
        return stored;
    }

    const QString system = QLocale::system().name();

    for (const Offer& offer : Offered())
    {
        if (system == QLatin1String(offer.code))
        {
            return system;
        }
    }

    return system.startsWith(QLatin1String("pt")) ? QStringLiteral("pt_BR") : QLatin1String(kSourceLanguage);
}

bool LanguageSwitch::Use(const QString& language)
{
    const QString wanted = Resolve(language);

    if (installed_ && wanted == inUse_)
    {
        return true;
    }

    Uninstall();

    const bool loaded = interface_.load(QStringLiteral(":/i18n/app_%1").arg(wanted));

    if (loaded)
    {
        QCoreApplication::installTranslator(&interface_);
    }

    if (LoadNativeWidgets(nativeWidgets_, wanted))
    {
        QCoreApplication::installTranslator(&nativeWidgets_);
    }

    const bool onScreen = loaded || wanted == QLatin1String(kSourceLanguage);

    inUse_ = onScreen ? wanted : QLatin1String(kSourceLanguage);
    installed_ = true;

    return onScreen;
}

QString LanguageSwitch::InUse() const
{
    return inUse_;
}

void LanguageSwitch::Uninstall()
{
    if (!installed_)
    {
        return;
    }

    QCoreApplication::removeTranslator(&interface_);
    QCoreApplication::removeTranslator(&nativeWidgets_);
    installed_ = false;
}
