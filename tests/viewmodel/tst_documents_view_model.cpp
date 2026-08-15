#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeChartCatalogueParser.h"
#include "tests/doubles/FakeChartVersions.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeDocumentIndexCache.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
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
#include "viewmodel/DocumentsViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class DocumentsViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void DocumentsGroupsByAddonAndChartsGroupsByAirport();
        static void TheTypeIsAGroupInsideTheAirportInsteadOfAPrefixRepeatedOnEveryLine();
        static void TheAddonNamesTheAirportHeadingOnlyWhenTheCodeIsNotInIt();
        static void EveryFavouriteOfThePanelIsGatheredOnTopAcrossTheWholeLibrary();
        static void TheHeadingOfAnAddonCarryingOneDocumentSaysNoCount();
        static void TheCaptionNamesWhatIsOpenBecauseATabHasNoTitle();
        static void TheLineOfASupersededChartNamesTheThingAndNotTheRelation();
        static void TheLineSaysWhichPageTheReadingStoppedAt();
        static void WhatWasKeptShowsWithoutWalkingAndTheReadingReplacesIt();
        static void AReadingThatWasStoppedChangesNeitherTheIndexNorWhatWasKept();
        static void TheReadingSaysHowManyAddonsItHasIndexedSoFar();
        static void TheIndexTakesEachAddonAsItArrivesInsteadOfWaitingForTheWholeReading();
        static void TheRereadShowsTheIndexItAlreadyHasAndNotTheOneBeingBuilt();
        static void ThePanelThatCarriesAnAddonIsTheOneWithItsDocumentation();
        static void EachLineSaysWhetherItIsAChartSoTheReaderKnowsTheGesture();
        static void AMarkedPageIsKeptWithTheDocumentThatCarriesIt();
        static void TheMarksComeBackInPageOrderWhateverOrderTheyWereMadeIn();
        static void MarkingAPageThatAlreadyCarriesOneTakesTheMarkAway();
        static void TheNameTheUserGaveIsKeptAndTheDerivedOneIsNot();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library");
    const std::string kBrussels = "aerosoft-airport-ebbr-brussels";
    const std::string kStrasbourg = "francevfr-airport-lfst-strasbourg";
    const std::string kCrj = "aerosoft-crj";
    const std::string kTbm = "bksq-aircraft-tbm850";
    const std::string kCatalogueOfBrussels = "the catalogue of Brussels";

    [[nodiscard]] std::filesystem::path FolderOf(const std::string& folderName)
    {
        return PathUnder(kLibrary, PathFromUtf8(folderName));
    }

    [[nodiscard]] SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.libraries = {{.id = "library-1", .path = kLibrary, .label = "Library"}};

        return profile;
    }

    [[nodiscard]] TreeNode AddonNamed(const std::string& folderName)
    {
        return {.kind = TreeNodeKind::Addon,
                .path = FolderOf(folderName),
                .addon = Addon{},
                .children = {},
                .declaredAsCategory = false};
    }

    [[nodiscard]] ChartCatalogue TheBrusselsCatalogue()
    {
        return {.icao = "EBBR",
                .entries = {{.chartId = "53117", .chartType = "AFC", .chartName = "AFC"},
                            {.chartId = "53206", .chartType = "IAC", .chartName = "ILS or LOC Y 25L"},
                            {.chartId = "53211", .chartType = "AOI", .chartName = "10"},
                            {.chartId = "53241", .chartType = "AOI", .chartName = "10"}}};
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kLibrary);

            catalog.SetTree(kLibrary,
                            TreeNode{.kind = TreeNodeKind::Library,
                                     .path = kLibrary,
                                     .addon = {},
                                     .children = {AddonNamed(kBrussels), AddonNamed(kCrj), AddonNamed(kStrasbourg),
                                                  AddonNamed(kTbm), AddonNamed("sound-mod")},
                                     .declaredAsCategory = true});

            fileSystem.AddFileWithContents(FolderOf(kBrussels) / "scenery" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"EBBR"}));
            fileSystem.AddFileWithContents(FolderOf(kStrasbourg) / "scenery" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"LFST"}));

            for (const std::string& chart :
                 {std::string("53117"), std::string("53206"), std::string("53211"), std::string("53241")})
            {
                fileSystem.AddFile(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / (chart + ".pdf"));
            }

            fileSystem.AddFileWithContents(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "catalogue.json",
                                           kCatalogueOfBrussels);
            catalogueParser.Answer(kCatalogueOfBrussels, TheBrusselsCatalogue());

            fileSystem.AddFile(FolderOf(kStrasbourg) / "Charts" / "LFWH_VAC.pdf");

            fileSystem.AddFile(FolderOf(kCrj) / "Documentation" / "Vol2_Quick Reference Guide_550_700.pdf");
            fileSystem.AddFile(FolderOf(kCrj) / "Documentation" / "Vol4_Normal Ops Checklist.pdf");
            fileSystem.AddFile(FolderOf(kTbm) / "TBM850 manual.pdf");
            fileSystem.AddFile(FolderOf("sound-mod") / "layout.json");

            chartVersions.Answer(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "53211.pdf", 1486377);
            chartVersions.Answer(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "53241.pdf", 1455100);

            session.ShowActiveProfile();
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
        FakeSettingsRepository settings{SettingsWith(Profile())};
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService scenery{filesystemProbe, sceneryParser, clock, sceneryCache};
        FakeChartCatalogueParser catalogueParser;
        FakeChartVersions chartVersions;
        DocumentService documents{catalog, filesystemProbe, catalogueParser, chartVersions};
        FakeDocumentIndexCache cache;
        DocumentsViewModel viewModel{documents, scenery, session, runner, cache, clock};
    };

    [[nodiscard]] const DocumentGroup* GroupNamed(const std::vector<DocumentGroup>& groups, const QString& name)
    {
        for (const DocumentGroup& group : groups)
        {
            if (group.name == name)
            {
                return &group;
            }
        }

        return nullptr;
    }

    [[nodiscard]] DocumentLine TheFirstDocumentOfTheCrj(const Fixture& fixture)
    {
        const std::vector<DocumentGroup> documents = fixture.viewModel.GroupsOf(DocumentPanel::Documents);

        return GroupNamed(documents, QString::fromStdString(kCrj))->lines.front();
    }

}

