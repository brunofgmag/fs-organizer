#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include <cstddef>
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
#include "view/documents/DocumentsPage.h"
#include "viewmodel/DocumentsViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class DocumentsPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void OneClickOnTheLineOpensIt();
        static void TheStarTurnsTheFavouriteWithoutOpeningAnything();
        static void OnlyTheGlyphOfTheArrowOpensAndClosesTheGroup();
        static void TheBarSwapsTheListAndLeavesOpenWhatWasOpen();
        static void TheNameIsCutInTheMiddleBecauseWhatTellsTwoApartIsAtTheEnd();
        static void DetachingLeavesTheTabAsItWasAndMarksThePlaceItLeft();
        static void TheAddonAsksForThePanelThatCarriesIt();
        static void TheIndexKeepsTheWidthTheFormWasMeasuredAt();
        static void TheProgressAppearsWhenTheReadingStartsAndGoesWhenItEnds();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library");
    const std::string kBrussels = "aerosoft-airport-ebbr-brussels";
    const std::string kCrj = "aerosoft-crj";
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
                            {.chartId = "53206", .chartType = "IAC", .chartName = "ILS or LOC Y 25L"}}};
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
                                     .children = {AddonNamed(kBrussels), AddonNamed(kCrj)},
                                     .declaredAsCategory = true});

            fileSystem.AddFileWithContents(FolderOf(kBrussels) / "scenery" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"EBBR"}));

            fileSystem.AddFile(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "53117.pdf");
            fileSystem.AddFile(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "53206.pdf");
            fileSystem.AddFileWithContents(FolderOf(kBrussels) / "NavDataPro" / "EBBR" / "catalogue.json",
                                           kCatalogueOfBrussels);
            catalogueParser.Answer(kCatalogueOfBrussels, TheBrusselsCatalogue());

            fileSystem.AddFile(FolderOf(kCrj) / "Documentation" / "Vol2_Quick Reference Guide_550_700.pdf");
            fileSystem.AddFile(FolderOf(kCrj) / "Documentation" / "Vol4_Normal Ops Checklist.pdf");

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

    [[nodiscard]] QTreeWidget* TheIndexOf(const DocumentsPage& page, const DocumentPanel panel)
    {
        return page.findChild<QTreeWidget*>(panel == DocumentPanel::Documents ? QStringLiteral("DocumentsIndex")
                                                                              : QStringLiteral("ChartsIndex"));
    }

    [[nodiscard]] QString WhatIsOpen(const DocumentsPage& page)
    {
        const QLabel* caption = page.findChild<QLabel*>(QStringLiteral("ReadingCaption"));

        return caption == nullptr ? QString() : caption->text();
    }

    [[nodiscard]] QTreeWidgetItem* GroupNamed(const QTreeWidget& index, const QString& name)
    {
        for (int group = 0; group < index.topLevelItemCount(); ++group)
        {
            if (index.topLevelItem(group)->text(1) == name)
            {
                return index.topLevelItem(group);
            }
        }

        return nullptr;
    }

    [[nodiscard]] QPushButton* ButtonSaying(const QWidget& page, const QString& text)
    {
        for (QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text().startsWith(text))
            {
                return button;
            }
        }

        return nullptr;
    }

    [[nodiscard]] bool SomethingSays(const QWidget& page, const QString& text)
    {
        for (const QLabel* said : page.findChildren<QLabel*>(QStringLiteral("EmptyBody")))
        {
            if (said->text().contains(text))
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] bool ItSaysItIsReading(const DocumentsPage& page)
    {
        const QPushButton* stop = ButtonSaying(page, QStringLiteral("Stop"));

        return stop != nullptr && stop->isVisible();
    }

    void ClickAt(QTreeWidget& index, const QPoint& where)
    {
        QTest::mouseClick(index.viewport(), Qt::LeftButton, {}, where);
    }
}

void DocumentsPageTest::OneClickOnTheLineOpensIt()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);

    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));

    QVERIFY(crj != nullptr);
    crj->setExpanded(true);

    QTreeWidgetItem* line = crj->child(0);
    const QRect name = index->visualItemRect(line);

    QVERIFY(WhatIsOpen(page).isEmpty());

    ClickAt(*index, QPoint(name.center().x(), name.center().y()));

    QVERIFY2(WhatIsOpen(page).contains(QStringLiteral("Vol2_Quick Reference Guide_550_700")),
             "the line offers one action, so one click on it does that action");
}

void DocumentsPageTest::TheStarTurnsTheFavouriteWithoutOpeningAnything()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));
    crj->setExpanded(true);

    const QRect star = index->visualItemRect(crj->child(0));

    ClickAt(*index, QPoint(star.left() + 4, star.center().y()));

    QCOMPARE(f.settings.stored.documents.size(), std::size_t{1});
    QVERIFY(f.settings.stored.documents.front().favourite);
    QVERIFY2(WhatIsOpen(page).isEmpty(), "the star is the one column that does not open the document");
}

