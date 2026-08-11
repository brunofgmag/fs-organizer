#include <QtTest/QtTest>

#include <cstdint>
#include <vector>

#include "shared/BglAirportCodes.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class BglAirportCodesTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheNewRecordIsPackedWithASixBitShiftAndTheOldOneWithFive();
        static void TheCodeComesOutOfTheRecordTheFileCarries();
        static void AFileWithoutTheSignatureIsNotABgl();
        static void AFileThatEndsBeforeItSaysItDoesIsReportedInsteadOfGuessed();
        static void AFileWithoutAnAirportSectionIsReadAndCarriesNoCode();
    };

    constexpr std::uint32_t kRctpFromTheNewRecord = 0x0626E941;
    constexpr std::uint32_t kEhamFromTheOldRecord = 0x01BA5181;
    constexpr std::uint32_t kLpmaFromTheOldRecord = 0x027BBA01;

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
                                                         const std::uint32_t sectionType = 3)
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
        PutDoubleWord(bytes, kRecord + 2, static_cast<std::uint32_t>(kPayload));
        PutDoubleWord(bytes, kRecord + identifierAt, packed);

        return bytes;
    }
}

void BglAirportCodesTest::TheNewRecordIsPackedWithASixBitShiftAndTheOldOneWithFive()
{
    QCOMPARE(QString::fromStdString(AirportCodeFrom(kRctpFromTheNewRecord, 6)), QStringLiteral("RCTP"));
    QCOMPARE(QString::fromStdString(AirportCodeFrom(kEhamFromTheOldRecord, 5)), QStringLiteral("EHAM"));
    QCOMPARE(QString::fromStdString(AirportCodeFrom(kLpmaFromTheOldRecord, 5)), QStringLiteral("LPMA"));

    QVERIFY2(AirportCodeFrom(kRctpFromTheNewRecord, 5) != "RCTP",
             "the shift the old record uses answered the new one, so the two are not being told apart");
}

void BglAirportCodesTest::TheCodeComesOutOfTheRecordTheFileCarries()
{
    const BglAirportCodes newFormat = AirportCodesIn(FileCarrying(0x113, 76, kRctpFromTheNewRecord));

    QCOMPARE(newFormat.reading, BglReading::Read);
    QCOMPARE(newFormat.codes.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(newFormat.codes.front()), QStringLiteral("RCTP"));

    const BglAirportCodes oldFormat = AirportCodesIn(FileCarrying(0x56, 40, kEhamFromTheOldRecord));

    QCOMPARE(oldFormat.codes.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(oldFormat.codes.front()), QStringLiteral("EHAM"));
}

void BglAirportCodesTest::AFileWithoutTheSignatureIsNotABgl()
{
    std::vector<std::uint8_t> bytes = FileCarrying(0x113, 76, kRctpFromTheNewRecord);
    PutDoubleWord(bytes, 0, 0x19920202);

    QCOMPARE(AirportCodesIn(bytes).reading, BglReading::ItCarriesNoSignature);
    QCOMPARE(AirportCodesIn({}).reading, BglReading::ItCarriesNoSignature);
}

void BglAirportCodesTest::AFileThatEndsBeforeItSaysItDoesIsReportedInsteadOfGuessed()
{
    std::vector<std::uint8_t> bytes = FileCarrying(0x113, 76, kRctpFromTheNewRecord);
    bytes.resize(kRecord + 8);

    const BglAirportCodes cut = AirportCodesIn(bytes);

    QCOMPARE(cut.reading, BglReading::ItEndsBeforeItSaysItDoes);
    QVERIFY(cut.codes.empty());
}

void BglAirportCodesTest::AFileWithoutAnAirportSectionIsReadAndCarriesNoCode()
{
    const BglAirportCodes modelLibrary = AirportCodesIn(FileCarrying(0x113, 76, kRctpFromTheNewRecord, 37));

    QCOMPARE(modelLibrary.reading, BglReading::Read);
    QVERIFY(modelLibrary.codes.empty());
}

QTEST_APPLESS_MAIN(BglAirportCodesTest)

#include "tst_bgl_airport_codes.moc"
