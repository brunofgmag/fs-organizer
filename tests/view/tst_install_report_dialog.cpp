#include <QtTest/QtTest>

#include <QtCore/QTranslator>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>

#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/InstalledCatalogue.h"
#include "view/library/InstallReportDialog.h"
#include "viewmodel/ArchiveRefusalText.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class InstallReportDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void initTestCase();
        static void TheDialogIsFiveHundredTwentyPixelsWide();
        static void OneArchiveWithOneLineIsAsTallAsItsContentAndNotTheFloorOfAScrollArea();
        static void ReasonsWrapAndAreNeverCut_data();
        static void ReasonsWrapAndAreNeverCut();
        static void EachKindHasItsTitle_data();
        static void EachKindHasItsTitle();
        static void EachKindHasItsHeadlineOrNone_data();
        static void EachKindHasItsHeadlineOrNone();
        static void EachKindMakesItsPromiseOrNone_data();
        static void EachKindMakesItsPromiseOrNone();
        static void AnAddonNameIsShownOnlyWhenTheLineHasOne();
        static void TheFolderThatStayedBehindIsToldBelowTheLinesOfItsArchiveInEitherLanguage_data();
        static void TheFolderThatStayedBehindIsToldBelowTheLinesOfItsArchiveInEitherLanguage();
        static void NothingWasInstalledNoLongerPromisesThatNothingChangedWhenAFolderStayedBehind();
        static void TheDialogIsNeverTallerThanTheWindowItOpensOver();
        static void CloseEndsTheDialog();
    };

    constexpr int kWide = 520;
    constexpr int kFloorOfAScrollArea = 68;

    [[nodiscard]] QStringList TheLongestReasonsTheAppGives()
    {
        return {WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("scenery.7z"), SourceKind::Archive),
                WhyTheArchiveIsRefused(NothingToInstall::AnEntryIsNotSafe, SourceKind::Archive),
                WhyTheArchiveIsRefused(NothingToInstall::ThePackageHasNoFolder, SourceKind::Archive),
                WhyItWasNotInstalled(FileResult::VerificationFailed, SourceKind::Archive),
                WhyItWasNotInstalled(FileResult::CouldNotDiscard, SourceKind::Archive)};
    }

    [[nodiscard]] QTranslator& TheEnglishOfTheApp()
    {
        static QTranslator english;

        return english;
    }

    void ShowItSettled(QDialog& dialog)
    {
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCoreApplication::processEvents();
    }

    [[nodiscard]] InstallReport ReportOf(const InstallReportKind kind, const QStringList& reasons, const int archives)
    {
        InstallReport report{.kind = kind};

        for (int at = 0; at < archives; ++at)
        {
            InstallReportArchive archive{.fileName = QStringLiteral("flytampa-amsterdam-v2.4.%1.zip").arg(at)};

            for (const QString& reason : reasons)
            {
                archive.lines.push_back({.addon = QString(), .reason = reason});
            }

            report.archives.push_back(archive);
        }

        return report;
    }

    [[nodiscard]] InstallReport OneArchiveOneLine(const InstallReportKind kind)
    {
        return {.kind = kind,
                .archives = {{.fileName = QStringLiteral("scenery.zip"),
                              .lines = {{.addon = QString(), .reason = QStringLiteral("It needs a password.")}}}}};
    }

    [[nodiscard]] QLabel* LabelNamed(const QDialog& dialog, const QString& name)
    {
        return dialog.findChild<QLabel*>(name);
    }

    void AddEveryLanguage()
    {
        QTest::addColumn<QString>("language");

        QTest::newRow("English") << QStringLiteral("en");
        QTest::newRow("Brazilian Portuguese") << QStringLiteral("pt_BR");
    }

    [[nodiscard]] InstallReport
    ReportFor(const InstallReportKind kind, const int installed, const int notInstalled, const int lines)
    {
        InstallReport report = ReportOf(kind, TheLongestReasonsTheAppGives().mid(0, lines), 1);
        report.installed = installed;
        report.notInstalled = notInstalled;

        return report;
    }

    void AddEveryKind()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<InstallReportKind>("kind");
        QTest::addColumn<int>("installed");
        QTest::addColumn<int>("notInstalled");
        QTest::addColumn<int>("lines");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("headline");
        QTest::addColumn<QString>("promise");

        const QString promiseInEnglish =
            QStringLiteral("Nothing changed in the library, and what you dropped is where it was.");
        const QString promiseInPortuguese =
            QStringLiteral("Nada mudou na biblioteca, e o que você soltou continua onde estava.");

        QTest::newRow("en, nothing to install") << "en" << InstallReportKind::NothingToInstall << 0 << 0 << 1
                                                << "Nothing to install" << QString() << promiseInEnglish;
        QTest::newRow("en, nothing was installed") << "en" << InstallReportKind::NothingWasInstalled << 0 << 2 << 2
                                                   << "Nothing was installed" << QString() << promiseInEnglish;
        QTest::newRow("en, some were not installed")
            << "en" << InstallReportKind::SomeWereNotInstalled << 2 << 1 << 1 << "Some addons were not installed"
            << "2 addons are now in the library. 1 addon was not installed:" << QString();
        QTest::newRow("en, one is in and two were not installed")
            << "en" << InstallReportKind::SomeWereNotInstalled << 1 << 2 << 2 << "Some addons were not installed"
            << "1 addon is now in the library. 2 addons were not installed:" << QString();
        QTest::newRow("en, installed but not linked, one")
            << "en" << InstallReportKind::InstalledButNotLinked << 2 << 0 << 1 << "Installed, but not enabled"
            << "1 addon is in the library, but its links could not be made again:" << QString();
        QTest::newRow("en, installed but not linked, three")
            << "en" << InstallReportKind::InstalledButNotLinked << 5 << 0 << 3 << "Installed, but not enabled"
            << "3 addons are in the library, but their links could not be made again:" << QString();

        QTest::newRow("en, installed but a folder stayed")
            << "en" << InstallReportKind::InstalledButAFolderStayed << 2 << 0 << 0
            << "Installed, but a temporary folder was left behind" << "2 addons are now in the library." << QString();
        QTest::newRow("pt_BR, installed but a folder stayed")
            << "pt_BR" << InstallReportKind::InstalledButAFolderStayed << 2 << 0 << 0
            << "Instalado, mas uma pasta temporária ficou para trás" << "2 addons agora estão na biblioteca."
            << QString();

        QTest::newRow("pt_BR, nothing to install") << "pt_BR" << InstallReportKind::NothingToInstall << 0 << 0 << 1
                                                   << "Nada para instalar" << QString() << promiseInPortuguese;
        QTest::newRow("pt_BR, nothing was installed") << "pt_BR" << InstallReportKind::NothingWasInstalled << 0 << 2
                                                      << 2 << "Nada foi instalado" << QString() << promiseInPortuguese;
        QTest::newRow("pt_BR, some were not installed")
            << "pt_BR" << InstallReportKind::SomeWereNotInstalled << 2 << 1 << 1 << "Alguns addons não foram instalados"
            << "2 addons agora estão na biblioteca. 1 addon não foi instalado:" << QString();
        QTest::newRow("pt_BR, installed but not linked, one")
            << "pt_BR" << InstallReportKind::InstalledButNotLinked << 2 << 0 << 1 << "Instalado, mas não ativado"
            << "1 addon está na biblioteca, mas os links dele não puderam ser refeitos:" << QString();
        QTest::newRow("pt_BR, installed but not linked, three")
            << "pt_BR" << InstallReportKind::InstalledButNotLinked << 5 << 0 << 3 << "Instalado, mas não ativado"
            << "3 addons estão na biblioteca, mas os links deles não puderam ser refeitos:" << QString();
    }
}