void DocumentsViewModelTest::DocumentsGroupsByAddonAndChartsGroupsByAirport()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> documents = f.viewModel.GroupsOf(DocumentPanel::Documents);
    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);

    QCOMPARE(documents.size(), std::size_t{2});
    QVERIFY(GroupNamed(documents, QString::fromStdString(kCrj)) != nullptr);
    QVERIFY(GroupNamed(documents, QString::fromStdString(kTbm)) != nullptr);
    QVERIFY2(GroupNamed(documents, QString::fromStdString("sound-mod")) == nullptr,
             "an addon carrying no PDF is not a heading with nothing under it");

    QCOMPARE(charts.size(), std::size_t{2});
    QVERIFY(GroupNamed(charts, QStringLiteral("EBBR")) != nullptr);
    QVERIFY(GroupNamed(charts, QStringLiteral("LFWH")) != nullptr);
}

void DocumentsViewModelTest::TheTypeIsAGroupInsideTheAirportInsteadOfAPrefixRepeatedOnEveryLine()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);
    const DocumentGroup* brussels = GroupNamed(charts, QStringLiteral("EBBR"));

    QVERIFY(brussels != nullptr);
    QVERIFY(brussels->lines.empty());
    QCOMPARE(brussels->groups.size(), std::size_t{3});

    for (const DocumentGroup& type : brussels->groups)
    {
        QVERIFY2(!type.name.contains(QStringLiteral("EBBR")),
                 "the airport is written once on the heading above, not once on every type under it");
    }

    QCOMPARE(brussels->groups.front().name, QStringLiteral("AFC"));
    QCOMPARE(brussels->groups.front().aside, QStringLiteral("Terminal area, frequencies"));
    QCOMPARE(brussels->count, QStringLiteral("4"));
}

void DocumentsViewModelTest::TheAddonNamesTheAirportHeadingOnlyWhenTheCodeIsNotInIt()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);

    QCOMPARE(GroupNamed(charts, QStringLiteral("EBBR"))->aside, QString());
    QCOMPARE(GroupNamed(charts, QStringLiteral("LFWH"))->aside, QString::fromStdString(kStrasbourg));
}

