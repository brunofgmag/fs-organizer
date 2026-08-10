#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/profile/ExternalOrigins.h"
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
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class ImportViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void CancellingDuringTheCopyStopsTheRemainingFoldersAndSaysSo();
        static void AnImportGoesThroughTheRunnerTheViewModelWasGiven();
        static void GivingAnAddonBackForgetsWhereItCameFromInsteadOfRememberingIt();
        static void EveryLongOperationOpensAndClosesTheSameProgress();
    };
}

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
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

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
        FakeSidecarStore sidecars{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeProcessProbe processProbe;
        FakeLibraryIdGenerator identities;
        EntryClassifier classifier{linkService, filesystemProbe};
        LinkingEngine linking{linkService, filesystemProbe};
        ImportEngine engine{filesystemProbe, files, sidecars, linking, log, LinkType::Junction};
        ImportService service{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                              linking, log,          LinkType::Junction};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
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

    f.viewModel.Import(
        {ImportRequest{.source = kSmall, .category = kLibrary}, ImportRequest{.source = kBig, .category = kLibrary}});

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

    f.viewModel.Import({ImportRequest{.source = kSmall, .category = kLibrary}});

    QCOMPARE(f.runner.runs, beforeTheImport + 1);
    QCOMPARE(finished.size(), 1);
}

namespace
{
    const std::filesystem::path kVendorFolder = "C:/Addon Manager/gsx-pro";
    const std::filesystem::path kVendorInLibrary = "D:/Library/gsx-pro";

    void AManagedExternal(Fixture& f)
    {
        f.fileSystem.AddDirectory(kVendorFolder.parent_path());
        f.fileSystem.AddDirectory(kVendorInLibrary);
        f.fileSystem.AddFile(kVendorInLibrary / "manifest.json", 900);
        f.fileSystem.AddLink(kVendorFolder, kVendorInLibrary);
        f.fileSystem.AddLink(kDestination / "gsx-pro", kVendorInLibrary);

        SimulatorProfile stored = Profile();
        RememberWhereItCameFrom(stored, kVendorInLibrary, kVendorFolder);

        f.settings.stored.profiles = {stored};
        f.session.ShowActiveProfile();
    }
}

void ImportViewModelTest::GivingAnAddonBackForgetsWhereItCameFromInsteadOfRememberingIt()
{
    Fixture f;
    AManagedExternal(f);

    QCOMPARE(ExternalAddonsOf(f.session.Profile()).size(), std::size_t{1});

    const QSignalSpy gaveBack(&f.viewModel, &ImportViewModel::GaveBack);

    f.viewModel.GiveBack({kVendorInLibrary});

    QCOMPARE(gaveBack.size(), 1);

    const auto results = gaveBack.front().front().value<std::vector<FileOperationResult>>();
    QCOMPARE(results.size(), std::size_t{1});
    QCOMPARE(results.front().result, FileResult::Completed);
    QVERIFY2(ExternalAddonsOf(f.session.Profile()).empty(),
             "an adoption that remembers would write back the origin the give back just erased");
    QVERIFY(f.fileSystem.IsDirectory(kVendorFolder));
    QVERIFY(!f.fileSystem.Exists(kVendorInLibrary));
}

void ImportViewModelTest::EveryLongOperationOpensAndClosesTheSameProgress()
{
    Fixture f;
    AManagedExternal(f);

    const QSignalSpy started(&f.viewModel, &ImportViewModel::Started);
    const QSignalSpy idle(&f.viewModel, &ImportViewModel::Idle);

    f.viewModel.Import({ImportRequest{.source = kSmall, .category = kLibrary}});
    f.viewModel.GiveBack({kVendorInLibrary});
    f.viewModel.ResolveConflicts(
        {ConflictToResolve{.conflict = CopyConflict{.provenancePath = kBig, .libraryPath = kLibrary / "big-addon"},
                           .choice = ConflictChoice::KeepTheProvenanceCopy}});

    QCOMPARE(started.size(), 3);
    QCOMPARE(idle.size(), 3);
}

QTEST_MAIN(ImportViewModelTest)

#include "tst_import_view_model.moc"
