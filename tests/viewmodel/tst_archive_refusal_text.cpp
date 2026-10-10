#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <set>

#include "tests/support/EnumPrinting.h"
#include "tests/support/InstalledCatalogue.h"
#include "viewmodel/ArchiveRefusalText.h"
#include "viewmodel/FailureText.h"

namespace
{
    class ArchiveRefusalTextTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryFailureOfAnArchiveButNoneHasASentenceOfItsOwn();
        static void NoFailureIsNoSentence();
        static void EveryReasonToInstallNothingHasASentenceOfItsOwn();
        static void NothingToInstallNoneIsNoSentence();
        static void AnArchiveThatCouldNotBeOpenedSaysSoByItsExtension_data();
        static void AnArchiveThatCouldNotBeOpenedSaysSoByItsExtension();
        static void OnlyCouldNotOpenDependsOnTheFileName();
        static void EveryResultThatStopsAnInstallHasASentenceOfItsOwn();
        static void AnyOtherResultFallsBackToTheExplanationOfTheResult();
        static void NoSentenceCarriesADash();
        static void NoSentenceClaimsARemovalNobodyChecked();
        static void ATimeOutServesTheReadingAndTheExtraction();
        static void WhenTheDiskOrTheSourceStopsAPackageTheSentenceSaysWhatToDo();
        static void TheFolderThatStayedBehindNamesTheScreenThatLetsYouDiscardIt();
        static void TheSentencesSpeakPortugueseWhenTheCatalogueIsLoaded();
        static void TheSentencesOfTheEarlierRoundsSayItPlainly();
        static void AFolderIsNeverToldItWasAnArchiveOrWasExtracted();
        static void AFolderThatCouldNotBeReadIsNotCalledDamaged();
        static void AFolderTellsLinksAndNamesApart();
        static void TheSentenceAboutASourceInTheWayHoldsInBothDirectionsForAnArchiveAndAFolder();
        static void TheSentencesOfAFolderSpeakPortugueseWhenTheCatalogueIsLoaded();
    };
}

namespace
{
    constexpr std::array kFailures{ArchiveFailure::TheToolIsMissing, ArchiveFailure::CouldNotOpen,
                                   ArchiveFailure::NeedsAPassword,   ArchiveFailure::CouldNotExtract,
                                   ArchiveFailure::TookTooLong,      ArchiveFailure::Cancelled};

    constexpr std::array kRefusals{NothingToInstall::NoPackage,
                                   NothingToInstall::OnlyAnInstaller,
                                   NothingToInstall::AnEntryIsNotSafe,
                                   NothingToInstall::ThePackageHasNoFolder,
                                   NothingToInstall::TwoPackagesShareAName,
                                   NothingToInstall::ThePackageNameIsReserved};

    constexpr std::array kResultsWithASentence{FileResult::NotEnoughFreeSpace,
                                               FileResult::CouldNotExtract,
                                               FileResult::VerificationFailed,
                                               FileResult::TheSimulatorIsRunning,
                                               FileResult::CouldNotDiscard,
                                               FileResult::TheOriginIsOccupied,
                                               FileResult::Cancelled,
                                               FileResult::TheArchiveNeedsAPassword,
                                               FileResult::TheDiskDisagreesWithTheScan,
                                               FileResult::TheSourceIsInsideWhatWouldBeRemoved};

    bool HasASentenceOfItsOwn(const FileResult result)
    {
        return std::ranges::find(kResultsWithASentence, result) != kResultsWithASentence.end();
    }

    constexpr std::array kSources{SourceKind::Archive, SourceKind::Folder};

    QStringList EverySentence()
    {
        QStringList sentences;

        for (const SourceKind source : kSources)
        {
            for (const ArchiveFailure failure : kFailures)
            {
                sentences << WhyTheArchiveIsRefused(failure, QStringLiteral("pack.zip"), source)
                          << WhyTheArchiveIsRefused(failure, QStringLiteral("pack.rar"), source);
            }

            for (const NothingToInstall refusal : kRefusals)
            {
                sentences << WhyTheArchiveIsRefused(refusal, source);
            }

            for (const FileResult result : kResultsWithASentence)
            {
                sentences << WhyItWasNotInstalled(result, source);
            }
        }

        return sentences;
    }
}

