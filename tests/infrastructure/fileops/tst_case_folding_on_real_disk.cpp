#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <windows.h>

#include <string>
#include <vector>

#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class CaseFoldingOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheDiskAndTheDomainAgreeOnEveryLetterOfTheDeclaredRanges();
        static void ALetterOutsideTheDeclaredRangesIsLeftAloneOnPurpose();
    };
}

namespace
{
    constexpr int kLettersTheSweepMustReach = 150;

    struct Range
    {
        char32_t from = 0;
        char32_t to = 0;
    };

    const std::vector<Range> kDeclaredRanges = {{.from = 0x0041, .to = 0x005A},
                                                {.from = 0x00C0, .to = 0x00FF},
                                                {.from = 0x0100, .to = 0x017F},
                                                {.from = 0x0386, .to = 0x03CE},
                                                {.from = 0x0400, .to = 0x045F}};

    [[nodiscard]] wchar_t LoweredByWindows(const wchar_t letter)
    {
        wchar_t buffer[2] = {letter, L'\0'};
        CharLowerBuffW(static_cast<LPWSTR>(buffer), 1);

        return buffer[0];
    }

    [[nodiscard]] std::filesystem::path FolderNamed(const std::filesystem::path& root, const wchar_t letter)
    {
        return root / (std::wstring(L"addon-") + letter + L"-pack");
    }
}

void CaseFoldingOnRealDiskTest::TheDiskAndTheDomainAgreeOnEveryLetterOfTheDeclaredRanges()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::filesystem::path root{directory.path().toStdString()};
    QStringList disagreements;
    int measured = 0;

    for (const Range& range : kDeclaredRanges)
    {
        for (char32_t code = range.from; code <= range.to; ++code)
        {
            const auto upper = static_cast<wchar_t>(code);
            const wchar_t lower = LoweredByWindows(upper);

            if (lower == upper)
            {
                continue;
            }

            const std::filesystem::path written = FolderNamed(root, upper);
            const std::filesystem::path spelledLower = FolderNamed(root, lower);

            std::filesystem::remove_all(written);
            QVERIFY(std::filesystem::create_directory(written));

            const bool theDiskSaysTheSameFolder = std::filesystem::exists(spelledLower);
            const bool theDomainSaysTheSameFolder = ComparablePath(written) == ComparablePath(spelledLower);
            ++measured;

            if (theDiskSaysTheSameFolder != theDomainSaysTheSameFolder)
            {
                disagreements.append(QStringLiteral("U+%1 (disk %2, domain %3)")
                                         .arg(static_cast<uint>(code), 4, 16, QLatin1Char('0'))
                                         .arg(theDiskSaysTheSameFolder)
                                         .arg(theDomainSaysTheSameFolder));
            }

            std::filesystem::remove_all(written);
        }
    }

    QVERIFY(measured > kLettersTheSweepMustReach);
    QVERIFY2(disagreements.isEmpty(), qPrintable(disagreements.join(QStringLiteral(", "))));
}

void CaseFoldingOnRealDiskTest::ALetterOutsideTheDeclaredRangesIsLeftAloneOnPurpose()
{
    const auto outside = static_cast<char32_t>(0x01A0);

    QCOMPARE(LoweredByWindows(static_cast<wchar_t>(outside)), static_cast<wchar_t>(0x01A1));
    QCOMPARE(LoweredCodePoint(outside), outside);
}

QTEST_MAIN(CaseFoldingOnRealDiskTest)

#include "tst_case_folding_on_real_disk.moc"
