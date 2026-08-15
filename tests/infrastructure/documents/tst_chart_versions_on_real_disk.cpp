#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "infrastructure/documents/QtPdfChartVersions.h"
#include "tests/support/APdf.h"

namespace
{
    class ChartVersionsOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheVersionComesOutOfTheTitleThePublisherWroteInUtf16();
        static void ATitleWrittenInPlainAsciiAnswersTheSameNumber();
        static void APdfWhoseTitleSaysNothingAboutAVersionAnswersNothing();
        static void AFileThatIsNotAPdfAnswersNothingInsteadOfFailing();
    };

    struct AFolderOfCharts
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Holding(const std::string& name, const std::string& content) const
        {
            const std::filesystem::path file = std::filesystem::path(directory.path().toStdWString()) / name;
            std::ofstream stream(file, std::ios::binary);
            stream.write(content.data(), static_cast<std::streamsize>(content.size()));

            return file;
        }
    };

    void ChartVersionsOnRealDiskTest::TheVersionComesOutOfTheTitleThePublisherWroteInUtf16()
    {
        const AFolderOfCharts charts;
        const QtPdfChartVersions versions;

        QCOMPARE(versions.VersionOf(
                     charts.Holding("53211.pdf", APdfWhoseInfoSays("/Title(" + AsAUtf16Literal("CV-1486377") + ")"))),
                 1486377LL);
    }

    void ChartVersionsOnRealDiskTest::ATitleWrittenInPlainAsciiAnswersTheSameNumber()
    {
        const AFolderOfCharts charts;
        const QtPdfChartVersions versions;

        QCOMPARE(versions.VersionOf(charts.Holding("53241.pdf", APdfWhoseInfoSays("/Title(CV-1455100)"))), 1455100LL);
    }

    void ChartVersionsOnRealDiskTest::APdfWhoseTitleSaysNothingAboutAVersionAnswersNothing()
    {
        const AFolderOfCharts charts;
        const QtPdfChartVersions versions;

        QCOMPARE(versions.VersionOf(charts.Holding("manual.pdf", APdfWhoseInfoSays("/Title(MegaAirport Brussels)"))),
                 0LL);
    }

    void ChartVersionsOnRealDiskTest::AFileThatIsNotAPdfAnswersNothingInsteadOfFailing()
    {
        const AFolderOfCharts charts;
        const QtPdfChartVersions versions;

        QCOMPARE(versions.VersionOf(charts.Holding("broken.pdf", "not a pdf at all")), 0LL);
    }
}

QTEST_APPLESS_MAIN(ChartVersionsOnRealDiskTest)

#include "tst_chart_versions_on_real_disk.moc"
