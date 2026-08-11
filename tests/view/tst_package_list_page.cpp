#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>

#include <string>
#include <vector>

#include "application/CoverageService.h"
#include "application/SceneryService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePackageList.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/simulator/PackageListPage.h"
#include "view/simulator/SimulatorPage.h"
#include "view/simulator/StartupPage.h"
#include "viewmodel/SessionNotifier.h"
#include "viewmodel/StartupViewModel.h"

namespace
{
    class PackageListPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TurnedOffTheScreenSaysSoAndSaysWhereToTurnItOn();
        static void AnInstallationWhereNobodyTurnedAnythingOffOpensWithAnEmptyHalf();
        static void ThePairOfTwoAddonsOffersNoWayToTurnEitherOff();
        static void ThePackageTheSimulatorShipsIsTheOnlyOneTheScreenOffersToTurnOff();
        static void TheTwoButtonsOfTheTabSwapThePanelInsteadOfScrollingIt();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library");

    [[nodiscard]] SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.libraries = {{.id = "library-1", .path = kLibrary, .label = "Library"}};

        return profile;
    }

    [[nodiscard]] AppSettings SettingsThatManageThePackageList(SimulatorProfile profile)
    {
        AppSettings settings = SettingsWith(std::move(profile));
        settings.managePackageList = true;

        return settings;
    }

    [[nodiscard]] TreeNode AddonNamed(const std::string& folderName)
    {
        return {.kind = TreeNodeKind::Addon,
                .path = PathUnder(kLibrary, PathFromUtf8(folderName)),
                .addon = Addon{},
                .children = {},
                .declaredAsCategory = false};
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kLibrary);

            catalog.SetTree(
                kLibrary,
                TreeNode{.kind = TreeNodeKind::Category,
                         .path = kLibrary,
                         .addon = {},
                         .children = {AddonNamed("one-eham"), AddonNamed("another-eham"), AddonNamed("payware-lpma")},
                         .declaredAsCategory = true});

            for (const std::string& name : {std::string("one-eham"), std::string("another-eham")})
            {
                fileSystem.AddFileWithContents(PathUnder(kLibrary, PathFromUtf8(name)) / "scenery" / "APX.bgl",
                                               FakeSceneryParser::Carrying({"EHAM"}));
            }

            fileSystem.AddFileWithContents(PathUnder(kLibrary, PathFromUtf8("payware-lpma")) / "scenery" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"LPMA"}));

            packageList.CarryAnAirport("fs24-asobo-airport-lpma-madeira", "LPMA", PackageActivation::Activated);
            packageList.CarryAnAirport("fs24-asobo-airport-vqpr-paro", "VQPR", PackageActivation::UserDisabled);
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
        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsThatManageThePackageList(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakePackageList packageList;
        CoverageService coverageService{packageList, processProbe, true};
        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService scenery{filesystemProbe, sceneryParser, clock, sceneryCache};
        CoverageViewModel viewModel{coverageService, scenery, session, clock};

        void ReadEverySceneryFolder()
        {
            session.ShowActiveProfile();

            static_cast<void>(
                scenery.SceneryOfEach(SceneryService::AddonsOf(session.Profile(), session.Snapshot()), {}));
        }
    };

    [[nodiscard]] QTreeWidget* Conflicts(const PackageListPage& page)
    {
        return page.findChildren<QTreeWidget*>().value(0);
    }

    [[nodiscard]] QTreeWidget* TurnedOff(const PackageListPage& page)
    {
        return page.findChildren<QTreeWidget*>().value(1);
    }

    [[nodiscard]] QPushButton* ButtonSaying(const QWidget& page, const QString& text)
    {
        for (QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text() == text)
            {
                return button;
            }
        }

        return nullptr;
    }
}

void PackageListPageTest::TurnedOffTheScreenSaysSoAndSaysWhereToTurnItOn()
{
    Fixture f;
    f.coverageService.Manage(false);

    PackageListPage page(f.viewModel);
    page.resize(900, 400);

    QSignalSpy summary(&page, &PackageListPage::SummaryChanged);
    f.viewModel.Show();

    QVERIFY(!summary.isEmpty());
    QVERIFY2(summary.last().first().toString().contains(QStringLiteral("not managed")),
             "the off state is a screen that says where to turn it on, not an absent screen");

    QPushButton* turnOn = ButtonSaying(page, QStringLiteral("Manage it"));
    QVERIFY2(turnOn != nullptr, "the switch lives on the panel of the feature, so one click turns it on");

    turnOn->click();

    QVERIFY(f.viewModel.Managing());
    QVERIFY(f.settings.stored.managePackageList);
}

