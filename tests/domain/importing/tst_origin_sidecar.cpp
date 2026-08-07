#include <QtTest/QtTest>

#include "domain/importing/OriginSidecar.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class OriginSidecarTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheRecordSitsBesideTheItemAndNeverInsideIt();
        static void TheRecordIsNotADirectoryAndSoTheListingSkipsIt();
        static void WhatIsWrittenIsWhatIsReadBack();
        static void AnOriginWithAnEqualsSignInItSurvivesTheRoundTrip();
        static void AnAccentedOriginSurvivesTheRoundTrip();
        static void ARecordWrittenWithCarriageReturnsStillParses();
        static void TextThatIsNotARecordAnswersNothing();
        static void ARecordWithoutAnOriginAnswersNothing();
        static void ARecordFromAFutureVersionIsRefused();
        static void ARecordWithoutAMomentStillCarriesItsOrigin();
    };

    constexpr auto kMoment = std::chrono::system_clock::time_point{std::chrono::milliseconds{1754582400000}};
}

void OriginSidecarTest::TheRecordSitsBesideTheItemAndNeverInsideIt()
{
    const std::filesystem::path item = "E:/Sim/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path sidecar = SidecarPathFor(item);

    QCOMPARE(sidecar.parent_path(), item.parent_path());
    QVERIFY(!PathIsInside(sidecar, item));
}

void OriginSidecarTest::TheRecordIsNotADirectoryAndSoTheListingSkipsIt()
{
    QCOMPARE(SidecarPathFor("E:/Sim/_fsorganizer-quarantine/simbridge").filename(),
             std::filesystem::path{"simbridge.fsorg-origin"});
}

void OriginSidecarTest::WhatIsWrittenIsWhatIsReadBack()
{
    const QuarantineOrigin written{.origin = "E:/Sim/Community/simbridge", .quarantinedAt = kMoment};

    const std::optional<QuarantineOrigin> read = OriginFromText(TextOfTheOrigin(written));

    QVERIFY(read.has_value());
    QCOMPARE(read->origin, written.origin);
    QCOMPARE(read->quarantinedAt.value(), kMoment);
}

void OriginSidecarTest::AnOriginWithAnEqualsSignInItSurvivesTheRoundTrip()
{
    const QuarantineOrigin written{.origin = "E:/Sim/Community/a=b", .quarantinedAt = kMoment};

    const std::optional<QuarantineOrigin> read = OriginFromText(TextOfTheOrigin(written));

    QVERIFY(read.has_value());
    QCOMPARE(read->origin, written.origin);
}

void OriginSidecarTest::AnAccentedOriginSurvivesTheRoundTrip()
{
    const std::string accented = "E:/Sim/Community/Avi\xC3\xB5"
                                 "es";
    const QuarantineOrigin written{.origin = PathFromUtf8(accented), .quarantinedAt = kMoment};

    const std::string text = TextOfTheOrigin(written);
    const std::optional<QuarantineOrigin> read = OriginFromText(text);

    QVERIFY(text.find(accented) != std::string::npos);
    QVERIFY(read.has_value());
    QCOMPARE(read->origin, written.origin);
}

void OriginSidecarTest::ARecordWrittenWithCarriageReturnsStillParses()
{
    const std::optional<QuarantineOrigin> read =
        OriginFromText("version=1\r\norigin=E:/Sim/Community/simbridge\r\nquarantined=1754582400000\r\n");

    QVERIFY(read.has_value());
    QCOMPARE(read->origin, std::filesystem::path{"E:/Sim/Community/simbridge"});
    QCOMPARE(read->quarantinedAt.value(), kMoment);
}

void OriginSidecarTest::TextThatIsNotARecordAnswersNothing()
{
    QVERIFY(!OriginFromText("").has_value());
    QVERIFY(!OriginFromText("{\"origin\": \"E:/Sim/Community/simbridge\"}").has_value());
}

void OriginSidecarTest::ARecordWithoutAnOriginAnswersNothing()
{
    QVERIFY(!OriginFromText("version=1\nquarantined=1754582400000\n").has_value());
    QVERIFY(!OriginFromText("version=1\norigin=\n").has_value());
}

void OriginSidecarTest::ARecordFromAFutureVersionIsRefused()
{
    QVERIFY(!OriginFromText("version=2\norigin=E:/Sim/Community/simbridge\n").has_value());
}

void OriginSidecarTest::ARecordWithoutAMomentStillCarriesItsOrigin()
{
    const std::optional<QuarantineOrigin> read = OriginFromText("version=1\norigin=E:/Sim/Community/simbridge\n");

    QVERIFY(read.has_value());
    QVERIFY(!read->quarantinedAt.has_value());
}

QTEST_APPLESS_MAIN(OriginSidecarTest)

#include "tst_origin_sidecar.moc"