void DocumentsViewModelTest::EveryFavouriteOfThePanelIsGatheredOnTopAcrossTheWholeLibrary()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> before = f.viewModel.GroupsOf(DocumentPanel::Documents);
    const std::vector<DocumentGroup> beforeTheCharts = f.viewModel.GroupsOf(DocumentPanel::Charts);

    f.viewModel.Favour(GroupNamed(before, QString::fromStdString(kCrj))->lines.front(), true);
    f.viewModel.Favour(GroupNamed(beforeTheCharts, QStringLiteral("LFWH"))->groups.front().lines.front(), true);

    const std::vector<DocumentGroup> documents = f.viewModel.GroupsOf(DocumentPanel::Documents);
    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);

    QCOMPARE(documents.front().name, QStringLiteral("Favourites"));
    QCOMPARE(documents.front().lines.size(), std::size_t{1});
    QCOMPARE(documents.front().lines.front().detail, QString::fromStdString(kCrj));

    QCOMPARE(charts.front().name, QStringLiteral("Favourites"));
    QCOMPARE(charts.front().lines.size(), std::size_t{1});
    QVERIFY2(charts.front().lines.front().detail == QStringLiteral("LFWH"),
             "gathered away from its group, a line needs the locator that says where it came from");
}

void DocumentsViewModelTest::TheHeadingOfAnAddonCarryingOneDocumentSaysNoCount()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> documents = f.viewModel.GroupsOf(DocumentPanel::Documents);

    QCOMPARE(GroupNamed(documents, QString::fromStdString(kCrj))->count, QStringLiteral("2"));
    QVERIFY2(GroupNamed(documents, QString::fromStdString(kTbm))->count.isEmpty(),
             "a 1 beside a heading with one line under it is the heading repeating itself");
}

void DocumentsViewModelTest::TheCaptionNamesWhatIsOpenBecauseATabHasNoTitle()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> documents = f.viewModel.GroupsOf(DocumentPanel::Documents);
    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);
    const DocumentGroup* crj = GroupNamed(documents, QString::fromStdString(kCrj));
    const DocumentGroup* information =
        GroupNamed(GroupNamed(charts, QStringLiteral("EBBR"))->groups, QStringLiteral("AOI"));

    QCOMPARE(crj->lines.front().caption,
             QString::fromStdString(kCrj) + QString::fromUtf8(" · Vol2_Quick Reference Guide_550_700"));
    QCOMPARE(information->lines.back().caption, QString::fromUtf8("EBBR · Operating information · Previous edition"));
}

void DocumentsViewModelTest::TheLineOfASupersededChartNamesTheThingAndNotTheRelation()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);
    const DocumentGroup* information =
        GroupNamed(GroupNamed(charts, QStringLiteral("EBBR"))->groups, QStringLiteral("AOI"));

    QCOMPARE(information->lines.size(), std::size_t{2});
    QCOMPARE(information->lines.front().name, QStringLiteral("Operating information"));
    QCOMPARE(information->lines.back().name, QStringLiteral("Previous edition"));
    QCOMPARE(information->lines.front().document, PathFromUtf8("NavDataPro/EBBR/53211.pdf"));
}

void DocumentsViewModelTest::TheLineSaysWhichPageTheReadingStoppedAt()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> before = f.viewModel.GroupsOf(DocumentPanel::Documents);

    QVERIFY(GroupNamed(before, QString::fromStdString(kCrj))->lines.front().detail.isEmpty());

    f.viewModel.RememberThePage(GroupNamed(before, QString::fromStdString(kCrj))->lines.front(), 40);

    const std::vector<DocumentGroup> read = f.viewModel.GroupsOf(DocumentPanel::Documents);
    const DocumentLine& line = GroupNamed(read, QString::fromStdString(kCrj))->lines.front();

    QCOMPARE(line.detail, QStringLiteral("p. 41"));
    QCOMPARE(f.viewModel.PageOf(line), 40);
}

