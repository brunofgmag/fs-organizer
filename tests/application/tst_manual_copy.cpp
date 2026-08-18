#include <QtTest/QtTest>

#include <string>

#include "application/ManualCopy.h"
#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ManualCopyTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheUrlAsksTheTagOfTheInstalledVersionAndNotTheBranch();
        static void AnInterfaceLanguageTheManualWasNeverWrittenInReadsTheEnglishOne();
        static void TheCopyOnDiskCarriesTheVersionSoAnUpdateDoesNotOpenTheOldManual();
        static void WhatTheFolderKeepsIsToldApartFromWhateverElseLandsThere();
    };

    const std::filesystem::path kFolder = PathFromUtf8("C:/Users/bruno/AppData/Local/fs-organizer/manual");
}

void ManualCopyTest::TheUrlAsksTheTagOfTheInstalledVersionAndNotTheBranch()
{
    QCOMPARE(ManualUrlFor("0.52.0", "pt_BR"),
             std::string("https://raw.githubusercontent.com/brunofgmag/fs-organizer/v0.52.0/docs/"
                         "fs-organizer-pt_BR.pdf"));
}

void ManualCopyTest::AnInterfaceLanguageTheManualWasNeverWrittenInReadsTheEnglishOne()
{
    QCOMPARE(ManualLanguageFor("de"), std::string("en"));
    QCOMPARE(ManualLanguageFor("pt_PT"), std::string("en"));
    QCOMPARE(ManualLanguageFor("pt_BR"), std::string("pt_BR"));

    QCOMPARE(ManualUrlFor("0.52.0", "de"),
             std::string("https://raw.githubusercontent.com/brunofgmag/fs-organizer/v0.52.0/docs/"
                         "fs-organizer-en.pdf"));
}

void ManualCopyTest::TheCopyOnDiskCarriesTheVersionSoAnUpdateDoesNotOpenTheOldManual()
{
    const std::filesystem::path older = ManualFileIn(kFolder, "0.52.0", "pt_BR");
    const std::filesystem::path newer = ManualFileIn(kFolder, "0.53.0", "pt_BR");

    QCOMPARE(older, PathUnder(kFolder, PathFromUtf8("fs-organizer-pt_BR-0.52.0.pdf")));
    QVERIFY(older != newer);
    QCOMPARE(ManualFileIn(kFolder, "0.52.0", "en"), PathUnder(kFolder, PathFromUtf8("fs-organizer-en-0.52.0.pdf")));
}

void ManualCopyTest::WhatTheFolderKeepsIsToldApartFromWhateverElseLandsThere()
{
    QVERIFY(ItIsAManualCopy(ManualFileIn(kFolder, "0.52.0", "pt_BR")));
    QVERIFY(!ItIsAManualCopy(PathUnder(kFolder, PathFromUtf8("aerosoft-crj-checklist.pdf"))));
    QVERIFY(!ItIsAManualCopy(PathUnder(kFolder, PathFromUtf8("fs-organizer-pt_BR-0.52.0.tmp"))));
}

QTEST_APPLESS_MAIN(ManualCopyTest)

#include "tst_manual_copy.moc"
