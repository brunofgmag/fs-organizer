#include <QtTest/QtTest>

#include "infrastructure/fileops/ExtendedPaths.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ExtendedPathsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAbsolutePathTakesTheCommonPrefix();
        static void AUncPathTakesTheUncPrefixAndNotTheCommonOne();
        static void APathThatAlreadyCarriesThePrefixIsLeftAlone();
        static void APathThatCannotBeReachedFromTheRootIsLeftAlone();
        static void TheRootOfAVolumeKeepsItsTrailingSeparator();
        static void SeparatorsAndDotSegmentsAreSettledBeforeThePrefixGoesOn();
        static void StrippingUndoesBothFormsAndSparesWhatNeverHadThem();
    };
}

namespace
{
    [[nodiscard]] QString Shown(const std::filesystem::path& path)
    {
        return QString::fromStdString(path.string());
    }

    [[nodiscard]] QString Prefixed(const std::filesystem::path& path)
    {
        return Shown(WithExtendedPrefix(path));
    }
}

void ExtendedPathsTest::AnAbsolutePathTakesTheCommonPrefix()
{
    QCOMPARE(Prefixed(R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)"),
             QStringLiteral(R"(\\?\D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)"));
}

void ExtendedPathsTest::AUncPathTakesTheUncPrefixAndNotTheCommonOne()
{
    const QString prefixed = Prefixed(R"(\\hangar\msfs\Library\aerosoft-crj)");

    QCOMPARE(prefixed, QStringLiteral(R"(\\?\UNC\hangar\msfs\Library\aerosoft-crj)"));
    QVERIFY2(!prefixed.startsWith(QStringLiteral(R"(\\?\\)")),
             "a network path prefixed as a local one names a server that does not exist");
}

void ExtendedPathsTest::APathThatAlreadyCarriesThePrefixIsLeftAlone()
{
    const QString once = Prefixed(R"(D:\MSFS 2024\Aircrafts)");

    QCOMPARE(Prefixed(once.toStdString()), once);
    QCOMPARE(Prefixed(R"(\\?\UNC\hangar\msfs)"), QStringLiteral(R"(\\?\UNC\hangar\msfs)"));
}

void ExtendedPathsTest::APathThatCannotBeReachedFromTheRootIsLeftAlone()
{
    QCOMPARE(Prefixed(R"(Aircrafts\pmdg-aircraft-77w)"), QStringLiteral(R"(Aircrafts\pmdg-aircraft-77w)"));
    QCOMPARE(Prefixed(R"(D:MSFS)"), QStringLiteral(R"(D:MSFS)"));
    QCOMPARE(Prefixed(""), QString());
}

void ExtendedPathsTest::TheRootOfAVolumeKeepsItsTrailingSeparator()
{
    QCOMPARE(Prefixed(R"(D:\)"), QStringLiteral(R"(\\?\D:\)"));
}

void ExtendedPathsTest::SeparatorsAndDotSegmentsAreSettledBeforeThePrefixGoesOn()
{
    QCOMPARE(Prefixed("D:/MSFS 2024/Aircrafts"), QStringLiteral(R"(\\?\D:\MSFS 2024\Aircrafts)"));
    QCOMPARE(Prefixed(R"(D:\MSFS 2024\Aircrafts\..\Sceneries)"), QStringLiteral(R"(\\?\D:\MSFS 2024\Sceneries)"));
}

void ExtendedPathsTest::StrippingUndoesBothFormsAndSparesWhatNeverHadThem()
{
    QCOMPARE(Shown(WithoutExtendedPrefix(R"(\\?\D:\MSFS 2024\Aircrafts)")),
             QStringLiteral(R"(D:\MSFS 2024\Aircrafts)"));
    QCOMPARE(Shown(WithoutExtendedPrefix(R"(\\?\UNC\hangar\msfs\Library)")),
             QStringLiteral(R"(\\hangar\msfs\Library)"));
    QCOMPARE(Shown(WithoutExtendedPrefix(R"(D:\MSFS 2024\Aircrafts)")), QStringLiteral(R"(D:\MSFS 2024\Aircrafts)"));

    const std::filesystem::path plain = R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)";
    QCOMPARE(Shown(WithoutExtendedPrefix(WithExtendedPrefix(plain))), Shown(plain));
}

QTEST_APPLESS_MAIN(ExtendedPathsTest)

#include "tst_extended_paths.moc"
