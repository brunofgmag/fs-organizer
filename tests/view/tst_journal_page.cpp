#include <QtTest/QtTest>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>

#include <chrono>
#include <filesystem>
#include <utility>
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
#include "view/JournalPage.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/PanelRail.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class JournalPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageFitsTheNarrowestWindow();
        static void TheTreeShowsWhatTheJournalRead();
        static void SelectingAnOperationOpensThePanelAndNamesItAfterTheOperation();
        static void ClosingThePanelLetsGoOfTheSelectionThatSummonedIt();
        static void KeepingOnlyWhatFailedLeavesTheFailureAlone();
        static void SearchingReachesTheAddonOfAnOperation();
        static void ReadingItAgainGoesBackToTheJournalInsteadOfTheModel();
        static void AJournalWithNothingInItSaysSoInsteadOfCountingOperations();
        static void ALanguageChangeKeepsTheToolbarAndTheOpenPanel();
    };
}

namespace
{
    const std::filesystem::path kDestination = "E:/Sim/Community";
    const std::filesystem::path kLibrary = "D:/Library";
    const std::filesystem::path kStaged = "D:/Library/Utils/simbridge";
    const std::filesystem::path kLinked = "E:/Sim/Community/pmdg-aircraft-77w";

    std::chrono::system_clock::time_point Moment(const int seconds)
    {
        return std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000 + seconds}};
    }

    AddonId Bridge()
    {
        return AddonId{.libraryId = "lib-1", .folderName = "simbridge"};
    }

    AddonId Aircraft()
    {
        return AddonId{.libraryId = "lib-1", .folderName = "pmdg-aircraft-77w"};
    }

    OperationRecord Step(const OperationKind kind, const int seconds)
    {
        return OperationRecord::OfImport(Moment(seconds), kind, Bridge(), kDestination / "simbridge", kStaged,
                                         FileResult::Completed);
    }

    OperationRecord Link(const OperationKind kind, const int seconds, const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(Moment(seconds), kind, Aircraft(), "D:/Library/Aircrafts/pmdg-aircraft-77w",
                                       kLinked, failure);
    }

    std::vector<OperationRecord> AnImportAndALink(const LinkFailure failure = LinkFailure::None)
    {
        return {
            Step(OperationKind::ImportCopyToStaging, 0),
            Step(OperationKind::ImportVerifyStaging, 1),
            Step(OperationKind::ImportMoveIntoPlace, 2),
            Link(OperationKind::DisableAddon, 3, failure),
        };
    }

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
        explicit Fixture(std::vector<OperationRecord> records = AnImportAndALink())
        {
            journal.appended = std::move(records);

            fileSystem.AddDirectory(kDestination);
            fileSystem.AddDirectory(kLibrary);
            catalog.SetTree(kLibrary, LibraryTree());

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
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings{SettingsWith(Active())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{profiles, organizer, settings, settings.stored, processProbe, runner, notifier};
        JournalModel model;
        JournalViewModel viewModel{journal, session, model};
        JournalPage page{viewModel, model};
    };

    QTreeView* TreeOf(const JournalPage& page)
    {
        return page.findChild<QTreeView*>();
    }

    ContextPanel* PanelOf(const JournalPage& page)
    {
        return page.findChild<ContextPanel*>();
    }

    QString PanelTitleOf(const JournalPage& page)
    {
        return PanelOf(page)->findChild<QLabel*>(QStringLiteral("PanelTitle"))->text();
    }

    QString TheNewestOperationIsCalled(const JournalModel& model)
    {
        return model.index(0, JournalModel::WhenColumn, {}).data(Qt::DisplayRole).toString();
    }

    void ChooseTheNewestOperation(const JournalPage& page)
    {
        QTreeView* tree = TreeOf(page);
        tree->setCurrentIndex(tree->model()->index(0, 0, {}));
        QCoreApplication::processEvents();
    }
}

