#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolButton>

#include <filesystem>

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
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PageFloor.h"
#include "tests/support/PathPrinting.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/PanelRail.h"
#include "view/quarantine/DiscardProgressDialog.h"
#include "view/quarantine/QuarantinePage.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class QuarantinePageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageFitsTheNarrowestWindow();
        static void EverythingHeldLandsOnItsOwnRow();
        static void WithNothingHeldTheScreenSaysSoInsteadOfShowingAnEmptyTable();
        static void SelectingAnItemOpensThePanelAndNamesItAfterTheItem();
        static void SelectingSeveralItemsTitlesThePanelAfterHowManyWerePicked();
        static void TheActionsOnlyLightUpWhenSomethingIsSelected();
        static void EmptyingIsOfferedWhileAnythingIsHeldAndNeverWhenNothingIs();
        static void EmptyingPutsAProgressDialogUpAndTakesItDownWhenTheRunnerLands();
        static void ClosingThePanelLetsGoOfTheSelectionThatSummonedIt();
        static void ALanguageChangeKeepsTheToolbarAndTheEmptyState();
    };
}

namespace
{
    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kQuarantine = "E:/Sim/_fsorganizer-quarantine";
    const std::filesystem::path kBridge = "E:/Sim/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path kAircraft = "E:/Sim/_fsorganizer-quarantine/pmdg-aircraft-77w";

    TreeNode LibraryTree()
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;

        return node;
    }

    SimulatorProfile Active()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kDestination};
        profile.defaultDestination = kDestination;
        profile.libraries = {Library{.id = "lib-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        explicit Fixture(const bool holding = true)
        {
            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kQuarantine);
            catalog.SetTree(kLibrary, LibraryTree());

            if (holding)
            {
                fileSystem.AddDirectory(kBridge);
                fileSystem.AddFile(kBridge / "content.bin", 4096);
                fileSystem.AddDirectory(kAircraft);
                fileSystem.AddFile(kAircraft / "content.bin", 8192);
            }

            session.ShowActiveProfile();
        }

        void Open()
        {
            page.resize(900, 320);
            page.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&page));

            viewModel.Show();
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
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Junction,
                            Verification::ByStructure};
        ImportService service{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                              linking, log,          LinkType::Junction};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Active())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        QuarantineModel model;
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        QuarantineViewModel viewModel{service, profiles, session, notifier, model, sizes, runner};
        QuarantinePage page{viewModel, model};
    };

    QTableView* TableOf(const QuarantinePage& page)
    {
        return page.findChild<QTableView*>();
    }

    ContextPanel* PanelOf(const QuarantinePage& page)
    {
        return page.findChild<ContextPanel*>();
    }

    QString PanelTitleOf(const QuarantinePage& page)
    {
        return PanelOf(page)->findChild<QLabel*>(QStringLiteral("PanelTitle"))->text();
    }

    QString NameOfRow(const QuarantineModel& model, const int row)
    {
        return model.index(row, QuarantineModel::NameColumn, {}).data(Qt::DisplayRole).toString();
    }

    void Pick(const QuarantinePage& page, const int firstRow, const int lastRow)
    {
        QTableView* table = TableOf(page);
        const QAbstractItemModel* shown = table->model();

        table->selectionModel()->clearSelection();

        for (int row = firstRow; row <= lastRow; ++row)
        {
            table->selectionModel()->select(shown->index(row, 0, {}),
                                            QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }

        QCoreApplication::processEvents();
    }
}

void QuarantinePageTest::EverythingHeldLandsOnItsOwnRow()
{
    Fixture f;
    f.Open();

    QCOMPARE(TableOf(f.page)->model()->rowCount({}), 2);
    QCOMPARE(f.page.findChild<QStackedWidget*>()->currentIndex(), 0);
}

void QuarantinePageTest::WithNothingHeldTheScreenSaysSoInsteadOfShowingAnEmptyTable()
{
    Fixture f(false);
    f.Open();

    QCOMPARE(TableOf(f.page)->model()->rowCount({}), 0);
    QCOMPARE(f.page.findChild<QStackedWidget*>()->currentIndex(), 1);
}

