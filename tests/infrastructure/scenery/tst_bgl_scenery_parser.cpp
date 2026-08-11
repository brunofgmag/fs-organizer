#include <QtTest/QtTest>

#include <cstdint>
#include <vector>

#include "infrastructure/scenery/BglSceneryParser.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class BglSceneryParserTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheNewRecordIsPackedWithASixBitShiftAndTheOldOneWithFive();
        static void TheCodeComesOutOfTheRecordTheFileCarries();
        static void AFileWithoutTheSignatureIsNotABgl();
        static void AFileThatEndsBeforeItSaysItDoesIsReportedInsteadOfGuessed();
        static void AFileWithoutAnAirportSectionIsReadAndCarriesNoCode();
        static void ARecordWhoseIdentifierDidNotDecodeIsNotTheSameAsNoRecordAtAll();
        static void ARecordOfATypeThatCarriesNoIdentifierIsNotAFailureToRead();
    };

    constexpr std::uint32_t kRctpFromTheNewRecord = 0x0626E941;
    constexpr std::uint32_t kEhamFromTheOldRecord = 0x01BA5181;
    constexpr std::uint32_t kLpmaFromTheOldRecord = 0x027BBA01;
    constexpr std::uint32_t kPackedWithANullPosition = 39u << 6;

    constexpr std::size_t kHeader = 56;
    constexpr std::size_t kSection = kHeader;
    constexpr std::size_t kSubsection = kSection + 20;
    constexpr std::size_t kRecord = kSubsection + 16;
    constexpr std::size_t kPayload = 128;

    void PutDoubleWord(std::vector<std::uint8_t>& bytes, const std::size_t at, const std::uint32_t value)
    {
        bytes[at] = static_cast<std::uint8_t>(value & 0xFF);
        bytes[at + 1] = static_cast<std::uint8_t>(value >> 8 & 0xFF);
        bytes[at + 2] = static_cast<std::uint8_t>(value >> 16 & 0xFF);
        bytes[at + 3] = static_cast<std::uint8_t>(value >> 24 & 0xFF);
    }

    void PutWord(std::vector<std::uint8_t>& bytes, const std::size_t at, const std::uint16_t value)
    {
        bytes[at] = static_cast<std::uint8_t>(value & 0xFF);
        bytes[at + 1] = static_cast<std::uint8_t>(value >> 8 & 0xFF);
    }

    [[nodiscard]] std::vector<std::uint8_t> FileCarrying(const std::uint16_t record,
                                                         const std::size_t identifierAt,
                                                         const std::uint32_t packed,
                                                         const std::uint32_t sectionType = 3,
                                                         const std::size_t recordSize = kPayload)
    {
        std::vector<std::uint8_t> bytes(kRecord + kPayload, 0);

        PutDoubleWord(bytes, 0, 0x19920201);
        PutDoubleWord(bytes, 4, static_cast<std::uint32_t>(kHeader));
        PutDoubleWord(bytes, 20, 1);

        PutDoubleWord(bytes, kSection, sectionType);
        PutDoubleWord(bytes, kSection + 8, 1);
        PutDoubleWord(bytes, kSection + 12, static_cast<std::uint32_t>(kSubsection));

        PutDoubleWord(bytes, kSubsection + 4, 1);
        PutDoubleWord(bytes, kSubsection + 8, static_cast<std::uint32_t>(kRecord));
        PutDoubleWord(bytes, kSubsection + 12, static_cast<std::uint32_t>(kPayload));

        PutWord(bytes, kRecord, record);
        PutDoubleWord(bytes, kRecord + 2, static_cast<std::uint32_t>(recordSize));
        PutDoubleWord(bytes, kRecord + identifierAt, packed);

        return bytes;
    }
}

void BglSceneryParserTest::TheNewRecordIsPackedWithASixBitShiftAndTheOldOneWithFive()
{
    QCOMPARE(QString::fromStdString(AirportCodeFrom(kRctpFromTheNewRecord, 6)), QStringLiteral("RCTP"));
    QCOMPARE(QString::fromStdString(AirportCodeFrom(kEhamFromTheOldRecord, 5)), QStringLiteral("EHAM"));
    QCOMPARE(QString::fromStdString(AirportCodeFrom(kLpmaFromTheOldRecord, 5)), QStringLiteral("LPMA"));

    QVERIFY2(AirportCodeFrom(kRctpFromTheNewRecord, 5) != "RCTP",
             "the shift the old record uses answered the new one, so the two are not being told apart");
}