void InstallReportDialogTest::initTestCase()
{
    ApplyModernistTheme(*qApp);

    QVERIFY2(LoadedTheShippedCatalogue(TheEnglishOfTheApp(), QStringLiteral("en")),
             "app_en.qm is not beside the build: build the release_translations target");
    QCoreApplication::installTranslator(&TheEnglishOfTheApp());
}

void InstallReportDialogTest::TheDialogIsFiveHundredTwentyPixelsWide()
{
    for (const InstallReportKind kind :
         {InstallReportKind::NothingToInstall, InstallReportKind::SomeWereNotInstalled,
          InstallReportKind::NothingWasInstalled, InstallReportKind::InstalledButNotLinked,
          InstallReportKind::InstalledButAFolderStayed})
    {
        InstallReportDialog dialog(ReportOf(kind, TheLongestReasonsTheAppGives(), 3));
        ShowItSettled(dialog);

        QCOMPARE(dialog.width(), kWide);
    }
}

void InstallReportDialogTest::OneArchiveWithOneLineIsAsTallAsItsContentAndNotTheFloorOfAScrollArea()
{
    InstallReportDialog dialog(OneArchiveOneLine(InstallReportKind::NothingToInstall));
    ShowItSettled(dialog);

    const QScrollArea* list = dialog.findChild<QScrollArea*>(QStringLiteral("ReportList"));

    QVERIFY(list != nullptr);
    QCOMPARE(list->viewport()->height(), list->widget()->height());
    QVERIFY2(list->height() < kFloorOfAScrollArea,
             qPrintable(QStringLiteral("a short report is %1 px tall in its list").arg(list->height())));
}