void QuarantinePageTest::SelectingAnItemOpensThePanelAndNamesItAfterTheItem()
{
    Fixture f;
    f.Open();

    QVERIFY(!PanelOf(f.page)->isVisible());

    Pick(f.page, 0, 0);

    QVERIFY(PanelOf(f.page)->isVisible());
    QCOMPARE(PanelTitleOf(f.page), NameOfRow(f.model, 0));
}

void QuarantinePageTest::SelectingSeveralItemsTitlesThePanelAfterHowManyWerePicked()
{
    Fixture f;
    f.Open();

    Pick(f.page, 0, 1);

    QVERIFY(PanelOf(f.page)->isVisible());
    QVERIFY(PanelTitleOf(f.page) != NameOfRow(f.model, 0));
    QVERIFY2(PanelTitleOf(f.page).contains(QStringLiteral("2")),
             "a batch is titled after how many were picked, and neither of the two names says two");
}

void QuarantinePageTest::TheActionsOnlyLightUpWhenSomethingIsSelected()
{
    Fixture f;
    f.Open();

    const QPushButton* restore = ButtonSaying(f.page, QStringLiteral("Restore the selected ones"));
    const QPushButton* discard = ButtonSaying(f.page, QStringLiteral("Discard the selected ones"));

    QVERIFY(restore != nullptr);
    QVERIFY(discard != nullptr);
    QVERIFY(!restore->isEnabled());
    QVERIFY(!discard->isEnabled());

    Pick(f.page, 0, 0);

    QVERIFY(restore->isEnabled());
    QVERIFY(discard->isEnabled());
}

void QuarantinePageTest::EmptyingIsOfferedWhileAnythingIsHeldAndNeverWhenNothingIs()
{
    Fixture holding;
    holding.Open();

    QVERIFY(ButtonSaying(holding.page, QStringLiteral("Empty the quarantine"))->isEnabled());

    Fixture empty(false);
    empty.Open();

    QVERIFY(!ButtonSaying(empty.page, QStringLiteral("Empty the quarantine"))->isEnabled());
}

void QuarantinePageTest::EmptyingPutsAProgressDialogUpAndTakesItDownWhenTheRunnerLands()
{
    Fixture f;
    f.Open();

    const QuarantinedItem* held = f.model.ItemAt(f.model.index(0, QuarantineModel::NameColumn, {}));
    QVERIFY(held != nullptr);

    f.runner.defer = true;
    f.viewModel.Discard({*held});

    auto* progress = f.page.findChild<DiscardProgressDialog*>();
    QVERIFY2(progress != nullptr && progress->isVisible(),
             "the deletion runs on the runner now, so the screen owes the user a sign that it is working");

    f.runner.Finish();

    QVERIFY2(!progress->isVisible(), "the sign goes away with the work that summoned it");
}

void QuarantinePageTest::ClosingThePanelLetsGoOfTheSelectionThatSummonedIt()
{
    Fixture f;
    f.Open();
    Pick(f.page, 0, 0);

    PanelOf(f.page)->findChild<QToolButton*>(QStringLiteral("PanelClose"))->click();
    QCoreApplication::processEvents();

    QVERIFY(!PanelOf(f.page)->isVisible());
    QVERIFY(TableOf(f.page)->selectionModel()->selectedRows().isEmpty());
}

void QuarantinePageTest::ALanguageChangeKeepsTheToolbarAndTheEmptyState()
{
    Fixture f(false);
    f.Open();

    QEvent language(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&f.page, &language);
    QCoreApplication::processEvents();

    QVERIFY(ButtonSaying(f.page, QStringLiteral("Empty the quarantine")) != nullptr);
    QCOMPARE(f.page.findChild<QStackedWidget*>()->currentIndex(), 1);
}

void QuarantinePageTest::ThePageFitsTheNarrowestWindow()
{
    Fixture f;
    f.Open();
    Pick(f.page, 0, 0);

    const ContextPanel* panel = PanelOf(f.page);

    QVERIFY2(panel->isVisible() && panel->minimumWidth() > PanelRail::Width(),
             "the guard measured the quarantine without the panel it is meant to fit, either because nothing was "
             "selected or because the panel came back folded from a previous run");

    ItFitsTheNarrowestWindow(f.page, "The quarantine page with an item selected");
}

QTEST_MAIN(QuarantinePageTest)

#include "tst_quarantine_page.moc"