void ArchiveRefusalTextTest::EveryFailureOfAnArchiveButNoneHasASentenceOfItsOwn()
{
    std::set<QString> seen;

    for (const ArchiveFailure failure : kFailures)
    {
        const QString sentence = WhyTheArchiveIsRefused(failure, QStringLiteral("pack.zip"), SourceKind::Archive);

        QVERIFY2(!sentence.isEmpty(), QByteArray::number(static_cast<int>(failure)).constData());
        QVERIFY2(seen.insert(sentence).second, QByteArray::number(static_cast<int>(failure)).constData());
    }
}

void ArchiveRefusalTextTest::NoFailureIsNoSentence()
{
    QVERIFY(WhyTheArchiveIsRefused(ArchiveFailure::None, QStringLiteral("pack.zip"), SourceKind::Archive).isEmpty());
    QVERIFY(WhyTheArchiveIsRefused(ArchiveFailure::None, QStringLiteral("Folder"), SourceKind::Folder).isEmpty());
}

void ArchiveRefusalTextTest::EveryReasonToInstallNothingHasASentenceOfItsOwn()
{
    std::set<QString> seen;

    for (const NothingToInstall refusal : kRefusals)
    {
        const QString sentence = WhyTheArchiveIsRefused(refusal, SourceKind::Archive);

        QVERIFY2(!sentence.isEmpty(), QByteArray::number(static_cast<int>(refusal)).constData());
        QVERIFY2(seen.insert(sentence).second, QByteArray::number(static_cast<int>(refusal)).constData());
    }
}

void ArchiveRefusalTextTest::NothingToInstallNoneIsNoSentence()
{
    QVERIFY(WhyTheArchiveIsRefused(NothingToInstall::None, SourceKind::Archive).isEmpty());
    QVERIFY(WhyTheArchiveIsRefused(NothingToInstall::None, SourceKind::Folder).isEmpty());
}

void ArchiveRefusalTextTest::AnArchiveThatCouldNotBeOpenedSaysSoByItsExtension_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("namesTheWindows10Limit");

    QTest::newRow("zip") << QStringLiteral("pack.zip") << false;
    QTest::newRow("ZIP") << QStringLiteral("PACK.ZIP") << false;
    QTest::newRow("rar") << QStringLiteral("pack.rar") << true;
    QTest::newRow("Rar") << QStringLiteral("Pack.Rar") << true;
    QTest::newRow("7z") << QStringLiteral("pack.7z") << true;
    QTest::newRow("7Z") << QStringLiteral("PACK.7Z") << true;
    QTest::newRow("a name with dots") << QStringLiteral("a.b.c.7z") << true;
}

void ArchiveRefusalTextTest::AnArchiveThatCouldNotBeOpenedSaysSoByItsExtension()
{
    QFETCH(QString, fileName);
    QFETCH(bool, namesTheWindows10Limit);

    const QString sentence = WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, fileName, SourceKind::Archive);

    QVERIFY(!sentence.isEmpty());
    QCOMPARE(sentence.contains(QStringLiteral("Windows 10")), namesTheWindows10Limit);
}

void ArchiveRefusalTextTest::OnlyCouldNotOpenDependsOnTheFileName()
{
    for (const ArchiveFailure failure : kFailures)
    {
        if (failure == ArchiveFailure::CouldNotOpen)
        {
            continue;
        }

        for (const SourceKind source : kSources)
        {
            QCOMPARE(WhyTheArchiveIsRefused(failure, QStringLiteral("pack.zip"), source),
                     WhyTheArchiveIsRefused(failure, QStringLiteral("pack.7z"), source));
        }
    }
}

void ArchiveRefusalTextTest::EveryResultThatStopsAnInstallHasASentenceOfItsOwn()
{
    for (const FileResult result : kResultsWithASentence)
    {
        const QString sentence = WhyItWasNotInstalled(result, SourceKind::Archive);
        const QByteArray which = QByteArray::number(static_cast<int>(result));

        QVERIFY2(!sentence.isEmpty(), which.constData());
        QVERIFY2(sentence != Explain(result), which.constData());
    }

    QCOMPARE(WhyItWasNotInstalled(FileResult::TheSimulatorIsRunning, SourceKind::Archive),
             QStringLiteral("The simulator is running, so it was not installed."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::Cancelled, SourceKind::Archive),
             QStringLiteral("You cancelled, so nothing was installed."));
}

