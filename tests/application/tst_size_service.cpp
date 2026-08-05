#include <QtTest/QtTest>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "application/SizeService.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const std::filesystem::path kLibrary = "D:/Library";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = {}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode LibraryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    struct Fixture
    {
        InMemoryFileSystem disk;
        FakeCatalogScanner catalog;
        FakeFilesystemProbe filesystemProbe{disk};
        FakeClock clock;
        InlineBackgroundRunner runner;
        SizeService service{catalog, filesystemProbe, clock, runner};
        MeasurementCaller caller = service.NewCaller();

        void GiveTheAddon(const std::filesystem::path& folder, const std::uintmax_t bytes)
        {
            disk.AddDirectory(folder);
            disk.AddFile(folder / "content.bin", bytes);
        }

        [[nodiscard]] SizeReport Measured(const std::vector<std::filesystem::path>& roots)
        {
            SizeReport measured;
            service.Measure(
                roots, caller, Freshness::ReuseWhatIsKnown,
                [](const SizeProgress&)
                {
                    return true;
                },
                [&measured](const SizeReport& report)
                {
                    measured = report;
                });

            return measured;
        }
    };

    class SizeServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EachAddonCarriesTheBytesOfItsOwnFolderAndTheCategoryAboveSumsThem();
        static void ACategoryCountsWhatIsBelowItAtAnyDepthAndNoAddonTwice();
        static void TwoLibrariesInTheProfileNeverSumIntoOneNumber();
        static void AFolderAlreadyMeasuredIsReadBackInsteadOfWalkedAgain();
        static void MeasuringAgainGoesBackToTheDiskEvenWhenTheNumberIsKnown();
        static void ProgressNamesTheFolderBeingMeasuredAndCountsTowardsTheTotal();
        static void CancellingStopsTheWalkAndTheReportRefusesToPassPartialsAsTotals();
        static void CancellingChangesNothingOnDiskBecauseMeasuringOnlyReads();
        static void ARequestOvertakenByAnotherEntersTheCacheAndIsNotEmitted();
        static void AFolderThatCannotBeReadIsNotTheSameAsAFolderOfZeroBytes();
        static void ALateResultFromAnOvertakenRequestNeverOverwritesFresherBytes();
        static void TwoCallersEachGetTheirOwnAnswerAndNeitherCancelsTheOther();
    };

    TreeNode OneAddonLibrary(const std::filesystem::path& addon)
    {
        return LibraryNode(kLibrary, {CategoryNode(kLibrary / "Utils", {AddonNode(addon)})});
    }
}

void SizeServiceTest::EachAddonCarriesTheBytesOfItsOwnFolderAndTheCategoryAboveSumsThem()
{
    Fixture fixture;
    fixture.GiveTheAddon(kLibrary / "Utils" / "sim-rate-selector", 300);
    fixture.GiveTheAddon(kLibrary / "Utils" / "briefing-panel", 700);

    fixture.catalog.SetTree(kLibrary,
                            LibraryNode(kLibrary,
                                        {CategoryNode(kLibrary / "Utils",
                                                      {AddonNode(kLibrary / "Utils" / "sim-rate-selector"),
                                                       AddonNode(kLibrary / "Utils" / "briefing-panel")})}));

    const SizeReport report = fixture.Measured({kLibrary});

    QCOMPARE(report.libraries.size(), std::size_t{1});

    const MeasuredNode& library = report.libraries.front();
    QCOMPARE(library.kind, TreeNodeKind::Library);
    QCOMPARE(library.bytes, std::uintmax_t{1000});

    QCOMPARE(library.children.size(), std::size_t{1});

    const MeasuredNode& category = library.children.front();
    QCOMPARE(category.kind, TreeNodeKind::Category);
    QCOMPARE(category.path, kLibrary / "Utils");
    QCOMPARE(category.bytes, std::uintmax_t{1000});

    QCOMPARE(category.children.size(), std::size_t{2});
    QCOMPARE(category.children[0].kind, TreeNodeKind::Addon);
    QCOMPARE(category.children[0].bytes, std::uintmax_t{300});
    QCOMPARE(category.children[1].bytes, std::uintmax_t{700});
}