void DocumentsViewModelTest::WhatWasKeptShowsWithoutWalkingAndTheReadingReplacesIt()
{
    Fixture f;
    f.cache.kept =
        RememberedDocuments{.readAt = f.clock.Now(),
                            .addons = {{.addon = {.libraryId = "library-1", .folderName = "an-addon-that-was-deleted"},
                                        .folder = FolderOf("an-addon-that-was-deleted"),
                                        .itWasWalked = true,
                                        .documents = {PathFromUtf8("Manual.pdf")},
                                        .airports = {}}}};

    f.viewModel.ShowWhatWasKept();

    QVERIFY2(f.filesystemProbe.walked.empty(), "what was kept shows without walking a single addon folder");
    QCOMPARE(f.viewModel.GroupsOf(DocumentPanel::Documents).size(), std::size_t{1});
    QCOMPARE(f.viewModel.GroupsOf(DocumentPanel::Documents).front().name, QStringLiteral("an-addon-that-was-deleted"));

    f.viewModel.ReadTheLibrary();

    QCOMPARE(f.viewModel.GroupsOf(DocumentPanel::Documents).size(), std::size_t{2});
    QVERIFY2(GroupNamed(f.viewModel.GroupsOf(DocumentPanel::Documents), QStringLiteral("an-addon-that-was-deleted"))
                 == nullptr,
             "the reading replaces the index instead of adding to it, so what the user deleted stops being shown");
    QCOMPARE(f.cache.writes, std::size_t{1});
    QVERIFY2(f.cache.kept->addons.size() == std::size_t{4},
             "the addon carrying no PDF is not written down, because an entry saying nothing is an entry to read "
             "back and skip");
}

void DocumentsViewModelTest::AReadingThatWasStoppedChangesNeitherTheIndexNorWhatWasKept()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::size_t groups = f.viewModel.GroupsOf(DocumentPanel::Documents).size();
    const std::size_t writes = f.cache.writes;

    QObject::connect(&f.viewModel, &DocumentsViewModel::Progressed, &f.viewModel,
                     [&f]
                     {
                         f.viewModel.Stop();
                     });

    f.viewModel.ReadTheLibrary();

    QCOMPARE(f.viewModel.GroupsOf(DocumentPanel::Documents).size(), groups);
    QVERIFY2(f.cache.writes == writes, "half a library written down would be read next time as the whole of it");
    QVERIFY(!f.viewModel.Reading());
}

void DocumentsViewModelTest::TheReadingSaysHowManyAddonsItHasIndexedSoFar()
{
    Fixture f;
    const QSignalSpy progressed(&f.viewModel, &DocumentsViewModel::Progressed);

    f.viewModel.ReadTheLibrary();

    QCOMPARE(progressed.size(), 5);
    QCOMPARE(progressed.first().at(0).toInt(), 1);
    QCOMPARE(progressed.first().at(1).toInt(), 5);
    QCOMPARE(progressed.last().at(0).toInt(), 5);
    QVERIFY(f.viewModel.ItWasRead());
}

void DocumentsViewModelTest::TheIndexTakesEachAddonAsItArrivesInsteadOfWaitingForTheWholeReading()
{
    Fixture f;
    std::vector<std::size_t> grew;

    QObject::connect(&f.viewModel, &DocumentsViewModel::Arrived, &f.viewModel,
                     [&f, &grew]
                     {
                         grew.push_back(f.viewModel.GroupsOf(DocumentPanel::Documents).size()
                                        + f.viewModel.GroupsOf(DocumentPanel::Charts).size());
                     });

    f.viewModel.ReadTheLibrary();

    QVERIFY2(grew.size() == std::size_t{4},
             "the first reading of all shows a group the moment its addon is read, and the addon carrying nothing "
             "shows none");
    QCOMPARE(grew.front(), std::size_t{1});
    QCOMPARE(grew.back(), std::size_t{4});
}

void DocumentsViewModelTest::TheRereadShowsTheIndexItAlreadyHasAndNotTheOneBeingBuilt()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::size_t whole = f.viewModel.GroupsOf(DocumentPanel::Documents).size();
    const QSignalSpy arrived(&f.viewModel, &DocumentsViewModel::Arrived);
    std::size_t duringTheReread = 0;

    QObject::connect(&f.viewModel, &DocumentsViewModel::Progressed, &f.viewModel,
                     [&f, &duringTheReread]
                     {
                         duringTheReread = f.viewModel.GroupsOf(DocumentPanel::Documents).size();
                     });

    f.viewModel.ReadTheLibrary();

    QCOMPARE(duringTheReread, whole);
    QVERIFY2(arrived.isEmpty(),
             "the reread happens behind the index the user already has, so no half-built one is put on top of it");
}