void ArchiveRefusalTextTest::AnyOtherResultFallsBackToTheExplanationOfTheResult()
{
    for (const FileResult result : kAllFileResults)
    {
        if (HasASentenceOfItsOwn(result))
        {
            continue;
        }

        QCOMPARE(WhyItWasNotInstalled(result, SourceKind::Archive), Explain(result));
        QCOMPARE(WhyItWasNotInstalled(result, SourceKind::Folder), Explain(result));
    }

    QVERIFY(!WhyItWasNotInstalled(FileResult::CouldNotCreateLink, SourceKind::Folder).isEmpty());
}

void ArchiveRefusalTextTest::NoSentenceCarriesADash()
{
    const QString emDash = QString(QChar(0x2014));
    const QString enDash = QString(QChar(0x2013));

    QStringList sentences = EverySentence();
    sentences << TheExtractionFolderStayed(SourceKind::Archive) << TheExtractionFolderStayed(SourceKind::Folder);

    for (const QString& sentence : sentences)
    {
        QVERIFY2(!sentence.contains(emDash), qPrintable(sentence));
        QVERIFY2(!sentence.contains(enDash), qPrintable(sentence));
    }
}

void ArchiveRefusalTextTest::NoSentenceClaimsARemovalNobodyChecked()
{
    for (const QString& sentence : EverySentence())
    {
        QVERIFY2(!sentence.contains(QStringLiteral("removed")), qPrintable(sentence));
        QVERIFY2(!sentence.contains(QStringLiteral("stayed in the library")), qPrintable(sentence));
    }

    QCOMPARE(WhyTheArchiveIsRefused(ArchiveFailure::CouldNotExtract, QStringLiteral("pack.zip"), SourceKind::Archive),
             QStringLiteral("Extracting the files failed. Nothing was installed."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::VerificationFailed, SourceKind::Archive),
             QStringLiteral("The extracted files do not match the archive's contents. Nothing was installed."));
}

void ArchiveRefusalTextTest::ATimeOutServesTheReadingAndTheExtraction()
{
    const QString sentence =
        WhyTheArchiveIsRefused(ArchiveFailure::TookTooLong, QStringLiteral("pack.zip"), SourceKind::Archive);

    QVERIFY(sentence.contains(QStringLiteral("read")));
    QVERIFY(sentence.contains(QStringLiteral("extract")));
}

void ArchiveRefusalTextTest::WhenTheDiskOrTheSourceStopsAPackageTheSentenceSaysWhatToDo()
{
    const QString changed = WhyItWasNotInstalled(FileResult::TheDiskDisagreesWithTheScan, SourceKind::Archive);
    const QString changedFolder = WhyItWasNotInstalled(FileResult::TheDiskDisagreesWithTheScan, SourceKind::Folder);

    QVERIFY(changed.contains(QStringLiteral("changed")));
    QVERIFY(changed.contains(QStringLiteral("Drop the archive again")));
    QVERIFY(changedFolder.contains(QStringLiteral("Drop the folder again")));
}

void ArchiveRefusalTextTest::TheFolderThatStayedBehindNamesTheScreenThatLetsYouDiscardIt()
{
    for (const SourceKind source : kSources)
    {
        const QString sentence = TheExtractionFolderStayed(source);

        QVERIFY(sentence.contains(QStringLiteral("Unfinished imports")));
        QVERIFY(!sentence.contains(QString(QChar(0x2014))));
    }
}

