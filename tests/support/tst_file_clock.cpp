#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "support/FileClock.h"

namespace
{
    class FileClockTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheSameFileTimeConvertsToTheSameMomentEveryTime();
        static void TheConversionAgreesWithTheTwoClockFormulaForAFileWrittenNow();
        static void AKnownFileTimeConvertsToItsKnownUnixTime();
        static void AFileWrittenOnceAnswersTheSameMomentEveryTimeItIsRead();
    };
}

namespace
{
    constexpr int kConversions = 1000;
    constexpr std::chrono::milliseconds kAgreement{5};
    constexpr std::int64_t kUnixBillennium = 1'000'000'000;
    constexpr std::int64_t kFileTimeTicksAtTheUnixBillennium = 126'444'736'000'000'000;

    [[nodiscard]] std::chrono::system_clock::time_point
    TheTwoClockFormula(const std::filesystem::file_time_type written)
    {
        return std::chrono::system_clock::now()
            + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                   written - std::filesystem::file_time_type::clock::now());
    }

    [[nodiscard]] std::filesystem::path WrittenFile(const QTemporaryDir& directory)
    {
        const std::filesystem::path file = std::filesystem::path(directory.path().toStdWString()) / "written.txt";
        std::ofstream(file) << "written";

        return file;
    }
}

void FileClockTest::TheSameFileTimeConvertsToTheSameMomentEveryTime()
{
    const std::filesystem::file_time_type written = std::filesystem::file_time_type::clock::now();
    const std::chrono::system_clock::time_point first = SystemTimeOf(written);

    for (int conversion = 0; conversion < kConversions; ++conversion)
    {
        QCOMPARE(SystemTimeOf(written).time_since_epoch().count(), first.time_since_epoch().count());
    }
}

void FileClockTest::TheConversionAgreesWithTheTwoClockFormulaForAFileWrittenNow()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::filesystem::file_time_type written = std::filesystem::last_write_time(WrittenFile(directory));
    const std::chrono::system_clock::duration apart = SystemTimeOf(written) - TheTwoClockFormula(written);

    QVERIFY2(apart < kAgreement && -apart < kAgreement,
             qPrintable(QStringLiteral("the conversions are %1 us apart")
                            .arg(std::chrono::duration_cast<std::chrono::microseconds>(apart).count())));
}

void FileClockTest::AKnownFileTimeConvertsToItsKnownUnixTime()
{
    const std::filesystem::file_time_type billennium{
        std::filesystem::file_time_type::duration{kFileTimeTicksAtTheUnixBillennium}};

    const std::chrono::system_clock::time_point converted = SystemTimeOf(billennium);

    QCOMPARE(std::chrono::duration_cast<std::chrono::seconds>(converted.time_since_epoch()).count(), kUnixBillennium);
    QCOMPARE(converted, std::chrono::system_clock::time_point{std::chrono::seconds{kUnixBillennium}});
}

void FileClockTest::AFileWrittenOnceAnswersTheSameMomentEveryTimeItIsRead()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::filesystem::path file = WrittenFile(directory);
    const std::chrono::system_clock::time_point first = SystemTimeOf(std::filesystem::last_write_time(file));

    for (int reading = 0; reading < kConversions; ++reading)
    {
        QCOMPARE(SystemTimeOf(std::filesystem::last_write_time(file)).time_since_epoch().count(),
                 first.time_since_epoch().count());
    }
}

QTEST_APPLESS_MAIN(FileClockTest)

#include "tst_file_clock.moc"
