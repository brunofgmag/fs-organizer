#ifndef FS_ORGANIZER_TESTS_SUPPORT_INSTALLED_CATALOGUE_H
#define FS_ORGANIZER_TESTS_SUPPORT_INSTALLED_CATALOGUE_H

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>

#include "tests/support/CatalogueBesideTheBuild.h"

inline constexpr int kRoundsAfterALanguageChange = 3;

inline void LanguageChoices()
{
    QTest::addColumn<QString>("language");

    QTest::newRow("English") << QStringLiteral("en");
    QTest::newRow("Brazilian Portuguese") << QStringLiteral("pt_BR");
}

inline bool LoadedTheCatalogue(QTranslator& catalogue, const QString& language)
{
    if (language == QLatin1String("en"))
    {
        return true;
    }

    const QString file = TheCatalogueBesideTheBuild(language);

    return !file.isEmpty() && catalogue.load(file);
}

struct Installed
{
    explicit Installed(QTranslator& translator) : translator_(translator)
    {
        QCoreApplication::installTranslator(&translator_);
        Settle();
    }

    ~Installed()
    {
        QCoreApplication::removeTranslator(&translator_);
        Settle();
    }

    Installed(const Installed&) = delete;
    Installed& operator=(const Installed&) = delete;

    QTranslator& translator_;

private:
    static void Settle()
    {
        for (int round = 0; round < kRoundsAfterALanguageChange; ++round)
        {
            QCoreApplication::processEvents();
        }
    }
};

#endif // FS_ORGANIZER_TESTS_SUPPORT_INSTALLED_CATALOGUE_H
