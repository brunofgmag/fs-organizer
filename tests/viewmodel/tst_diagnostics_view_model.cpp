#include <algorithm>
#include <numeric>

#include <QtTest/QtTest>

#include "application/ImportService.h"
#include "application/LibraryOrganizer.h"
#include "application/SizeService.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/DiagnosticsViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class DiagnosticsViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheCountsCoverEveryClassificationAndSumToWhatTheDestinationsScreenLists();
        static void AClassificationWithNoEntryIsShownWithZeroInsteadOfDisappearing();
        static void BrokenAndUnavailableAreCarriedApartFromEachOther();
        static void TheQuarantineWeightSumsBothPlacesAndSaysWhichOnesExist();
        static void EachSectionCarriesTheMomentItWasMeasured();
        static void NoTreeIsWalkedUntilTheSizeSectionIsOpened();
        static void ComingBackToTheSizeSectionShowsWhatWasMeasuredWithoutWalkingAgain();
        static void ProgressNamesTheFolderBeingMeasured();
        static void CancellingLeavesTheNumbersMarkedIncomplete();
        static void MeasuringAgainSupersedesTheEarlierRequestAndTheLateAnswerIsNotShown();
        static void AMeasurementInFlightSurvivesTheScreenBeingLeftAndRevisited();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w")};

        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;
        node.children = {aircrafts};

        return node;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    std::size_t CountOf(const std::vector<ClassificationCount>& counts, const EntryClassification classification)
    {
        const auto found = std::ranges::find(counts, classification, &ClassificationCount::classification);

        return found == counts.end() ? 0 : found->count;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
            catalog.SetTree(kLibrary, LibraryTree());
        }

        void Seed(const SimulatorProfile& profile)
        {
            settings.stored.profiles = {profile};
            settings.stored.activeProfileId = profile.id;

            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, filesystemProbe, sidecars,          classifier, linking,
                               log,     identities,      LinkType::Junction};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        ImportEngine importEngine{filesystemProbe, files, sidecars, linking, log, LinkType::Junction};
        ImportService imports{importEngine, processProbe, filesystemProbe,   catalog, files, sidecars,
                              linking,      log,          LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        DiagnosticsViewModel viewModel{imports, sizes, session, clock};
    };
}

void DiagnosticsViewModelTest::TheCountsCoverEveryClassificationAndSumToWhatTheDestinationsScreenLists()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/pmdg-aircraft-77w",
                         "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone");
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/physical");
    f.Seed(Profile());

    f.viewModel.Show();

    const std::vector<ClassificationCount> counts = f.viewModel.Counts();
    const std::size_t total = std::accumulate(counts.begin(), counts.end(), std::size_t{0},
                                              [](const std::size_t sum, const ClassificationCount& row)
                                              {
                                                  return sum + row.count;
                                              });

    QCOMPARE(total, f.session.Snapshot().entries.size());
    QCOMPARE(total, std::size_t{3});
    QCOMPARE(CountOf(counts, EntryClassification::Managed), std::size_t{1});
    QCOMPARE(CountOf(counts, EntryClassification::Broken), std::size_t{1});
    QCOMPARE(CountOf(counts, EntryClassification::Unmanaged), std::size_t{1});
}

void DiagnosticsViewModelTest::AClassificationWithNoEntryIsShownWithZeroInsteadOfDisappearing()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/physical");
    f.Seed(Profile());

    f.viewModel.Show();

    const std::vector<ClassificationCount> counts = f.viewModel.Counts();

    QCOMPARE(counts.size(), kEveryClassification.size());
    QCOMPARE(CountOf(counts, EntryClassification::Unmanaged), std::size_t{1});
    QCOMPARE(CountOf(counts, EntryClassification::Duplicated), std::size_t{0});
    QCOMPARE(CountOf(counts, EntryClassification::Unavailable), std::size_t{0});
}

void DiagnosticsViewModelTest::BrokenAndUnavailableAreCarriedApartFromEachOther()
{
    Fixture f;
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone");
    f.fileSystem.AddLink("E:/Flight Simulator 2024/Community/on-a-detached-volume", "Z:/Library/parked");
    f.fileSystem.MarkVolumeUnavailable("Z:/");
    f.Seed(Profile());

    f.viewModel.Show();

    QCOMPARE(f.viewModel.Broken().size(), std::size_t{1});
    QCOMPARE(f.viewModel.Broken().front().path, std::filesystem::path("E:/Flight Simulator 2024/Community/gone"));
    QCOMPARE(f.viewModel.Unavailable().size(), std::size_t{1});
    QCOMPARE(f.viewModel.Unavailable().front().path,
             std::filesystem::path("E:/Flight Simulator 2024/Community/on-a-detached-volume"));
}