void InstallReportDialogTest::ReasonsWrapAndAreNeverCut_data()
{
    AddEveryLanguage();
}

void InstallReportDialogTest::ReasonsWrapAndAreNeverCut()
{
    QFETCH(QString, language);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    const QStringList reasons = TheLongestReasonsTheAppGives();

    for (const InstallReportKind kind :
         {InstallReportKind::NothingToInstall, InstallReportKind::SomeWereNotInstalled,
          InstallReportKind::NothingWasInstalled, InstallReportKind::InstalledButNotLinked,
          InstallReportKind::InstalledButAFolderStayed})
    {
        InstallReportDialog dialog(ReportOf(kind, reasons, 3));
        ShowItSettled(dialog);

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            if (!label->isVisible())
            {
                continue;
            }

            QVERIFY2(label->wordWrap(), qPrintable(QStringLiteral("this text does not wrap: %1").arg(label->text())));
            QVERIFY2(label->height() >= label->heightForWidth(label->width()),
                     qPrintable(QStringLiteral("this wrapped text is cut: %1").arg(label->text())));
        }
    }
}

void InstallReportDialogTest::EachKindHasItsTitle_data()
{
    AddEveryKind();
}

void InstallReportDialogTest::EachKindHasItsHeadlineOrNone_data()
{
    AddEveryKind();
}

void InstallReportDialogTest::EachKindMakesItsPromiseOrNone_data()
{
    AddEveryKind();
}

void InstallReportDialogTest::EachKindHasItsTitle()
{
    QFETCH(QString, language);
    QFETCH(InstallReportKind, kind);
    QFETCH(int, installed);
    QFETCH(int, notInstalled);
    QFETCH(int, lines);
    QFETCH(QString, title);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installedCatalogue(catalogue);

    InstallReportDialog dialog(ReportFor(kind, installed, notInstalled, lines));

    QCOMPARE(dialog.windowTitle(), title);
}

void InstallReportDialogTest::EachKindHasItsHeadlineOrNone()
{
    QFETCH(QString, language);
    QFETCH(InstallReportKind, kind);
    QFETCH(int, installed);
    QFETCH(int, notInstalled);
    QFETCH(int, lines);
    QFETCH(QString, headline);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installedCatalogue(catalogue);

    InstallReportDialog dialog(ReportFor(kind, installed, notInstalled, lines));
    ShowItSettled(dialog);

    const QLabel* said = LabelNamed(dialog, QStringLiteral("ReportHeadline"));

    if (headline.isEmpty())
    {
        QVERIFY2(said == nullptr, "this kind has no headline");

        return;
    }

    QVERIFY(said != nullptr);
    QCOMPARE(said->text(), headline);
}

void InstallReportDialogTest::EachKindMakesItsPromiseOrNone()
{
    QFETCH(QString, language);
    QFETCH(InstallReportKind, kind);
    QFETCH(int, installed);
    QFETCH(int, notInstalled);
    QFETCH(int, lines);
    QFETCH(QString, promise);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installedCatalogue(catalogue);

    InstallReportDialog dialog(ReportFor(kind, installed, notInstalled, lines));
    ShowItSettled(dialog);

    const QLabel* promised = LabelNamed(dialog, QStringLiteral("ReportPromise"));

    if (promise.isEmpty())
    {
        QVERIFY2(promised == nullptr, "this kind makes no promise: something was installed");

        return;
    }

    QVERIFY(promised != nullptr);
    QCOMPARE(promised->text(), promise);
}