void ArchiveRefusalTextTest::TheSentencesSpeakPortugueseWhenTheCatalogueIsLoaded()
{
    const QString english =
        WhyTheArchiveIsRefused(ArchiveFailure::NeedsAPassword, QStringLiteral("pack.zip"), SourceKind::Archive);
    const QString englishOfTheLimit =
        WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("pack.7z"), SourceKind::Archive);

    QTranslator catalogue;
    QVERIFY2(LoadedTheCatalogue(catalogue, QStringLiteral("pt_BR")),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    const QString said =
        WhyTheArchiveIsRefused(ArchiveFailure::NeedsAPassword, QStringLiteral("pack.zip"), SourceKind::Archive);
    const QString saidOfTheLimit =
        WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("pack.7z"), SourceKind::Archive);

    QVERIFY(said != english);
    QVERIFY(said.contains(QStringLiteral("senha")));
    QVERIFY(saidOfTheLimit != englishOfTheLimit);
    QVERIFY(saidOfTheLimit.contains(QStringLiteral("Windows 10")));
    QVERIFY(WhyItWasNotInstalled(FileResult::Cancelled, SourceKind::Archive).contains(QStringLiteral("cancelou")));
    QCOMPARE(WhyItWasNotInstalled(FileResult::CouldNotDiscard, SourceKind::Archive),
             QStringLiteral("O FS Organizer não conseguiu apagar uma pasta temporária que uma instalação anterior "
                            "deixou nesta categoria, então nada foi instalado."));
}

void ArchiveRefusalTextTest::TheSentencesOfTheEarlierRoundsSayItPlainly()
{
    constexpr SourceKind archive = SourceKind::Archive;

    QCOMPARE(WhyItWasNotInstalled(FileResult::Cancelled, archive),
             QStringLiteral("You cancelled, so nothing was installed."));
    QCOMPARE(WhyTheArchiveIsRefused(ArchiveFailure::Cancelled, QStringLiteral("pack.zip"), archive),
             QStringLiteral("You cancelled, so nothing was installed."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::CouldNotExtract, archive),
             QStringLiteral("Extracting the files failed. Nothing was installed."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::VerificationFailed, archive),
             QStringLiteral("The extracted files do not match the archive's contents. Nothing was installed."));
    QCOMPARE(WhyTheArchiveIsRefused(ArchiveFailure::TookTooLong, QStringLiteral("pack.zip"), archive),
             QStringLiteral("FS Organizer stopped reading or extracting it because it took too long."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::TheOriginIsOccupied, archive),
             QStringLiteral("The category already has a folder with this name that is not an addon. FS Organizer does "
                            "not replace it."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::CouldNotDiscard, archive),
             QStringLiteral("FS Organizer could not delete a temporary folder that an earlier installation left in "
                            "this category, so nothing was installed."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::TheDiskDisagreesWithTheScan, archive),
             QStringLiteral("The library or the archive changed after FS Organizer read them. Nothing was installed. "
                            "Drop the archive again to read it again."));
    QCOMPARE(TheExtractionFolderStayed(archive),
             QStringLiteral("FS Organizer could not delete the temporary folder it extracted into. On the next start "
                            "it lists that folder under \"Unfinished imports\", where you can discard it."));
}

void ArchiveRefusalTextTest::AFolderIsNeverToldItWasAnArchiveOrWasExtracted()
{
    constexpr SourceKind folder = SourceKind::Folder;

    QStringList sentences;

    for (const ArchiveFailure failure : kFailures)
    {
        if (failure != ArchiveFailure::NeedsAPassword && failure != ArchiveFailure::TheToolIsMissing)
        {
            sentences << WhyTheArchiveIsRefused(failure, QStringLiteral("Folder"), folder);
        }
    }

    for (const NothingToInstall refusal : kRefusals)
    {
        if (refusal != NothingToInstall::ThePackageHasNoFolder)
        {
            sentences << WhyTheArchiveIsRefused(refusal, folder);
        }
    }

    for (const FileResult result : kResultsWithASentence)
    {
        if (result != FileResult::TheArchiveNeedsAPassword)
        {
            sentences << WhyItWasNotInstalled(result, folder);
        }
    }

    sentences << TheExtractionFolderStayed(folder);

    for (const QString& sentence : sentences)
    {
        QVERIFY2(!sentence.contains(QStringLiteral("archive"), Qt::CaseInsensitive), qPrintable(sentence));
        QVERIFY2(!sentence.contains(QStringLiteral("extract"), Qt::CaseInsensitive), qPrintable(sentence));
        QVERIFY2(!sentence.contains(QStringLiteral("Windows could not open")), qPrintable(sentence));
    }

    QCOMPARE(WhyItWasNotInstalled(FileResult::CouldNotExtract, folder),
             QStringLiteral("Copying the files failed. Nothing was installed."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::VerificationFailed, folder),
             QStringLiteral("The copied files do not match what was in the folder. Nothing was installed."));
    QCOMPARE(WhyTheArchiveIsRefused(ArchiveFailure::TookTooLong, QStringLiteral("Folder"), folder),
             QStringLiteral("FS Organizer stopped reading or copying it because it took too long."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::NotEnoughFreeSpace, folder),
             QStringLiteral("There is not enough free space in the library to copy it."));
    QCOMPARE(TheExtractionFolderStayed(folder),
             QStringLiteral("FS Organizer could not delete the temporary folder it copied into. On the next start it "
                            "lists that folder under \"Unfinished imports\", where you can discard it."));
}