void DocumentsPageTest::OnlyTheGlyphOfTheArrowOpensAndClosesTheGroup()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    f.viewModel.ReadTheLibrary();

    QTreeWidget* index = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*index, QString::fromStdString(kCrj));
    const QRect glyph = index->visualItemRect(crj);

    QVERIFY(!crj->isExpanded());

    ClickAt(*index, QPoint(glyph.right() - 3, glyph.center().y()));

    QVERIFY2(!crj->isExpanded(),
             "the column is sized by its content and the glyph fills a fraction of it, so the rest of it answering "
             "would be a target with nothing drawn on it");

    ClickAt(*index, QPoint(glyph.left() + 3, glyph.center().y()));

    QVERIFY(crj->isExpanded());
}

void DocumentsPageTest::TheBarSwapsTheListAndLeavesOpenWhatWasOpen()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    QTreeWidget* documents = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*documents, QString::fromStdString(kCrj));
    crj->setExpanded(true);

    ClickAt(*documents, documents->visualItemRect(crj->child(0)).center());

    const QString reading = WhatIsOpen(page);
    QVERIFY(!reading.isEmpty());

    page.Show(DocumentPanel::Charts);

    QVERIFY(TheIndexOf(page, DocumentPanel::Charts)->isVisible());
    QVERIFY(!documents->isVisible());
    QCOMPARE(WhatIsOpen(page), reading);
}

void DocumentsPageTest::TheNameIsCutInTheMiddleBecauseWhatTellsTwoApartIsAtTheEnd()
{
    Fixture f;
    const DocumentsPage page(f.viewModel);

    QCOMPARE(TheIndexOf(page, DocumentPanel::Documents)->textElideMode(), Qt::ElideMiddle);
    QCOMPARE(TheIndexOf(page, DocumentPanel::Charts)->textElideMode(), Qt::ElideMiddle);
}

void DocumentsPageTest::DetachingLeavesTheTabAsItWasAndMarksThePlaceItLeft()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    QTreeWidget* documents = TheIndexOf(page, DocumentPanel::Documents);
    QTreeWidgetItem* crj = GroupNamed(*documents, QString::fromStdString(kCrj));
    crj->setExpanded(true);
    ClickAt(*documents, documents->visualItemRect(crj->child(0)).center());

    const QString reading = WhatIsOpen(page);
    const int wide = documents->width();

    page.DetachTheReading();

    QVERIFY2(ButtonSaying(page, QStringLiteral("Documents")) != nullptr,
             "the bar that swaps the panels stays, so nobody comes back to a screen they did not leave");
    QCOMPARE(documents->width(), wide);

    QPushButton* back = ButtonSaying(page, QStringLiteral("Bring it back"));

    QVERIFY2(back != nullptr, "a card marks the place the reading left, and it is the way back");
    QVERIFY(SomethingSays(page, reading));

    back->click();

    QCOMPARE(WhatIsOpen(page), reading);
    QVERIFY2(!ButtonSaying(page, QStringLiteral("Bring it back"))->isVisible(),
             "bringing it back gives the place to the reading it marked");
}

void DocumentsPageTest::TheAddonAsksForThePanelThatCarriesIt()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();
    f.viewModel.ReadTheLibrary();

    page.Reveal(kBrussels);

    QVERIFY(TheIndexOf(page, DocumentPanel::Charts)->isVisible());
    QCOMPARE(TheIndexOf(page, DocumentPanel::Charts)->currentItem()->text(1), QStringLiteral("EBBR"));

    page.Reveal(kCrj);

    QVERIFY(TheIndexOf(page, DocumentPanel::Documents)->isVisible());
    QCOMPARE(TheIndexOf(page, DocumentPanel::Documents)->currentItem()->text(1), QString::fromStdString(kCrj));
}

void DocumentsPageTest::TheIndexKeepsTheWidthTheFormWasMeasuredAt()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1450, 760);
    page.show();
    f.viewModel.ReadTheLibrary();

    QCOMPARE(TheIndexOf(page, DocumentPanel::Documents)->width(), 340);
}

void DocumentsPageTest::TheProgressAppearsWhenTheReadingStartsAndGoesWhenItEnds()
{
    Fixture f;
    DocumentsPage page(f.viewModel);
    page.resize(1120, 621);
    page.show();

    QVERIFY2(!ItSaysItIsReading(page), "a tab that was never told to read is not a tab that is reading");

    f.runner.defer = true;
    f.viewModel.ReadTheLibrary();

    QVERIFY2(ItSaysItIsReading(page),
             "the scenery half runs before the first addon is indexed, so waiting for the first count leaves the "
             "screen saying nothing for as long as it takes");

    f.runner.Finish();

    QVERIFY(!ItSaysItIsReading(page));
}

QTEST_MAIN(DocumentsPageTest)

#include "tst_documents_page.moc"