void SizeServiceTest::ACategoryCountsWhatIsBelowItAtAnyDepthAndNoAddonTwice()
{
    Fixture fixture;
    const std::filesystem::path sceneries = kLibrary / "Sceneries";
    const std::filesystem::path europe = sceneries / "Europe";
    const std::filesystem::path germany = europe / "Germany";

    fixture.GiveTheAddon(sceneries / "airport-sbgr", 100);
    fixture.GiveTheAddon(europe / "airport-eddf", 20);
    fixture.GiveTheAddon(germany / "airport-edds", 3);

    fixture.catalog.SetTree(
        kLibrary,
        LibraryNode(kLibrary,
                    {CategoryNode(sceneries,
                                  {AddonNode(sceneries / "airport-sbgr"),
                                   CategoryNode(europe,
                                                {AddonNode(europe / "airport-eddf"),
                                                 CategoryNode(germany, {AddonNode(germany / "airport-edds")})})

                                  })}));

    const SizeReport report = fixture.Measured({kLibrary});

    const MeasuredNode& library = report.libraries.front();
    QCOMPARE(library.bytes, std::uintmax_t{123});

    const MeasuredNode& measuredSceneries = library.children.front();
    QCOMPARE(measuredSceneries.path, sceneries);
    QCOMPARE(measuredSceneries.bytes, std::uintmax_t{123});

    const MeasuredNode& measuredEurope = measuredSceneries.children[1];
    QCOMPARE(measuredEurope.path, europe);
    QCOMPARE(measuredEurope.bytes, std::uintmax_t{23});

    const MeasuredNode& measuredGermany = measuredEurope.children[1];
    QCOMPARE(measuredGermany.path, germany);
    QCOMPARE(measuredGermany.bytes, std::uintmax_t{3});
}

void SizeServiceTest::TwoLibrariesInTheProfileNeverSumIntoOneNumber()
{
    Fixture fixture;
    const std::filesystem::path other = "E:/Addons";

    fixture.GiveTheAddon(kLibrary / "Utils" / "sim-rate-selector", 300);
    fixture.GiveTheAddon(other / "Utils" / "briefing-panel", 700);

    fixture.catalog.SetTree(
        kLibrary,
        LibraryNode(kLibrary,
                    {CategoryNode(kLibrary / "Utils", {AddonNode(kLibrary / "Utils" / "sim-rate-selector")})}));
    fixture.catalog.SetTree(
        other, LibraryNode(other, {CategoryNode(other / "Utils", {AddonNode(other / "Utils" / "briefing-panel")})}));

    const SizeReport report = fixture.Measured({kLibrary, other});

    QCOMPARE(report.libraries.size(), std::size_t{2});
    QCOMPARE(report.libraries[0].path, kLibrary);
    QCOMPARE(report.libraries[0].bytes, std::uintmax_t{300});
    QCOMPARE(report.libraries[1].path, other);
    QCOMPARE(report.libraries[1].bytes, std::uintmax_t{700});
}

void SizeServiceTest::AFolderAlreadyMeasuredIsReadBackInsteadOfWalkedAgain()
{
    Fixture fixture;
    const std::filesystem::path addon = kLibrary / "Utils" / "sim-rate-selector";

    fixture.GiveTheAddon(addon, 4096);
    fixture.catalog.SetTree(kLibrary, OneAddonLibrary(addon));

    const SizeReport first = fixture.Measured({kLibrary});
    QCOMPARE(first.libraries.front().bytes, std::uintmax_t{4096});
    QCOMPARE(fixture.filesystemProbe.TimesWalked(addon), std::size_t{1});

    const SizeReport again = fixture.Measured({kLibrary});

    QCOMPARE(again.libraries.front().bytes, std::uintmax_t{4096});
    QCOMPARE(fixture.filesystemProbe.TimesWalked(addon), std::size_t{1});

    QCOMPARE(fixture.service.BytesOf(addon), std::optional<std::uintmax_t>{4096});
    QCOMPARE(fixture.service.BytesOf(kLibrary / "Utils" / "never-measured"), std::optional<std::uintmax_t>{});
}