void PackageListPageTest::AnInstallationWhereNobodyTurnedAnythingOffOpensWithAnEmptyHalf()
{
    Fixture f;
    QCOMPARE(f.packageList.Switch("fs24-asobo-airport-vqpr-paro", true), FileResult::Completed);

    PackageListPage page(f.viewModel);
    f.viewModel.Show();

    QCOMPARE(TurnedOff(page)->topLevelItemCount(), 0);
    QVERIFY2(!ButtonSaying(page, QStringLiteral("Turn it back on"))->isEnabled(),
             "an empty half is a correct answer, not a broken screen");
}

void PackageListPageTest::ThePairOfTwoAddonsOffersNoWayToTurnEitherOff()
{
    Fixture f;
    f.ReadEverySceneryFolder();

    PackageListPage page(f.viewModel);
    f.viewModel.Show();

    QTreeWidget* conflicts = Conflicts(page);
    QVERIFY(conflicts->topLevelItemCount() > 0);

    QTreeWidgetItem* pair = nullptr;
    for (int row = 0; row < conflicts->topLevelItemCount(); ++row)
    {
        if (conflicts->topLevelItem(row)->text(0) == QStringLiteral("EHAM"))
        {
            pair = conflicts->topLevelItem(row);
        }
    }

    QVERIFY(pair != nullptr);
    conflicts->setCurrentItem(pair);

    QVERIFY2(!ButtonSaying(page, QStringLiteral("Turn the simulator's one off"))->isEnabled(),
             "between two addons of the library the app offers no action, because turning one off is enabling and "
             "disabling, which the user already does");

    QPushButton* coexist = ButtonSaying(page, QStringLiteral("They can coexist"));
    QVERIFY(coexist->isEnabled());

    coexist->click();

    QCOMPARE(f.settings.stored.coexistingAirports.size(), std::size_t{1});
    QVERIFY(f.packageList.switched.empty());

    for (int row = 0; row < Conflicts(page)->topLevelItemCount(); ++row)
    {
        QVERIFY2(Conflicts(page)->topLevelItem(row)->text(0) != QStringLiteral("EHAM"),
                 "the pair the user marked stops being shown");
    }
}

void PackageListPageTest::ThePackageTheSimulatorShipsIsTheOnlyOneTheScreenOffersToTurnOff()
{
    Fixture f;
    f.ReadEverySceneryFolder();

    PackageListPage page(f.viewModel);
    f.viewModel.Show();

    QTreeWidget* conflicts = Conflicts(page);

    QTreeWidgetItem* covered = nullptr;
    for (int row = 0; row < conflicts->topLevelItemCount(); ++row)
    {
        if (conflicts->topLevelItem(row)->text(0) == QStringLiteral("LPMA"))
        {
            covered = conflicts->topLevelItem(row);
        }
    }

    QVERIFY(covered != nullptr);
    QCOMPARE(covered->text(2), QStringLiteral("fs24-asobo-airport-lpma-madeira"));

    conflicts->setCurrentItem(covered);

    QVERIFY(!ButtonSaying(page, QStringLiteral("They can coexist"))->isEnabled());

    ButtonSaying(page, QStringLiteral("Turn the simulator's one off"))->click();

    QCOMPARE(f.packageList.switched.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(f.packageList.switched.front().first),
             QStringLiteral("fs24-asobo-airport-lpma-madeira"));
    QVERIFY(!f.packageList.switched.front().second);
}

void PackageListPageTest::TheTwoButtonsOfTheTabSwapThePanelInsteadOfScrollingIt()
{
    Fixture f;
    StartupViewModel startupViewModel(f.startup.service, f.session, f.clock);

    auto* startup = new StartupPage(startupViewModel);
    auto* packages = new PackageListPage(f.viewModel);
    SimulatorPage page(startup, packages);
    page.resize(900, 400);

    auto* panels = page.findChild<QStackedWidget*>();
    QVERIFY(panels != nullptr);
    QCOMPARE(panels->currentWidget(), startup);

    ButtonSaying(page, QStringLiteral("Package list"))->click();
    QCOMPARE(panels->currentWidget(), packages);

    ButtonSaying(page, QStringLiteral("Startup entries"))->click();
    QVERIFY2(panels->currentWidget() == startup,
             "the two features that write into a file of the simulator live in one tab, and the bar swaps the panel");
}

QTEST_MAIN(PackageListPageTest)

#include "tst_package_list_page.moc"