void DocumentsViewModelTest::ThePanelThatCarriesAnAddonIsTheOneWithItsDocumentation()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::optional<DocumentPlace> brussels = f.viewModel.WhereToFind(kBrussels);
    const std::optional<DocumentPlace> crj = f.viewModel.WhereToFind(kCrj);

    QVERIFY(brussels.has_value());
    QCOMPARE(brussels->panel, DocumentPanel::Charts);
    QCOMPARE(brussels->group, QStringLiteral("EBBR"));

    QVERIFY(crj.has_value());
    QCOMPARE(crj->panel, DocumentPanel::Documents);
    QCOMPARE(crj->group, QString::fromStdString(kCrj));

    QVERIFY(!f.viewModel.WhereToFind("sound-mod").has_value());
}

void DocumentsViewModelTest::EachLineSaysWhetherItIsAChartSoTheReaderKnowsTheGesture()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const std::vector<DocumentGroup> documents = f.viewModel.GroupsOf(DocumentPanel::Documents);
    const std::vector<DocumentGroup> charts = f.viewModel.GroupsOf(DocumentPanel::Charts);
    const DocumentGroup* crj = GroupNamed(documents, QString::fromStdString(kCrj));
    const DocumentGroup* strasbourg = GroupNamed(charts, QStringLiteral("LFWH"));

    QCOMPARE(crj->lines.front().kind, DocumentKind::Document);
    QCOMPARE(strasbourg->groups.front().lines.front().kind, DocumentKind::Chart);
    QCOMPARE(crj->lines.front().file, FolderOf(kCrj) / "Documentation" / "Vol2_Quick Reference Guide_550_700.pdf");
}

void DocumentsViewModelTest::AMarkedPageIsKeptWithTheDocumentThatCarriesIt()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const DocumentLine line = TheFirstDocumentOfTheCrj(f);

    QVERIFY(f.viewModel.BookmarksOf(line).empty());

    f.viewModel.MarkThePage(line, 12, true);

    QCOMPARE(f.viewModel.BookmarksOf(line).size(), std::size_t{1});
    QCOMPARE(f.viewModel.BookmarksOf(line).front().page, 12);
    QCOMPARE(f.settings.stored.documents.size(), std::size_t{1});
    QCOMPARE(f.settings.stored.documents.front().bookmarks.size(), std::size_t{1});
}

void DocumentsViewModelTest::TheMarksComeBackInPageOrderWhateverOrderTheyWereMadeIn()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const DocumentLine line = TheFirstDocumentOfTheCrj(f);

    f.viewModel.MarkThePage(line, 40, true);
    f.viewModel.MarkThePage(line, 7, true);

    const std::vector<DocumentBookmark> marks = f.viewModel.BookmarksOf(line);

    QCOMPARE(marks.size(), std::size_t{2});
    QVERIFY2(marks.front().page == 7,
             "the ordinal of a derived name counts in page order, and the panel nests them under their section in the "
             "same order");
    QCOMPARE(marks.back().page, 40);
}

void DocumentsViewModelTest::MarkingAPageThatAlreadyCarriesOneTakesTheMarkAway()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const DocumentLine line = TheFirstDocumentOfTheCrj(f);

    f.viewModel.MarkThePage(line, 12, true);
    f.viewModel.MarkThePage(line, 40, true);
    f.viewModel.MarkThePage(line, 12, false);

    const std::vector<DocumentBookmark> marks = f.viewModel.BookmarksOf(line);

    QCOMPARE(marks.size(), std::size_t{1});
    QCOMPARE(marks.front().page, 40);
}

void DocumentsViewModelTest::TheNameTheUserGaveIsKeptAndTheDerivedOneIsNot()
{
    Fixture f;
    f.viewModel.ReadTheLibrary();

    const DocumentLine line = TheFirstDocumentOfTheCrj(f);

    f.viewModel.MarkThePage(line, 12, true);
    f.viewModel.NameTheBookmark(line, 12, "Where I stopped");

    QCOMPARE(f.viewModel.BookmarksOf(line).front().name, std::string{"Where I stopped"});
    QCOMPARE(f.settings.stored.documents.front().bookmarks.front().name, std::string{"Where I stopped"});
}

QTEST_APPLESS_MAIN(DocumentsViewModelTest)

#include "tst_documents_view_model.moc"