void SizeServiceTest::MeasuringAgainGoesBackToTheDiskEvenWhenTheNumberIsKnown()
{
    Fixture fixture;
    const std::filesystem::path addon = kLibrary / "Utils" / "sim-rate-selector";

    fixture.GiveTheAddon(addon, 4096);
    fixture.catalog.SetTree(kLibrary, OneAddonLibrary(addon));

    const SizeReport first = fixture.Measured({kLibrary});
    QCOMPARE(first.libraries.front().bytes, std::uintmax_t{4096});

    fixture.disk.AddFile(addon / "extra.bin", 904);

    SizeReport remeasured;
    fixture.service.Measure(
        {kLibrary}, fixture.caller, Freshness::MeasureAgain,
        [](const SizeProgress&)
        {
            return true;
        },
        [&remeasured](const SizeReport& report)
        {
            remeasured = report;
        });

    QCOMPARE(fixture.filesystemProbe.TimesWalked(addon), std::size_t{2});
    QCOMPARE(remeasured.libraries.front().bytes, std::uintmax_t{5000});
    QCOMPARE(fixture.service.BytesOf(addon), std::optional<std::uintmax_t>{5000});
}

void SizeServiceTest::ProgressNamesTheFolderBeingMeasuredAndCountsTowardsTheTotal()
{
    Fixture fixture;
    fixture.GiveTheAddon(kLibrary / "Utils" / "one", 10);
    fixture.GiveTheAddon(kLibrary / "Utils" / "two", 20);

    fixture.catalog.SetTree(
        kLibrary,
        LibraryNode(kLibrary,
                    {CategoryNode(kLibrary / "Utils",
                                  {AddonNode(kLibrary / "Utils" / "one"), AddonNode(kLibrary / "Utils" / "two")})}));

    std::vector<SizeProgress> seen;
    fixture.service.Measure({kLibrary}, fixture.caller, Freshness::ReuseWhatIsKnown,
                            [&seen](const SizeProgress& progress)
                            {
                                seen.push_back(progress);

                                return true;
                            },
                            {});

    QCOMPARE(seen.size(), std::size_t{2});
    QCOMPARE(seen[0].folder, kLibrary / "Utils" / "one");
    QCOMPARE(seen[0].measured, std::size_t{0});
    QCOMPARE(seen[0].total, std::size_t{2});
    QCOMPARE(seen[1].folder, kLibrary / "Utils" / "two");
    QCOMPARE(seen[1].measured, std::size_t{1});
    QCOMPARE(seen[1].total, std::size_t{2});
}