void DiagnosticsViewModelTest::TheQuarantineWeightSumsBothPlacesAndSaysWhichOnesExist()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/_fsorganizer-quarantine/pmdg-737");
    f.fileSystem.AddFile("E:/Flight Simulator 2024/_fsorganizer-quarantine/pmdg-737/texture.dds", 700);
    f.fileSystem.AddDirectory("D:/MSFS 2024/_fsorganizer-quarantine/fenix-a320");
    f.fileSystem.AddFile("D:/MSFS 2024/_fsorganizer-quarantine/fenix-a320/model.bin", 300);
    f.Seed(Profile());

    f.viewModel.Show();

    const QuarantineWeight weight = f.viewModel.Quarantine();

    QCOMPARE(weight.bytes, std::uintmax_t{1000});
    QCOMPARE(weight.besideDestinations, std::size_t{1});
    QCOMPARE(weight.insideLibraries, std::size_t{1});
}

void DiagnosticsViewModelTest::EachSectionCarriesTheMomentItWasMeasured()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/physical");
    f.Seed(Profile());

    f.viewModel.Show();
    const std::chrono::system_clock::time_point first = f.clock.now;

    QCOMPARE(f.viewModel.CountedAt(), std::optional(first));
    QVERIFY(!f.viewModel.MeasuredAt().has_value());

    f.clock.now += std::chrono::minutes{5};
    f.viewModel.Show();

    QCOMPARE(f.viewModel.CountedAt(), std::optional(first + std::chrono::minutes{5}));
    QVERIFY(!f.viewModel.MeasuredAt().has_value());
}

void DiagnosticsViewModelTest::NoTreeIsWalkedUntilTheSizeSectionIsOpened()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
    f.Seed(Profile());

    f.viewModel.Show();

    QCOMPARE(f.filesystemProbe.TimesWalked("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"), std::size_t{0});
    QVERIFY(!f.viewModel.MeasuredAt().has_value());

    f.viewModel.ShowSize();

    QCOMPARE(f.filesystemProbe.TimesWalked("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"), std::size_t{1});
    QVERIFY(f.viewModel.MeasuredAt().has_value());
    QCOMPARE(f.viewModel.Size().libraries.front().bytes, std::uintmax_t{4096});
}

void DiagnosticsViewModelTest::ComingBackToTheSizeSectionShowsWhatWasMeasuredWithoutWalkingAgain()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
    f.Seed(Profile());

    f.viewModel.Show();
    f.viewModel.ShowSize();

    const std::chrono::system_clock::time_point measured = *f.viewModel.MeasuredAt();

    f.clock.now += std::chrono::minutes{7};
    f.viewModel.Show();
    f.viewModel.ShowSize();

    QCOMPARE(f.filesystemProbe.TimesWalked("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w"), std::size_t{1});
    QCOMPARE(f.viewModel.MeasuredAt(), std::optional(measured));
    QCOMPARE(f.viewModel.CountedAt(), std::optional(measured + std::chrono::minutes{7}));
}

void DiagnosticsViewModelTest::ProgressNamesTheFolderBeingMeasured()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
    f.Seed(Profile());

    QStringList named;
    QObject::connect(&f.viewModel, &DiagnosticsViewModel::SizeProgressed, &f.viewModel,
                     [&named](const QString& folder, int, int)
                     {
                         named.append(folder);
                     });

    f.viewModel.ShowSize();

    QVERIFY(!named.isEmpty());
    QVERIFY(named.last().contains(QStringLiteral("pmdg-aircraft-77w")));
}

void DiagnosticsViewModelTest::CancellingLeavesTheNumbersMarkedIncomplete()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
    f.Seed(Profile());

    QObject::connect(&f.viewModel, &DiagnosticsViewModel::SizeProgressed, &f.viewModel,
                     [&f]
                     {
                         f.viewModel.CancelSize();
                     });

    f.viewModel.ShowSize();

    QVERIFY(!f.viewModel.Size().complete);
    QVERIFY(!f.viewModel.Measuring());
    QVERIFY(f.fileSystem.Exists("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin"));
}

void DiagnosticsViewModelTest::MeasuringAgainSupersedesTheEarlierRequestAndTheLateAnswerIsNotShown()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
    f.Seed(Profile());

    int measured = 0;
    QObject::connect(&f.viewModel, &DiagnosticsViewModel::SizeMeasured, &f.viewModel,
                     [&measured]
                     {
                         ++measured;
                     });

    f.runner.defer = true;
    f.viewModel.ShowSize();
    f.viewModel.MeasureSizeAgain();

    QCOMPARE(f.runner.HowManyPending(), std::size_t{2});

    f.runner.RunPendingWork();
    f.runner.FinishNewestDone();
    f.runner.Finish();

    QCOMPARE(measured, 1);
}

void DiagnosticsViewModelTest::AMeasurementInFlightSurvivesTheScreenBeingLeftAndRevisited()
{
    Fixture f;
    f.fileSystem.AddFile("D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w/model.bin", 4096);
    f.Seed(Profile());

    f.runner.defer = true;
    f.viewModel.ShowSize();

    QVERIFY(f.viewModel.Measuring());

    f.viewModel.Show();
    f.runner.Finish();

    QVERIFY(!f.viewModel.Measuring());
    QVERIFY(f.viewModel.Size().complete);
    QCOMPARE(f.viewModel.Size().libraries.front().bytes, std::uintmax_t{4096});
}

QTEST_APPLESS_MAIN(DiagnosticsViewModelTest)

#include "tst_diagnostics_view_model.moc"
