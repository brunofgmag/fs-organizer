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
#include "viewmodel/QuarantineViewModel.h"

class QuarantineViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheQuarantineListsWhatBelongsToTheProfileTheSessionIsShowing();
    static void TheQuarantineCatchesUpWhenTheActiveProfileFinallyLands();
};

namespace
{
    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kQuarantined = "E:/Sim/_fsorganizer-quarantine/simbridge";

    TreeNode LibraryTree()
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;

        return node;
    }

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
            fileSystem.AddDirectory(kQuarantined);
            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = Profile().id;
        }

        void ScanLands()
        {
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
        Session session{profiles, organizer, settings, runner, notifier};
        QuarantineModel model;
        QuarantineViewModel viewModel{service, profiles, session, notifier, model};
    };
}

void QuarantineViewModelTest::TheQuarantineListsWhatBelongsToTheProfileTheSessionIsShowing()
{
    Fixture f;
    f.ScanLands();

    f.viewModel.Show();

    QCOMPARE(f.model.rowCount({}), 1);
    QCOMPARE(f.model.ItemAt(f.model.index(0, QuarantineModel::NameColumn, {}))->path, kQuarantined);
}

void QuarantineViewModelTest::TheQuarantineCatchesUpWhenTheActiveProfileFinallyLands()
{
    Fixture f;

    f.viewModel.Show();
    QCOMPARE(f.model.rowCount({}), 0);

    f.ScanLands();

    QCOMPARE(f.model.rowCount({}), 1);
}

QTEST_MAIN(QuarantineViewModelTest)

#include "tst_quarantine_view_model.moc"