void SizeServiceTest::CancellingStopsTheWalkAndTheReportRefusesToPassPartialsAsTotals()
{
    Fixture fixture;
    fixture.GiveTheAddon(kLibrary / "Utils" / "one", 10);
    fixture.GiveTheAddon(kLibrary / "Utils" / "two", 20);
    fixture.GiveTheAddon(kLibrary / "Utils" / "three", 40);

    fixture.catalog.SetTree(
        kLibrary,
        LibraryNode(kLibrary,
                    {CategoryNode(kLibrary / "Utils",
                                  {AddonNode(kLibrary / "Utils" / "one"), AddonNode(kLibrary / "Utils" / "two"),
                                   AddonNode(kLibrary / "Utils" / "three")})}));

    SizeReport cancelled;
    fixture.service.Measure(
        {kLibrary}, fixture.caller, Freshness::ReuseWhatIsKnown,
        [](const SizeProgress& progress)
        {
            return progress.measured < 1;
        },
        [&cancelled](const SizeReport& report)
        {
            cancelled = report;
        });

    QVERIFY(!cancelled.complete);
    QCOMPARE(fixture.filesystemProbe.TimesWalked(kLibrary / "Utils" / "one"), std::size_t{1});
    QCOMPARE(fixture.filesystemProbe.TimesWalked(kLibrary / "Utils" / "two"), std::size_t{0});
    QCOMPARE(fixture.filesystemProbe.TimesWalked(kLibrary / "Utils" / "three"), std::size_t{0});

    const MeasuredNode& library = cancelled.libraries.front();
    QVERIFY(!library.measured);

    const MeasuredNode& category = library.children.front();
    QVERIFY(!category.measured);
    QVERIFY(category.children[0].measured);
    QVERIFY(!category.children[1].measured);
    QVERIFY(!category.children[2].measured);

    QCOMPARE(category.children[0].bytes, std::uintmax_t{10});
    QCOMPARE(fixture.service.BytesOf(kLibrary / "Utils" / "one"), std::optional<std::uintmax_t>{10});
    QCOMPARE(fixture.service.BytesOf(kLibrary / "Utils" / "two"), std::optional<std::uintmax_t>{});
}

void SizeServiceTest::CancellingChangesNothingOnDiskBecauseMeasuringOnlyReads()
{
    Fixture fixture;
    fixture.GiveTheAddon(kLibrary / "Utils" / "one", 10);
    fixture.GiveTheAddon(kLibrary / "Utils" / "two", 20);

    fixture.catalog.SetTree(
        kLibrary,
        LibraryNode(kLibrary,
                    {CategoryNode(kLibrary / "Utils",
                                  {AddonNode(kLibrary / "Utils" / "one"), AddonNode(kLibrary / "Utils" / "two")})}));

    const std::vector<std::filesystem::path> before = fixture.disk.FilesUnder(kLibrary);

    fixture.service.Measure({kLibrary}, fixture.caller, Freshness::ReuseWhatIsKnown,
                            [](const SizeProgress&)
                            {
                                return false;
                            },
                            {});

    QCOMPARE(fixture.disk.FilesUnder(kLibrary), before);
    QCOMPARE(fixture.disk.FileSize(kLibrary / "Utils" / "one" / "content.bin"), std::uintmax_t{10});
    QCOMPARE(fixture.disk.FileSize(kLibrary / "Utils" / "two" / "content.bin"), std::uintmax_t{20});
}

void SizeServiceTest::ARequestOvertakenByAnotherEntersTheCacheAndIsNotEmitted()
{
    Fixture fixture;
    const std::filesystem::path addon = kLibrary / "Utils" / "sim-rate-selector";

    fixture.GiveTheAddon(addon, 4096);
    fixture.catalog.SetTree(kLibrary, OneAddonLibrary(addon));
    fixture.runner.defer = true;

    int emitted = 0;
    const auto count = [&emitted](const SizeReport&)
    {
        ++emitted;
    };
    const auto keepGoing = [](const SizeProgress&)
    {
        return true;
    };

    fixture.service.Measure({kLibrary}, fixture.caller, Freshness::ReuseWhatIsKnown, keepGoing, count);
    fixture.service.Measure({kLibrary}, fixture.caller, Freshness::ReuseWhatIsKnown, keepGoing, count);

    QCOMPARE(fixture.runner.HowManyPending(), std::size_t{2});

    fixture.runner.Finish();

    QCOMPARE(emitted, 0);
    QCOMPARE(fixture.service.BytesOf(addon), std::optional<std::uintmax_t>{4096});

    fixture.runner.Finish();

    QCOMPARE(emitted, 1);
}