void JournalPageTest::TheTreeShowsWhatTheJournalRead()
{
    Fixture f;
    f.Open();

    const QAbstractItemModel* shown = TreeOf(f.page)->model();

    QCOMPARE(shown->rowCount({}), 2);
    QCOMPARE(shown->rowCount(shown->index(1, 0, {})), 3);
    QCOMPARE(shown->index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("pmdg-aircraft-77w"));
    QCOMPARE(shown->index(0, JournalModel::LibraryColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("MSFS 2024"));
}

void JournalPageTest::SelectingAnOperationOpensThePanelAndNamesItAfterTheOperation()
{
    Fixture f;
    f.Open();

    QVERIFY(!PanelOf(f.page)->isVisible());

    ChooseTheNewestOperation(f.page);

    QVERIFY(PanelOf(f.page)->isVisible());
    QCOMPARE(PanelTitleOf(f.page), TheNewestOperationIsCalled(f.model));
}

void JournalPageTest::ClosingThePanelLetsGoOfTheSelectionThatSummonedIt()
{
    Fixture f;
    f.Open();
    ChooseTheNewestOperation(f.page);

    PanelOf(f.page)->findChild<QToolButton*>(QStringLiteral("PanelClose"))->click();
    QCoreApplication::processEvents();

    QVERIFY(!PanelOf(f.page)->isVisible());
    QVERIFY(TreeOf(f.page)->selectionModel()->selectedRows().isEmpty());
}

void JournalPageTest::KeepingOnlyWhatFailedLeavesTheFailureAlone()
{
    Fixture f(AnImportAndALink(LinkFailure::CouldNotRemoveLink));
    f.Open();

    QCOMPARE(TreeOf(f.page)->model()->rowCount({}), 2);

    f.page.findChild<QCheckBox*>()->setChecked(true);

    QCOMPARE(TreeOf(f.page)->model()->rowCount({}), 1);
    QCOMPARE(TreeOf(f.page)->model()->index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("pmdg-aircraft-77w"));
}

void JournalPageTest::SearchingReachesTheAddonOfAnOperation()
{
    Fixture f;
    f.Open();

    f.page.findChild<QLineEdit*>()->setText(QStringLiteral("simbridge"));

    const QAbstractItemModel* shown = TreeOf(f.page)->model();

    QCOMPARE(shown->rowCount({}), 1);
    QCOMPARE(shown->index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("simbridge"));
}

void JournalPageTest::ReadingItAgainGoesBackToTheJournalInsteadOfTheModel()
{
    Fixture f;
    f.Open();

    QCOMPARE(TreeOf(f.page)->model()->rowCount({}), 2);

    f.journal.appended.push_back(Link(OperationKind::EnableAddon, 4));
    ButtonSaying(f.page, QStringLiteral("Read the journal again"))->click();

    QCOMPARE(TreeOf(f.page)->model()->rowCount({}), 3);
}

void JournalPageTest::AJournalWithNothingInItSaysSoInsteadOfCountingOperations()
{
    Fixture f({});
    const QSignalSpy summary(&f.page, &JournalPage::SummaryChanged);

    f.Open();

    QCOMPARE(TreeOf(f.page)->model()->rowCount({}), 0);
    QVERIFY(!summary.isEmpty());
    QCOMPARE(summary.back().front().toString(),
             QStringLiteral("The journal has not recorded any change on the disk yet."));
}

void JournalPageTest::ALanguageChangeKeepsTheToolbarAndTheOpenPanel()
{
    Fixture f;
    f.Open();
    ChooseTheNewestOperation(f.page);

    QEvent language(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&f.page, &language);
    QCoreApplication::processEvents();

    QVERIFY(ButtonSaying(f.page, QStringLiteral("Read the journal again")) != nullptr);
    QVERIFY(PanelOf(f.page)->isVisible());
    QCOMPARE(PanelTitleOf(f.page), TheNewestOperationIsCalled(f.model));
}

void JournalPageTest::ThePageFitsTheNarrowestWindow()
{
    Fixture f;
    f.Open();
    ChooseTheNewestOperation(f.page);

    const ContextPanel* panel = PanelOf(f.page);

    QVERIFY2(panel->isVisible() && panel->minimumWidth() > PanelRail::Width(),
             "the guard measured the journal without the panel it is meant to fit, either because nothing was "
             "selected or because the panel came back folded from a previous run");

    ItFitsTheNarrowestWindow(f.page, "The journal page with an operation selected");
}

QTEST_MAIN(JournalPageTest)

#include "tst_journal_page.moc"
