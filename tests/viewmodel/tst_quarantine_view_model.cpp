#include <QtTest/QtTest>

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
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/QuarantineViewModel.h"

class QuarantineViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void TheQuarantineListsWhatBelongsToTheProfileTheTreeIsShowing();
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
        }

        void ScanLands()
        {
            treeModel.ShowSnapshot(profiles.Scan(Profile()), Profile());
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
        AddonTreeModel treeModel;
        QuarantineModel model;
        QuarantineViewModel viewModel{service, profiles, treeModel, model};
    };
}

void QuarantineViewModelTest::TheQuarantineListsWhatBelongsToTheProfileTheTreeIsShowing()
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

    QEXPECT_FAIL("",
                 "The quarantine reads the profile from AddonTreeModel, which only learns it when the scan lands, "
                 "and nothing re-runs Show afterwards: ScanFinished reaches CommunityViewModel alone.",
                 Abort);
    QCOMPARE(f.model.rowCount({}), 1);
}

QTEST_MAIN(QuarantineViewModelTest)

#include "tst_quarantine_view_model.moc"
