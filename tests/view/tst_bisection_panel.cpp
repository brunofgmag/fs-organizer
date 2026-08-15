#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "domain/importing/ImportPaths.h"
#include "tests/doubles/FakeBisectionStore.h"
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
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/PathPrinting.h"
#include "view/diagnostics/BisectionPanel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class BisectionPanelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheOpeningPageIsTheOneAPanelShowsBeforeAnythingStarts();
        static void BeginningPutsTheReferenceRoundOnTheScreenWithNothingTurnedOn();
        static void TheHistoryStaysHiddenUntilARoundHasBeenAnswered();
        static void EveryPageOfThePanelHasAWidgetBehindIt();
        static void AGroupOpensIntoItsMembersAndSaysWhichOneOnlyBringsTheName();
    };
}

namespace
{
    constexpr auto kLibraryId = "lib-1";
    constexpr auto kProfileId = "msfs2024";

    const std::filesystem::path kLibrary = "D:/MSFS 2024";
    const std::filesystem::path kCommunity = "E:/Sim/Community";

    constexpr auto kCrj = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kFenix = "D:/MSFS 2024/Aircrafts/fenix-a320";
    constexpr auto kPmdg = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";

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
        TreeNode category;
        category.kind = TreeNodeKind::Category;
        category.path = "D:/MSFS 2024/Aircrafts";
        category.children = {AddonNode(kCrj), AddonNode(kFenix), AddonNode(kPmdg)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {category};

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = kLibraryId, .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);

            for (const std::filesystem::path& addon : {kCrj, kFenix, kPmdg})
            {
                fileSystem.AddDirectory(addon);
                fileSystem.AddFileWithContents(ManifestPathIn(addon), "{}");
                fileSystem.AddLink(PathUnder(kCommunity, addon.filename()), addon);
            }

            catalog.SetTree(kLibrary, LibraryTree());

            const SimulatorProfile profile = Profile();

            static_cast<void>(session.Rewrite(
                [&profile](AppSettings& settings)
                {
                    settings.profiles = {profile};
                    settings.activeProfileId = profile.id;

                    return true;
                }));

            session.ShowActiveProfile();
        }

        mutable InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};
        FakeSidecarStore sidecars{fileSystem};
        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        mutable Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        CouplingScan coupling{filesystemProbe};
        FakeBisectionStore store;
        BisectionService bisection{service, coupling, filesystemProbe, store, clock};
        BisectionViewModel viewModel{bisection, session};
    };

    void PutTheModelFolderIn(const Fixture& f, const std::filesystem::path& addon, const std::string& written)
    {
        const std::string model = "SimObjects/Airplanes/Shared_Model";

        for (const std::string& level :
             {std::string("SimObjects"), std::string("SimObjects/Airplanes"), model, model + "/" + written})
        {
            f.fileSystem.AddDirectory(PathUnder(addon, PathFromUtf8(level)));
        }
    }

    [[nodiscard]] QStackedWidget* PagesOf(const BisectionPanel& panel)
    {
        return panel.findChild<QStackedWidget*>();
    }
}

void BisectionPanelTest::TheOpeningPageIsTheOneAPanelShowsBeforeAnythingStarts()
{
    Fixture f;
    BisectionPanel panel(f.viewModel);
    panel.show();

    QStackedWidget* pages = PagesOf(panel);

    QVERIFY(pages != nullptr);
    QCOMPARE(pages->currentIndex(), 0);
}

void BisectionPanelTest::BeginningPutsTheReferenceRoundOnTheScreenWithNothingTurnedOn()
{
    Fixture f;
    BisectionPanel panel(f.viewModel);
    panel.show();

    f.viewModel.Begin();

    QStackedWidget* pages = PagesOf(panel);

    QVERIFY(pages != nullptr);
    QVERIFY2(pages->currentIndex() != 0, "beginning leaves the opening page");
    QVERIFY2(!pages->currentWidget()->findChildren<QLabel*>().isEmpty(), "the page it landed on has something to read");
    QVERIFY2(f.viewModel.WhatToTurnOn().empty(),
             "the first round is the reference one, and it is the one that turns nothing on");
}

void BisectionPanelTest::TheHistoryStaysHiddenUntilARoundHasBeenAnswered()
{
    Fixture f;
    BisectionPanel panel(f.viewModel);
    panel.show();

    const QList<QTreeWidget*> before = panel.findChildren<QTreeWidget*>();

    const bool anyVisibleBefore = std::ranges::any_of(before,
                                                      [](const QTreeWidget* tree)
                                                      {
                                                          return !tree->isHidden() && tree->topLevelItemCount() > 0;
                                                      });

    QVERIFY2(!anyVisibleBefore, "nothing has happened yet, so no round is listed");

    f.viewModel.Begin();
    f.viewModel.Answer(BisectionAnswer::ItCrashed);

    QVERIFY2(!f.viewModel.Report().story.empty(), "the answered round reached the report");
}

void BisectionPanelTest::EveryPageOfThePanelHasAWidgetBehindIt()
{
    Fixture f;
    BisectionPanel panel(f.viewModel);
    panel.show();

    QStackedWidget* pages = PagesOf(panel);
    QVERIFY(pages != nullptr);

    QCOMPARE(pages->count(), 5);

    for (int at = 0; at < pages->count(); ++at)
    {
        pages->setCurrentIndex(at);

        QVERIFY2(pages->currentWidget() != nullptr, "a page with no widget behind it renders nothing");
        QVERIFY(!pages->currentWidget()->findChildren<QLabel*>().isEmpty());
    }
}

void BisectionPanelTest::AGroupOpensIntoItsMembersAndSaysWhichOneOnlyBringsTheName()
{
    Fixture f;
    PutTheModelFolderIn(f, kCrj, "liveries");
    PutTheModelFolderIn(f, kFenix, "liveries");
    PutTheModelFolderIn(f, kPmdg, "common");

    BisectionPanel panel(f.viewModel);
    panel.show();

    f.viewModel.Show();

    auto* units = panel.findChild<QTreeWidget*>(QStringLiteral("BisectionUnits"));

    QVERIFY(units != nullptr);
    QVERIFY2(units->rootIsDecorated(), "a group with members behind it and no handle to open cannot be opened");
    QCOMPARE(units->topLevelItemCount(), 1);

    QTreeWidgetItem* group = units->topLevelItem(0);

    QCOMPARE(group->childCount(), 3);

    std::map<QString, int> saidOfEachMember;

    for (int at = 0; at < group->childCount(); ++at)
    {
        QVERIFY2(!group->child(at)->text(0).isEmpty(), "a member row with no name says nothing");
        ++saidOfEachMember[group->child(at)->text(2)];
    }

    QCOMPARE(saidOfEachMember.size(), std::size_t{2});
    QCOMPARE(saidOfEachMember.begin()->second + std::next(saidOfEachMember.begin())->second, 3);
    QVERIFY2(std::ranges::any_of(saidOfEachMember,
                                 [](const std::pair<const QString, int>& said)
                                 {
                                     return said.second == 1;
                                 }),
             "one member is held differently from the other two, and the screen has to say so");
}

QTEST_MAIN(BisectionPanelTest)

#include "tst_bisection_panel.moc"
