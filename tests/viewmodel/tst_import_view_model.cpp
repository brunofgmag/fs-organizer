#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/SessionNotifier.h"

class ImportViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void MeasuringAddsUpEveryFolderItWasGiven();
    static void AFolderAlreadyMeasuredAnswersWithoutTouchingTheDiskAgain();
    static void OnlyTheLastMeasurementAskedForIsReported();
    static void ForgettingSendsTheNextMeasurementBackToTheDisk();
    static void CancellingDuringTheCopyStopsTheRemainingFoldersAndSaysSo();
    static void AnImportGoesThroughTheRunnerTheViewModelWasGiven();
};

namespace
{
    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kSmall = "E:/Sim/Community/small-addon";
    const std::filesystem::path kBig = "E:/Sim/Community/big-addon";

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kSmall);
            fileSystem.AddDirectory(kBig);
            fileSystem.AddFile(kSmall / "manifest.json", 300);
            fileSystem.AddFile(kBig / "manifest.json", 700);
            fileSystem.AddFile(kBig / "scenery" / "big.bgl", 4000);

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = Profile().id;
            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        ImportEngine engine{filesystemProbe, files, linking, log, LinkType::Junction};
        ImportService service{engine, processProbe, filesystemProbe, catalog, files, linking, log, LinkType::Junction};
        ProfileService profiles{catalog, classifier, linking, log, identities, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{profiles, organizer, settings, processProbe, runner, notifier};
        ImportViewModel viewModel{service, profiles, processProbe, session, runner};
    };
}

void ImportViewModelTest::CancellingDuringTheCopyStopsTheRemainingFoldersAndSaysSo()
{
    Fixture f;

    QObject::connect(&f.viewModel, &ImportViewModel::Progressed, &f.viewModel,
                     [&f]
                     {
                         f.viewModel.Cancel();
                     });

    const QSignalSpy finished(&f.viewModel, &ImportViewModel::Finished);

    f.viewModel.Import({ImportRequest{kSmall, kLibrary}, ImportRequest{kBig, kLibrary}});

    QCOMPARE(finished.size(), 1);

    const auto results = finished.front().front().value<std::vector<ImportOperationResult>>();
    QCOMPARE(results.size(), std::size_t{2});
    QVERIFY(!Succeeded(results.front().result));
    QCOMPARE(results.back().result, FileResult::Cancelled);
    QVERIFY(f.fileSystem.Exists(kSmall));
    QVERIFY(f.fileSystem.Exists(kBig));
}

void ImportViewModelTest::AnImportGoesThroughTheRunnerTheViewModelWasGiven()
{
    Fixture f;
    const QSignalSpy finished(&f.viewModel, &ImportViewModel::Finished);

    const int beforeTheImport = f.runner.runs;

    f.viewModel.Import({ImportRequest{kSmall, kLibrary}});

    QCOMPARE(f.runner.runs, beforeTheImport + 1);
    QCOMPARE(finished.size(), 1);
}

void ImportViewModelTest::MeasuringAddsUpEveryFolderItWasGiven()
{
    Fixture f;
    const QSignalSpy measured(&f.viewModel, &ImportViewModel::SizeMeasured);
    const QSignalSpy measuring(&f.viewModel, &ImportViewModel::SizeMeasuring);

    f.viewModel.MeasureTotalSize({kSmall, kBig});

    QCOMPARE(measuring.count(), 1);
    QCOMPARE(measured.count(), 1);
    QCOMPARE(measured.front().front().toULongLong(), 5000ULL);
}

void ImportViewModelTest::AFolderAlreadyMeasuredAnswersWithoutTouchingTheDiskAgain()
{
    Fixture f;
    f.viewModel.MeasureTotalSize({kBig});

    const QSignalSpy measured(&f.viewModel, &ImportViewModel::SizeMeasured);
    const QSignalSpy measuring(&f.viewModel, &ImportViewModel::SizeMeasuring);

    f.viewModel.MeasureTotalSize({kBig});

    QCOMPARE(measured.count(), 1);
    QCOMPARE(measured.front().front().toULongLong(), 4700ULL);
    QCOMPARE(measuring.count(), 0);
}

void ImportViewModelTest::OnlyTheLastMeasurementAskedForIsReported()
{
    Fixture f;
    const QSignalSpy measured(&f.viewModel, &ImportViewModel::SizeMeasured);

    f.viewModel.MeasureTotalSize({kSmall});
    f.viewModel.MeasureTotalSize({kBig});

    QCOMPARE(measured.count(), 2);
    QCOMPARE(measured.back().front().toULongLong(), 4700ULL);
}

void ImportViewModelTest::ForgettingSendsTheNextMeasurementBackToTheDisk()
{
    Fixture f;
    f.viewModel.MeasureTotalSize({kSmall});

    f.fileSystem.AddFile(kSmall / "scenery" / "added.bgl", 1000);
    f.viewModel.ForgetMeasuredSizes();

    const QSignalSpy measured(&f.viewModel, &ImportViewModel::SizeMeasured);
    const QSignalSpy measuring(&f.viewModel, &ImportViewModel::SizeMeasuring);

    f.viewModel.MeasureTotalSize({kSmall});

    QCOMPARE(measuring.count(), 1);
    QCOMPARE(measured.count(), 1);
    QCOMPARE(measured.front().front().toULongLong(), 1300ULL);
}

QTEST_MAIN(ImportViewModelTest)

#include "tst_import_view_model.moc"
