#include <QtTest/QtTest>

#include "domain/importing/ExternalSidecar.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ExternalSidecarTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheRecordSitsBesideTheAddonAndNeverInsideIt();
        static void TheRecordIsNotADirectoryAndSoTheListingSkipsIt();
        static void WhatIsWrittenIsWhatIsReadBack();
        static void APathWithAnEqualsSignInItSurvivesTheRoundTrip();
        static void AnAccentedPathSurvivesTheRoundTrip();
        static void ARecordWrittenWithCarriageReturnsStillParses();
        static void TextThatIsNotARecordAnswersNothing();
        static void ARecordWithoutAPathAnswersNothing();
        static void ARecordFromAFutureVersionIsRefused();
        static void TheRecordOfTheQuarantineIsNotTheRecordOfTheOtherProgram();
    };
}

void ExternalSidecarTest::TheRecordSitsBesideTheAddonAndNeverInsideIt()
{
    const std::filesystem::path addon = "D:/MSFS 2024/Utilities/gsx-pro";
    const std::filesystem::path sidecar = ExternalSidecarPathFor(addon);

    QCOMPARE(sidecar.parent_path(), addon.parent_path());
    QVERIFY(!PathIsInside(sidecar, addon));
}

void ExternalSidecarTest::TheRecordIsNotADirectoryAndSoTheListingSkipsIt()
{
    QCOMPARE(ExternalSidecarPathFor("D:/MSFS 2024/Utilities/gsx-pro").filename(),
             std::filesystem::path{"gsx-pro.fsorg-external"});
}

void ExternalSidecarTest::WhatIsWrittenIsWhatIsReadBack()
{
    const std::filesystem::path written = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";

    const std::optional<std::filesystem::path> read = ExternalOriginFromText(TextOfTheExternalOrigin(written));

    QVERIFY(read.has_value());
    QCOMPARE(*read, written);
}

void ExternalSidecarTest::APathWithAnEqualsSignInItSurvivesTheRoundTrip()
{
    const std::filesystem::path written = "C:/Program Files/Vendor/a=b";

    const std::optional<std::filesystem::path> read = ExternalOriginFromText(TextOfTheExternalOrigin(written));

    QVERIFY(read.has_value());
    QCOMPARE(*read, written);
}

void ExternalSidecarTest::AnAccentedPathSurvivesTheRoundTrip()
{
    const std::string accented = "C:/Programas/Avi\xC3\xB5"
                                 "es/gsx-pro";
    const std::filesystem::path written = PathFromUtf8(accented);

    const std::string text = TextOfTheExternalOrigin(written);
    const std::optional<std::filesystem::path> read = ExternalOriginFromText(text);

    QVERIFY(text.find(accented) != std::string::npos);
    QVERIFY(read.has_value());
    QCOMPARE(*read, written);
}

void ExternalSidecarTest::ARecordWrittenWithCarriageReturnsStillParses()
{
    const std::optional<std::filesystem::path> read =
        ExternalOriginFromText("version=1\r\nexternal=C:/Program Files/Addon Manager/MSFS/gsx-pro\r\n");

    QVERIFY(read.has_value());
    QCOMPARE(*read, std::filesystem::path{"C:/Program Files/Addon Manager/MSFS/gsx-pro"});
}

void ExternalSidecarTest::TextThatIsNotARecordAnswersNothing()
{
    QVERIFY(!ExternalOriginFromText("").has_value());
    QVERIFY(!ExternalOriginFromText(R"({"external": "C:/Program Files/Addon Manager"})").has_value());
}

void ExternalSidecarTest::ARecordWithoutAPathAnswersNothing()
{
    QVERIFY(!ExternalOriginFromText("version=1\n").has_value());
    QVERIFY(!ExternalOriginFromText("version=1\nexternal=\n").has_value());
}

void ExternalSidecarTest::ARecordFromAFutureVersionIsRefused()
{
    QVERIFY(!ExternalOriginFromText("version=2\nexternal=C:/Program Files/Addon Manager/MSFS/gsx-pro\n").has_value());
}

void ExternalSidecarTest::TheRecordOfTheQuarantineIsNotTheRecordOfTheOtherProgram()
{
    QVERIFY(!ExternalOriginFromText("version=1\norigin=E:/Sim/Community/simbridge\n").has_value());
}

QTEST_APPLESS_MAIN(ExternalSidecarTest)

#include "tst_external_sidecar.moc"
