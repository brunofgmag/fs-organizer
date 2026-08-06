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
            const QString named = QStringLiteral("U+%1").arg(static_cast<uint>(code), 4, 16, QLatin1Char('0'));

            std::error_code error;
            std::filesystem::remove_all(written, error);

            if (!std::filesystem::create_directory(written, error))
            {
                disagreements.append(
                    QStringLiteral("%1 could not be written: %2").arg(named, QString::fromStdString(error.message())));

                continue;
            }

            const bool theDiskSaysTheSameFolder = std::filesystem::exists(spelledLower, error);

            try
            {
                const bool theDomainSaysTheSameFolder = ComparablePath(written) == ComparablePath(spelledLower);
                ++measured;

                if (theDiskSaysTheSameFolder != theDomainSaysTheSameFolder)
                {
                    disagreements.append(QStringLiteral("%1 (disk %2, domain %3)")
                                             .arg(named)
                                             .arg(theDiskSaysTheSameFolder)
                                             .arg(theDomainSaysTheSameFolder));
                }
            }
            catch (const std::exception& thrown)
            {
                disagreements.append(
                    QStringLiteral("%1 threw out of ComparablePath: %2").arg(named, QString::fromUtf8(thrown.what())));
            }

            std::filesystem::remove_all(written, error);
        }
    }

    const QString where = QStringLiteral("code page %1, %2 letters measured").arg(GetACP()).arg(measured);

    QVERIFY2(disagreements.isEmpty(),
             qPrintable(where + QStringLiteral(" -- ") + disagreements.join(QStringLiteral(", "))));
    QVERIFY2(measured > kLettersTheSweepMustReach, qPrintable(where));
}

void CaseFoldingOnRealDiskTest::ALetterOutsideTheDeclaredRangesIsLeftAloneOnPurpose()
{
    const auto outside = static_cast<char32_t>(0x01A0);

    QCOMPARE(LoweredByWindows(static_cast<wchar_t>(outside)), static_cast<wchar_t>(0x01A1));
    QCOMPARE(LoweredCodePoint(outside), outside);
}

QTEST_MAIN(CaseFoldingOnRealDiskTest)

#include "tst_case_folding_on_real_disk.moc"
