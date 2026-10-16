#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QTranslator>
#include <QtCore/QXmlStreamReader>
#include <QtTest/QtTest>

#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/InstalledCatalogue.h"
#include "view/shell/LanguageSwitch.h"

namespace
{
    class LinuxWordingTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EverySentenceTheOverlayRewordsIsStillInTheCatalogue_data();
        static void EverySentenceTheOverlayRewordsIsStillInTheCatalogue();
        static void EverySentenceWordedForWindowsIsRewordedOrKnownToBeTrueOnLinux();
        static void TheLinuxWordingSaysNothingThatIsFalseOnLinux_data();
        static void TheLinuxWordingSaysNothingThatIsFalseOnLinux();
        static void TheLanguageSwitchPutsTheLinuxWordingOverTheCatalogue();
        static void TheBrazilianWordingKeepsTheCatalogueWhereLinuxHasNothingToAdd();
    };
}

namespace
{
    struct Sentence
    {
        QString context{};
        QString source{};
        QString said{};

        [[nodiscard]] QString Key() const
        {
            return context + QLatin1Char('|') + source;
        }
    };

    QList<Sentence> Read(const QString& file)
    {
        QFile catalogue(file);
        if (!catalogue.open(QIODevice::ReadOnly))
        {
            return {};
        }

        QList<Sentence> sentences;
        QXmlStreamReader xml(&catalogue);
        QString context;
        Sentence current;
        bool vanished = false;

        while (!xml.atEnd())
        {
            xml.readNext();

            if (xml.isStartElement() && xml.name() == QLatin1String("name"))
            {
                context = xml.readElementText();
            }
            else if (xml.isStartElement() && xml.name() == QLatin1String("message"))
            {
                current = Sentence{.context = context, .source = {}, .said = {}};
                vanished = false;
            }
            else if (xml.isStartElement() && xml.name() == QLatin1String("source"))
            {
                current.source = xml.readElementText();
            }
            else if (xml.isStartElement() && xml.name() == QLatin1String("translation"))
            {
                const QString type = xml.attributes().value(QLatin1String("type")).toString();
                vanished = type == QLatin1String("vanished") || type == QLatin1String("obsolete");
                current.said = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
            else if (xml.isEndElement() && xml.name() == QLatin1String("message") && !vanished)
            {
                sentences.append(current);
            }
        }

        return sentences;
    }

    QList<Sentence> TheCatalogue()
    {
        return Read(QStringLiteral(FSORG_I18N_DIR "/app_en.ts"));
    }

    QList<Sentence> TheOverlay(const QString& language)
    {
        return Read(QStringLiteral(FSORG_I18N_DIR "/linux/platform_%1.ts").arg(language));
    }

    QSet<QString> KeysOf(const QList<Sentence>& sentences)
    {
        QSet<QString> keys;

        for (const Sentence& sentence : sentences)
        {
            keys.insert(sentence.Key());
        }

        return keys;
    }

    const QRegularExpression& WindowsWords()
    {
        static const QRegularExpression words(QStringLiteral("Windows|Recycle|recycle|dministrator|Developer Mode"));

        return words;
    }

    const QRegularExpression& FalseOnLinux()
    {
        static const QRegularExpression words(
            QStringLiteral("Windows|Recycle|recycle|dministrat|Developer|Desenvolvedor|tar\\.exe|junction"));

        return words;
    }

    const QStringList& TrueOnLinuxToo()
    {
        static const QStringList sentences = {
            QStringLiteral("OptionsPage|Works across local drives without administrator rights. Recommended."),
            QStringLiteral("OptionsPage|Only needed for a library on a network share. Requires Developer Mode or "
                           "administrator rights."),
            QStringLiteral("QObject|It holds an entry that is not safe to extract: a link, an absolute path, .. or a "
                           "name Windows cannot write."),
            QStringLiteral("QObject|It holds something unsafe to copy: a link or junction, or a name Windows cannot "
                           "write."),
        };

        return sentences;
    }

    void LanguageChoicesTheOverlayHas()
    {
        QTest::addColumn<QString>("language");

        QTest::newRow("English") << QStringLiteral("en");
        QTest::newRow("Brazilian Portuguese") << QStringLiteral("pt_BR");
    }
}

void LinuxWordingTest::EverySentenceTheOverlayRewordsIsStillInTheCatalogue_data()
{
    LanguageChoicesTheOverlayHas();
}

void LinuxWordingTest::EverySentenceTheOverlayRewordsIsStillInTheCatalogue()
{
    QFETCH(QString, language);

    const QSet<QString> inTheCode = KeysOf(TheCatalogue());
    const QList<Sentence> overlay = TheOverlay(language);

    QVERIFY2(!overlay.isEmpty(), "the Linux wording catalogue could not be read");

    for (const Sentence& sentence : overlay)
    {
        QVERIFY2(inTheCode.contains(sentence.Key()),
                 qPrintable(QStringLiteral("the Linux wording rewords a sentence the code no longer says: %1")
                                .arg(sentence.Key())));
    }
}

void LinuxWordingTest::EverySentenceWordedForWindowsIsRewordedOrKnownToBeTrueOnLinux()
{
    const QSet<QString> reworded = KeysOf(TheOverlay(QStringLiteral("en")));

    for (const Sentence& sentence : TheCatalogue())
    {
        if (!WindowsWords().match(sentence.source).hasMatch())
        {
            continue;
        }

        QVERIFY2(reworded.contains(sentence.Key()) || TrueOnLinuxToo().contains(sentence.Key()),
                 qPrintable(QStringLiteral("this sentence talks about Windows and reaches a Linux user as it is: %1")
                                .arg(sentence.Key())));
    }
}

void LinuxWordingTest::TheLinuxWordingSaysNothingThatIsFalseOnLinux_data()
{
    LanguageChoicesTheOverlayHas();
}

void LinuxWordingTest::TheLinuxWordingSaysNothingThatIsFalseOnLinux()
{
    QFETCH(QString, language);

    for (const Sentence& sentence : TheOverlay(language))
    {
        QVERIFY2(!FalseOnLinux().match(sentence.said).hasMatch(),
                 qPrintable(QStringLiteral("the Linux wording still says something that is false on Linux: %1")
                                .arg(sentence.said)));
    }
}

void LinuxWordingTest::TheLanguageSwitchPutsTheLinuxWordingOverTheCatalogue()
{
    LanguageSwitch language;

    QVERIFY(language.Use(QStringLiteral("en")));

    QCOMPARE(QCoreApplication::translate("DeleteDialog", "Move to the Recycle Bin"),
             QStringLiteral("Move to the Trash"));
    QCOMPARE(QCoreApplication::translate("QObject", "Windows refused the operation"),
             QStringLiteral("the system refused the operation"));
    QCOMPARE(QCoreApplication::translate("DeleteDialog",
                                         "%n selected addon cannot go to the Recycle Bin and will "
                                         "stay in the library:",
                                         nullptr, 2),
             QStringLiteral("2 selected addons cannot go to the Trash and will stay in the library:"));
    QCOMPARE(QCoreApplication::translate("DeleteDialog", "Delete permanently"), QStringLiteral("Delete permanently"));
}

void LinuxWordingTest::TheBrazilianWordingKeepsTheCatalogueWhereLinuxHasNothingToAdd()
{
    QTranslator catalogue;
    QTranslator platform;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, QStringLiteral("pt_BR")),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    QVERIFY(LanguageSwitch::LoadPlatformWording(platform, QStringLiteral("pt_BR")));

    const Installed first(catalogue);
    const Installed second(platform);

    QCOMPARE(QCoreApplication::translate("QObject", "Windows refused the operation"),
             QStringLiteral("o sistema recusou a operação"));
    QCOMPARE(QCoreApplication::translate("DeleteDialog", "Move to the Recycle Bin"),
             QStringLiteral("Mover para a Lixeira"));
}

QTEST_GUILESS_MAIN(LinuxWordingTest)

#include "tst_linux_wording.moc"