void InstallReportDialogTest::AnAddonNameIsShownOnlyWhenTheLineHasOne()
{
    InstallReport report{.kind = InstallReportKind::SomeWereNotInstalled,
                         .archives = {{.fileName = QStringLiteral("pack.zip"),
                                       .lines = {{.addon = QStringLiteral("airport-eham"),
                                                  .reason = QStringLiteral("The simulator is running.")},
                                                 {.addon = QString(), .reason = QStringLiteral("Needs a password.")}}}},
                         .installed = 1,
                         .notInstalled = 2};

    InstallReportDialog dialog(report);
    ShowItSettled(dialog);

    const QList<QLabel*> names = dialog.findChildren<QLabel*>(QStringLiteral("ReportAddon"));
    const QList<QLabel*> reasons = dialog.findChildren<QLabel*>(QStringLiteral("ReportReason"));

    QCOMPARE(names.size(), 1);
    QCOMPARE(names.first()->text(), QStringLiteral("airport-eham"));
    QCOMPARE(reasons.size(), 2);
}

void InstallReportDialogTest::TheFolderThatStayedBehindIsToldBelowTheLinesOfItsArchiveInEitherLanguage_data()
{
    AddEveryLanguage();
}

void InstallReportDialogTest::TheFolderThatStayedBehindIsToldBelowTheLinesOfItsArchiveInEitherLanguage()
{
    QFETCH(QString, language);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    const QString sentence = TheExtractionFolderStayed(SourceKind::Archive);

    QVERIFY(!sentence.isEmpty());

    InstallReport report{.kind = InstallReportKind::SomeWereNotInstalled,
                         .archives = {{.fileName = QStringLiteral("pack.zip"),
                                       .lines = {{.addon = QStringLiteral("airport-eham"),
                                                  .reason = QStringLiteral("The simulator is running.")}},
                                       .leftBehind = sentence}},
                         .installed = 1,
                         .notInstalled = 1};

    InstallReportDialog dialog(report);
    ShowItSettled(dialog);

    const QLabel* told = LabelNamed(dialog, QStringLiteral("ReportLeftBehind"));

    QVERIFY(told != nullptr);
    QCOMPARE(told->text(), sentence);
    QVERIFY(told->wordWrap());
    QVERIFY2(told->height() >= told->heightForWidth(told->width()), "the sentence is cut");

    const QLabel* reason = LabelNamed(dialog, QStringLiteral("ReportReason"));

    QVERIFY(reason != nullptr);
    QVERIFY(reason->mapTo(&dialog, QPoint()).y() < told->mapTo(&dialog, QPoint()).y());
    QCOMPARE(dialog.findChildren<QLabel*>(QStringLiteral("ReportLeftBehind")).size(), 1);
}

void InstallReportDialogTest::NothingWasInstalledNoLongerPromisesThatNothingChangedWhenAFolderStayedBehind()
{
    InstallReport report{.kind = InstallReportKind::NothingWasInstalled,
                         .archives = {{.fileName = QStringLiteral("pack.zip"),
                                       .lines = {{.addon = QString(), .reason = QStringLiteral("It failed.")}},
                                       .leftBehind = QStringLiteral("A folder stayed.")}},
                         .installed = 0,
                         .notInstalled = 1};

    InstallReportDialog dialog(report);
    ShowItSettled(dialog);

    QVERIFY(LabelNamed(dialog, QStringLiteral("ReportPromise")) == nullptr);

    report.archives.front().leftBehind.clear();
    InstallReportDialog promising(report);
    ShowItSettled(promising);

    QVERIFY(LabelNamed(promising, QStringLiteral("ReportPromise")) != nullptr);
}

void InstallReportDialogTest::TheDialogIsNeverTallerThanTheWindowItOpensOver()
{
    QWidget window;
    window.resize(1000, 400);
    window.show();

    InstallReportDialog dialog(ReportOf(InstallReportKind::NothingToInstall, TheLongestReasonsTheAppGives(), 8),
                               &window);
    ShowItSettled(dialog);

    QVERIFY2(
        dialog.height() <= window.height(),
        qPrintable(
            QStringLiteral("the dialog is %1 px tall over a window of %2").arg(dialog.height()).arg(window.height())));
}

void InstallReportDialogTest::CloseEndsTheDialog()
{
    InstallReportDialog dialog(OneArchiveOneLine(InstallReportKind::NothingToInstall));
    ShowItSettled(dialog);

    auto* close = dialog.findChild<QPushButton*>(QStringLiteral("CloseTheReport"));

    QVERIFY(close != nullptr);
    QVERIFY(close->isDefault());

    close->click();

    QVERIFY(!dialog.isVisible());
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

QTEST_MAIN(InstallReportDialogTest)

#include "tst_install_report_dialog.moc"
