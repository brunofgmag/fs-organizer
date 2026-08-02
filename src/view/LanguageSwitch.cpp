#include "view/LanguageSwitch.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLocale>

namespace
{
    constexpr auto kFallback = "en";
    constexpr auto kBrazilian = "pt_BR";

    bool IsOffered(const QString& language)
    {
        return language == QLatin1String(kFallback) || language == QLatin1String(kBrazilian);
    }

    bool LoadNativeWidgets(QTranslator& translator, const QString& language)
    {
        const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
        const QString name = QStringLiteral("qt_%1").arg(language);

        return translator.load(name, beside)
            || translator.load(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    }
}

void LanguageSwitch::Use(const QString& language)
{
    const QString wanted = IsOffered(language) ? language : QLatin1String(kFallback);
    if (installed_ && wanted == inUse_)
    {
        return;
    }

    Uninstall();

    if (interface_.load(QStringLiteral(":/i18n/app_%1").arg(wanted)))
    {
        QCoreApplication::installTranslator(&interface_);
    }

    if (LoadNativeWidgets(nativeWidgets_, wanted))
    {
        QCoreApplication::installTranslator(&nativeWidgets_);
    }

    inUse_ = wanted;
    installed_ = true;
}

QString LanguageSwitch::InUse() const
{
    return inUse_;
}

QString LanguageSwitch::Stored(const std::string& stored)
{
    if (const QString written = QString::fromStdString(stored); IsOffered(written))
    {
        return written;
    }

    return QLocale::system().name().startsWith(QLatin1String("pt")) ? QLatin1String(kBrazilian)
                                                                    : QLatin1String(kFallback);
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