void BglSceneryParserTest::TheCodeComesOutOfTheRecordTheFileCarries()
{
    const BglSceneryParser parser;

    const SceneryCodes newFormat = parser.Parse(FileCarrying(0x113, 76, kRctpFromTheNewRecord));

    QCOMPARE(newFormat.reading, SceneryReading::Read);
    QCOMPARE(newFormat.codes.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(newFormat.codes.front()), QStringLiteral("RCTP"));

    const SceneryCodes oldFormat = parser.Parse(FileCarrying(0x56, 40, kEhamFromTheOldRecord));

    QCOMPARE(oldFormat.codes.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(oldFormat.codes.front()), QStringLiteral("EHAM"));
}

void BglSceneryParserTest::AFileWithoutTheSignatureIsNotABgl()
{
    const BglSceneryParser parser;

    std::vector<std::uint8_t> bytes = FileCarrying(0x113, 76, kRctpFromTheNewRecord);
    PutDoubleWord(bytes, 0, 0x19920202);

    QCOMPARE(parser.Parse(bytes).reading, SceneryReading::ItCarriesNoSignature);
    QCOMPARE(parser.Parse({}).reading, SceneryReading::ItCarriesNoSignature);
}

void BglSceneryParserTest::AFileThatEndsBeforeItSaysItDoesIsReportedInsteadOfGuessed()
{
    const BglSceneryParser parser;

    std::vector<std::uint8_t> bytes = FileCarrying(0x113, 76, kRctpFromTheNewRecord);
    bytes.resize(kRecord + 8);

    const SceneryCodes cut = parser.Parse(bytes);

    QCOMPARE(cut.reading, SceneryReading::ItEndsBeforeItSaysItDoes);
    QVERIFY(cut.codes.empty());
}

void BglSceneryParserTest::AFileWithoutAnAirportSectionIsReadAndCarriesNoCode()
{
    const BglSceneryParser parser;

    const SceneryCodes modelLibrary = parser.Parse(FileCarrying(0x113, 76, kRctpFromTheNewRecord, 37));

    QCOMPARE(modelLibrary.reading, SceneryReading::Read);
    QVERIFY(modelLibrary.codes.empty());
}

void BglSceneryParserTest::ARecordWhoseIdentifierDidNotDecodeIsNotTheSameAsNoRecordAtAll()
{
    const BglSceneryParser parser;

    const SceneryCodes noRecord = parser.Parse(FileCarrying(0x113, 76, kRctpFromTheNewRecord, 37));

    QCOMPARE(noRecord.reading, SceneryReading::Read);
    QVERIFY(noRecord.codes.empty());
    QVERIFY2(!noRecord.anIdentifierDidNotDecode,
             "a file with no airport section carries no identifier, so nothing there failed to be read");

    const SceneryCodes nullDigit = parser.Parse(FileCarrying(0x113, 76, kPackedWithANullPosition));

    QCOMPARE(nullDigit.reading, SceneryReading::Read);
    QVERIFY(nullDigit.codes.empty());
    QVERIFY2(
        nullDigit.anIdentifierDidNotDecode,
        "the record was there and the identifier did not come out of it, which is not the same as not being there");

    const SceneryCodes zeroed = parser.Parse(FileCarrying(0x113, 76, 0));

    QVERIFY2(zeroed.anIdentifierDidNotDecode,
             "an identifier of zero was read and answered nothing, so it did not decode");

    const SceneryCodes tooShort = parser.Parse(FileCarrying(0x113, 76, kRctpFromTheNewRecord, 3, 40));

    QCOMPARE(tooShort.reading, SceneryReading::Read);
    QVERIFY2(tooShort.anIdentifierDidNotDecode,
             "the record says it is 40 bytes long and the identifier of its type lives at 76, so it was not read");
}

void BglSceneryParserTest::ARecordOfATypeThatCarriesNoIdentifierIsNotAFailureToRead()
{
    const BglSceneryParser parser;

    const SceneryCodes otherRecord = parser.Parse(FileCarrying(0x5A, 76, kRctpFromTheNewRecord));

    QCOMPARE(otherRecord.reading, SceneryReading::Read);
    QVERIFY(otherRecord.codes.empty());
    QVERIFY2(!otherRecord.anIdentifierDidNotDecode,
             "the 0x5A is the commonest record of the reference installation and answers something else: calling it "
             "unread would put almost every addon in the unread state");
}

QTEST_APPLESS_MAIN(BglSceneryParserTest)

#include "tst_bgl_scenery_parser.moc"
