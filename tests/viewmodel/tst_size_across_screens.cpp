#include <QtTest/QtTest>

#include <filesystem>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeLoadingReportSource.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/AddonTreeViewModel.h"
#include "viewmodel/CommunityViewModel.h"
#include "viewmodel/DiagnosticsViewModel.h"
#include "viewmodel/QuarantineViewModel.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class SizeAcrossScreensTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheLibraryPanelAndTheDestinationsScreenAnswerTheSameNumberForTheSameAddon();
        static void TheWalkOfTheDiagnosticsScreenAnswersTheOtherTwoWithoutTouchingTheDiskAgain();
        static void TheQuarantineReadsTheSameCacheAsTheDiagnosticsScreen();
    };
}

namespace
{
    const std::filesystem::path kLibrary = "D:/MSFS 2024";
    const std::filesystem::path kAircrafts = "D:/MSFS 2024/Aircrafts";
    const std::filesystem::path kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLink = "E:/Sim/Community/pmdg-aircraft-77w";
    const std::filesystem::path kQuarantined = "E:/Sim/_fsorganizer-quarantine/simbridge";

    TreeNode LibraryTree()
    {
        TreeNode addon;
        addon.kind = TreeNodeKind::Addon;
        addon.path = kAddon;
        addon.addon = Addon{.folderPath = kAddon, .manifest = Manifest{}};

        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = kAircrafts;
        aircrafts.children = {addon};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {aircrafts};

        return library;
    }

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
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddFile(kAddon / "content.bin", 4096);
            fileSystem.AddLink(kLink, kAddon);
            fileSystem.AddDirectory(kQuarantined);
            fileSystem.AddFile(kQuarantined / "content.bin", 900);
            catalog.SetTree(kLibrary, LibraryTree());

            session.ShowActiveProfile();
        }

        [[nodiscard]] DestinationEntry ManagedEntry() const
        {
            for (const DestinationEntry& entry : session.Snapshot().entries)
            {
                if (entry.classification == EntryClassification::Managed)
                {
                    return entry;
                }
            }

            return {};
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeSidecarStore sidecars{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Junction,
                            Verification::ByStructure};
        ImportService imports{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                              linking, log,          LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};

        AddonTreeModel treeModel;
        FakeSimulatorPackages packages;
        AddonTreeViewModel tree{session, profiles, treeModel, packages, sizes, notifier};

        CommunityModel communityModel;
        CommunityViewModel community{profiles, session, notifier, communityModel, sizes};

        QuarantineModel quarantineModel;
        QuarantineViewModel quarantine{imports, profiles, session, notifier, quarantineModel, sizes, runner};

        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService scenery{filesystemProbe, sceneryParser, clock, sceneryCache};
        FakeLoadingReportSource loading;
        DiagnosticsViewModel diagnostics{imports, sizes, scenery, session, loading, clock, runner};
    };

    SelectionSize LastSize(const QSignalSpy& measured)
    {
        return measured.isEmpty() ? SelectionSize{} : measured.back().front().value<SelectionSize>();
    }
}

void SizeAcrossScreensTest::TheLibraryPanelAndTheDestinationsScreenAnswerTheSameNumberForTheSameAddon()
{
    Fixture f;

    const QSignalSpy fromTheLibrary(&f.tree, &AddonTreeViewModel::SizeMeasured);
    const QSignalSpy fromTheDestinations(&f.community, &CommunityViewModel::SizeMeasured);

    f.tree.MeasureTheSelection({kAddon});
    f.community.MeasureTheSelection({f.ManagedEntry()});

    QCOMPARE(LastSize(fromTheLibrary).bytes, std::uintmax_t{4096});
    QCOMPARE(LastSize(fromTheDestinations).bytes, LastSize(fromTheLibrary).bytes);
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddon), std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesWalked(kLink), std::size_t{0});
}

void SizeAcrossScreensTest::TheWalkOfTheDiagnosticsScreenAnswersTheOtherTwoWithoutTouchingTheDiskAgain()
{
    Fixture f;

    f.diagnostics.ShowSize();
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddon), std::size_t{1});

    const QSignalSpy fromTheLibrary(&f.tree, &AddonTreeViewModel::SizeMeasured);
    const QSignalSpy fromTheDestinations(&f.community, &CommunityViewModel::SizeMeasured);

    f.tree.MeasureTheSelection({kAddon});
    f.community.MeasureTheSelection({f.ManagedEntry()});

    QCOMPARE(LastSize(fromTheLibrary).bytes, std::uintmax_t{4096});
    QCOMPARE(LastSize(fromTheDestinations).bytes, std::uintmax_t{4096});
    QCOMPARE(f.filesystemProbe.TimesWalked(kAddon), std::size_t{1});
}

void SizeAcrossScreensTest::TheQuarantineReadsTheSameCacheAsTheDiagnosticsScreen()
{
    Fixture f;

    f.quarantine.Show();
    QCOMPARE(f.filesystemProbe.TimesWalked(kQuarantined), std::size_t{1});

    const QString shown =
        f.quarantineModel.data(f.quarantineModel.index(0, QuarantineModel::NameColumn, {}), SecondLineRole).toString();
    QVERIFY(!shown.isEmpty());

    f.quarantine.Show();

    QCOMPARE(f.filesystemProbe.TimesWalked(kQuarantined), std::size_t{1});
    QCOMPARE(
        f.quarantineModel.data(f.quarantineModel.index(0, QuarantineModel::NameColumn, {}), SecondLineRole).toString(),
        shown);
}

QTEST_MAIN(SizeAcrossScreensTest)

#include "tst_size_across_screens.moc"
