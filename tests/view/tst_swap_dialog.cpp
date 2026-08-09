#include <QtTest/QtTest>

#include <QtWidgets/QLabel>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "support/SizeText.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/PathPrinting.h"
#include "view/library/SwapDialog.h"

namespace
{
    class SwapDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BothSidesOpenWithTheirNameAndVersionAndNoSizeYet();
        static void TheMeasurementLandsAfterTheDialogIsBuiltAndFillsBothSides();
        static void ASideNobodyCouldMeasureKeepsItsNameInsteadOfShowingZero();
        static void TheSizeThatArrivesIsTheOneTheServiceMeasuredOnDisk();
    };

    constexpr std::uintmax_t kMegabyte = 1024 * 1024;

    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kAircrafts = "D:/Library/Aircrafts";
    const std::filesystem::path kCrj = "D:/Library/Aircrafts/aerosoft-crj";
    const std::filesystem::path kAtr = "D:/Library/Aircrafts/hype-atr";

    constexpr std::uintmax_t kCrjBytes = 9 * kMegabyte;
    constexpr std::uintmax_t kAtrBytes = 21 * kMegabyte;

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
        aircrafts.path = kAircrafts;
        aircrafts.children = {AddonNode(kCrj), AddonNode(kAtr)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

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
            fileSystem.AddDirectory(kCrj);
            fileSystem.AddDirectory(kAtr);
            fileSystem.AddFile(kCrj / "manifest.json", kCrjBytes);
            fileSystem.AddFile(kAtr / "manifest.json", kAtrBytes);

            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = Profile().id;

            session.ShowActiveProfile();
        }

        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        ProfileService service{catalog, filesystemProbe, classifier, linking, log, identities, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        AddonTreeModel model;
        FakeSimulatorPackages packages;
        AddonTreeViewModel viewModel{session, service, model, packages, sizes, notifier};
    };

    std::vector<TakenPlace> OneSwap()
    {
        return {TakenPlace{.addonFolder = kAtr, .linkPath = kDestination / "hype-atr", .occupant = kCrj}};
    }

    QString EverythingWritten(const SwapDialog& dialog)
    {
        QStringList said;

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said.join(QStringLiteral("\n"));
    }
}

void SwapDialogTest::BothSidesOpenWithTheirNameAndVersionAndNoSizeYet()
{
    Fixture fixture;
    const SwapDialog dialog(OneSwap(), fixture.viewModel);

    const QString said = EverythingWritten(dialog);

    QVERIFY(said.contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(said.contains(QStringLiteral("hype-atr")));
    QVERIFY(!said.contains(AsSize(kCrjBytes)));
    QVERIFY(!said.contains(AsSize(0)));
}

void SwapDialogTest::TheMeasurementLandsAfterTheDialogIsBuiltAndFillsBothSides()
{
    Fixture fixture;
    SwapDialog dialog(OneSwap(), fixture.viewModel);

    fixture.viewModel.WeighTheSwaps(OneSwap(),
                                    [&dialog](const std::vector<WeighedSwap>& weighed)
                                    {
                                        dialog.ShowTheSizes(weighed);
                                    });

    const QString said = EverythingWritten(dialog);

    QVERIFY(said.contains(AsSize(kCrjBytes)));
    QVERIFY(said.contains(AsSize(kAtrBytes)));
}

void SwapDialogTest::ASideNobodyCouldMeasureKeepsItsNameInsteadOfShowingZero()
{
    Fixture fixture;
    SwapDialog dialog(OneSwap(), fixture.viewModel);

    dialog.ShowTheSizes(
        {WeighedSwap{.goesOff = MeasuredFolder{.bytes = kCrjBytes, .measured = true}, .goesOn = MeasuredFolder{}}});

    const QString said = EverythingWritten(dialog);

    QVERIFY(said.contains(AsSize(kCrjBytes)));
    QVERIFY(said.contains(QStringLiteral("hype-atr")));
    QVERIFY(!said.contains(AsSize(0)));
}

void SwapDialogTest::TheSizeThatArrivesIsTheOneTheServiceMeasuredOnDisk()
{
    Fixture fixture;
    std::vector<WeighedSwap> landed;

    fixture.viewModel.WeighTheSwaps(OneSwap(),
                                    [&landed](const std::vector<WeighedSwap>& weighed)
                                    {
                                        landed = weighed;
                                    });

    QCOMPARE(landed.size(), std::size_t{1});
    QCOMPARE(landed.front().goesOff.folder, kCrj);
    QCOMPARE(landed.front().goesOn.folder, kAtr);
    QCOMPARE(landed.front().goesOff.bytes, kCrjBytes);
    QCOMPARE(landed.front().goesOn.bytes, kAtrBytes);
}

QTEST_MAIN(SwapDialogTest)

#include "tst_swap_dialog.moc"