void SizeServiceTest::AFolderThatCannotBeReadIsNotTheSameAsAFolderOfZeroBytes()
{
    Fixture fixture;
    const std::filesystem::path unreadable = kLibrary / "Utils" / "locked-addon";
    const std::filesystem::path empty = kLibrary / "Utils" / "empty-addon";

    fixture.GiveTheAddon(unreadable, 5000);
    fixture.disk.AddDirectory(empty);
    fixture.filesystemProbe.RefuseToWalk(unreadable);

    fixture.catalog.SetTree(
        kLibrary, LibraryNode(kLibrary, {CategoryNode(kLibrary / "Utils", {AddonNode(unreadable), AddonNode(empty)})}));

    const SizeReport report = fixture.Measured({kLibrary});

    const MeasuredNode& category = report.libraries.front().children.front();

    QVERIFY(!category.children[0].measured);
    QCOMPARE(category.children[0].bytes, std::uintmax_t{0});

    QVERIFY(category.children[1].measured);
    QCOMPARE(category.children[1].bytes, std::uintmax_t{0});

    QVERIFY(!category.measured);
    QVERIFY(!report.libraries.front().measured);
    QCOMPARE(fixture.service.BytesOf(unreadable), std::optional<std::uintmax_t>{});
    QCOMPARE(fixture.service.BytesOf(empty), std::optional<std::uintmax_t>{0});
}

void SizeServiceTest::ALateResultFromAnOvertakenRequestNeverOverwritesFresherBytes()
{
    Fixture fixture;
    const std::filesystem::path addon = kLibrary / "Utils" / "sim-rate-selector";

    fixture.GiveTheAddon(addon, 4096);
    fixture.catalog.SetTree(kLibrary, OneAddonLibrary(addon));
    fixture.runner.defer = true;

    const auto keepGoing = [](const SizeProgress&)
    {
        return true;
    };

    fixture.service.Measure({kLibrary}, fixture.caller, Freshness::MeasureAgain, keepGoing, {});
    fixture.runner.RunPendingWork();

    fixture.disk.AddFile(addon / "extra.bin", 904);

    fixture.service.Measure({kLibrary}, fixture.caller, Freshness::MeasureAgain, keepGoing, {});
    fixture.runner.RunPendingWork();

    fixture.runner.FinishNewestDone();
    QCOMPARE(fixture.service.BytesOf(addon), std::optional<std::uintmax_t>{5000});

    fixture.runner.FinishNewestDone();
    QCOMPARE(fixture.service.BytesOf(addon), std::optional<std::uintmax_t>{5000});
}

void SizeServiceTest::TwoCallersEachGetTheirOwnAnswerAndNeitherCancelsTheOther()
{
    Fixture fixture;
    const std::filesystem::path addon = kLibrary / "Utils" / "sim-rate-selector";

    fixture.GiveTheAddon(addon, 4096);
    fixture.catalog.SetTree(kLibrary, OneAddonLibrary(addon));
    fixture.runner.defer = true;

    const MeasurementCaller diagnostics = fixture.service.NewCaller();
    const MeasurementCaller quarantine = fixture.service.NewCaller();

    int answeredDiagnostics = 0;
    int answeredQuarantine = 0;
    const auto keepGoing = [](const SizeProgress&)
    {
        return true;
    };

    fixture.service.Measure({kLibrary}, diagnostics, Freshness::ReuseWhatIsKnown, keepGoing,
                            [&answeredDiagnostics](const SizeReport&)
                            {
                                ++answeredDiagnostics;
                            });
    fixture.service.Measure({kLibrary}, quarantine, Freshness::ReuseWhatIsKnown, keepGoing,
                            [&answeredQuarantine](const SizeReport&)
                            {
                                ++answeredQuarantine;
                            });

    fixture.runner.Finish();
    fixture.runner.Finish();

    QCOMPARE(answeredDiagnostics, 1);
    QCOMPARE(answeredQuarantine, 1);
}

QTEST_APPLESS_MAIN(SizeServiceTest)

#include "tst_size_service.moc"