void ArchiveRefusalTextTest::AFolderThatCouldNotBeReadIsNotCalledDamaged()
{
    const QString sentence =
        WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("Addon.7z"), SourceKind::Folder);

    QCOMPARE(sentence,
             QStringLiteral("FS Organizer could not read this folder. It is a link, a drive root, or a folder it "
                            "cannot open."));
    QVERIFY(!sentence.contains(QStringLiteral("Windows 10")));
}

void ArchiveRefusalTextTest::AFolderTellsLinksAndNamesApart()
{
    const QString folder = WhyTheArchiveIsRefused(NothingToInstall::AnEntryIsNotSafe, SourceKind::Folder);
    const QString archive = WhyTheArchiveIsRefused(NothingToInstall::AnEntryIsNotSafe, SourceKind::Archive);

    QCOMPARE(folder,
             QStringLiteral("It holds something unsafe to copy: a link or junction, or a name Windows cannot write."));
    QVERIFY(folder != archive);
    QVERIFY(!folder.contains(QStringLiteral("absolute path")));
}

void ArchiveRefusalTextTest::TheSentenceAboutASourceInTheWayHoldsInBothDirectionsForAnArchiveAndAFolder()
{
    const QString archive = WhyItWasNotInstalled(FileResult::TheSourceIsInsideWhatWouldBeRemoved, SourceKind::Archive);
    const QString folder = WhyItWasNotInstalled(FileResult::TheSourceIsInsideWhatWouldBeRemoved, SourceKind::Folder);

    QCOMPARE(archive, folder);
    QCOMPARE(folder,
             QStringLiteral("What you dropped is inside a folder that installing would remove, or contains one. It "
                            "was not installed. Move it somewhere else and try again."));
}

void ArchiveRefusalTextTest::TheSentencesOfAFolderSpeakPortugueseWhenTheCatalogueIsLoaded()
{
    QTranslator catalogue;
    QVERIFY2(LoadedTheCatalogue(catalogue, QStringLiteral("pt_BR")),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    QCOMPARE(WhyItWasNotInstalled(FileResult::CouldNotExtract, SourceKind::Folder),
             QStringLiteral("A cópia dos arquivos falhou. Nada foi instalado."));
    QCOMPARE(WhyItWasNotInstalled(FileResult::TheSourceIsInsideWhatWouldBeRemoved, SourceKind::Folder),
             QStringLiteral("O que você soltou está dentro de uma pasta que a instalação apagaria, ou contém uma. Não "
                            "foi instalado. Mova para outro lugar e tente de novo."));
    QCOMPARE(WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("Pasta"), SourceKind::Folder),
             QStringLiteral("O FS Organizer não conseguiu ler esta pasta. Ela é um link, a raiz de uma unidade ou uma "
                            "pasta que ele não pode abrir."));
    QCOMPARE(TheExtractionFolderStayed(SourceKind::Folder),
             QStringLiteral("O FS Organizer não conseguiu apagar a pasta temporária para a qual copiou. Na próxima "
                            "vez que abrir, ele lista essa pasta em \"Importações incompletas\", onde você pode "
                            "descartá-la."));
}

QTEST_GUILESS_MAIN(ArchiveRefusalTextTest)

#include "tst_archive_refusal_text.moc"
