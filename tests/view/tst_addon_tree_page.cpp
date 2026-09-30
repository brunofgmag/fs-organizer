#include <QtTest/QtTest>
#include <QtCore/QAbstractProxyModel>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeView>

#include <algorithm>
#include <string>
#include <vector>

#include "application/LibraryOrganizer.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeChartCatalogueParser.h"
#include "tests/doubles/FakeChartVersions.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/PhysicalRows.h"
#include "application/DeletionService.h"
#include "application/ImportService.h"
#include "tests/doubles/FakePackageList.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "view/library/AddonTreePage.h"
#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/DeletionViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "tests/support/PageFloor.h"
#include "view/panels/ContextPanel.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class AddonTreePageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ThePageFitsTheNarrowestWindow();
        static void ThePanelStartsLevelWithTheTree();
        static void ThePanelTitleStripEndsWhereTheColumnHeaderEnds();
        static void ThePanelTitleStripLineLandsOnTheRowsOfTheColumnHeaderLine();
        static void ThePanelLeftLineRunsFromTheTitleStripToTheBottom();
        static void TheScrollBarCapGoesWhenTheScrollBarDoes();
        static void TheToolbarSpansTheWholePageOverThePanel();
        static void TheTreeDoesNotJumpWhenThePanelOpensOnANarrowPage();
        static void ARescanPutsTheSelectionAndTheScrollBackWhereTheyWere();
        static void ARescanKeepsTheCurrentRowOfASelectionThatSpansSeveralAddons();
        static void AnAddonThatMovedRowIsFoundAgainBecauseItIsRememberedByPath();
        static void AnAddonThatVanishedLeavesTheTreeWithNothingSelected();
        static void ABatchWithNothingToDoSaysTheSelectionWasAlreadyAsAsked();
        static void ABatchStoppedByTheDiskSaysSoInsteadOfClaimingTheSelectionWasAlreadyRight();
        static void TheToolbarIsTwoLinesAt1024WithTheChipsUnderRefreshAndTheSearchAtTheMargin();
        static void TheToolbarIsOneLineAt1440WithTheDesignedGaps();
        static void TheToolbarKeepsItsHeightAndBottomWhenAnAddonIsSelected();
        static void TheChipsShowTheirCountsWithoutPaddingAndStartOnAll();
        static void AChipWithNoAddonsIsMarkedAsEmpty();
        static void EveryChipReservesTheDigitsOfAllSoNoSplitOf62ChangesTheToolbar();
        static void EveryChipReservesTheDigitsOfAllSoNoSplitOf1204ChangesTheToolbar();
        static void TheChipsAreMeasuredAgainInTheLanguageThatWasSwitchedTo();
        static void TabWalksTheActionsThenTheChipsThenTheCheckboxThenTheSearch();
        static void TypingASearchLeavesTheChipCountsAlone();
        static void TheStateFilterAndTheSearchBothHaveToHoldOnThePage();
        static void ACategoryWithoutAMatchingAddonIsHiddenUnderEnabledAndDisabled();
        static void WhileFilteringEachCountReadsShownOfTotalAndTheTotalsReturnAfterwards();
        static void TheCountsFollowAnAddonToggledUnderTheStateFilter();
        static void TheCountsWhileFilteringSpeakPortuguese();
        static void TheFilterDropsTheSelectionOfAnAddonItHides();
        static void WhenTheCheckedChipRunsOutTheFilterReturnsToAllAndSaysSo();
        static void WhenTheDisabledChipRunsOutTheFilterReturnsToAllAndSaysSo();
        static void AZeroCountChipTheUserClicksStaysChecked();
        static void EnableSelectedOnACategoryLeavesTheAddonsTheStateFilterHides();
        static void TheCheckboxOfACategoryLeavesTheAddonsTheStateFilterHides();
        static void EnableSelectedOnACategoryLeavesTheAddonsASearchHides();
        static void WithoutAnyFilterEnableSelectedReachesEveryAddonOfTheCategory();
        static void WithoutAnyFilterTheCheckboxOfACategoryReachesEveryAddonOfIt();
        static void TheMoveButtonCountsTheSelectedAddonsThatHaveACategoryToGoTo();
        static void TheMoveButtonStaysOffWhenTheOnlyCategoryIsTheOneTheAddonsSitIn();
        static void ASelectionOffersRelinkWhenAnyMemberStrayedEvenIfTheClickedOneDidNot();
        static void TheFilterLeavesTheOfferToKeepTheDestinationOfACategoryThatStrayed();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kSecondCommunity = "E:/Flight Simulator 2024/Community2024";
    constexpr auto kChosen = "D:/MSFS 2024/Aircrafts/addon-17";
    constexpr auto kCompanion = "D:/MSFS 2024/Aircrafts/addon-05";
    constexpr int kAddonsPerCategory = 20;

    const QStringList& Categories()
    {
        static const QStringList categories{QStringLiteral("Aircrafts"), QStringLiteral("Sceneries"),
                                            QStringLiteral("Traffic")};

        return categories;
    }

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    std::filesystem::path AddonPath(const QString& category, const int index)
    {
        return std::filesystem::path(kLibrary) / category.toStdString()
            / ("addon-" + QStringLiteral("%1").arg(index, 2, 10, QLatin1Char('0')).toStdString());
    }

    TreeNode CategoryNode(const QString& category, const std::vector<int>& order, const std::filesystem::path& skipped)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = std::filesystem::path(kLibrary) / category.toStdString();

        for (const int index : order)
        {
            const std::filesystem::path path = AddonPath(category, index);

            if (ComparablePath(path) != ComparablePath(skipped))
            {
                node.children.push_back(AddonNode(path));
            }
        }

        return node;
    }

    std::vector<int> Ascending()
    {
        std::vector<int> order;

        for (int index = 0; index < kAddonsPerCategory; ++index)
        {
            order.push_back(index);
        }

        return order;
    }

    TreeNode LibraryTree(const std::vector<int>& order, const std::filesystem::path& skipped)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;

        for (const QString& category : Categories())
        {
            node.children.push_back(CategoryNode(category, order, skipped));
        }

        return node;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    SimulatorProfile ProfileWithTwoDestinations()
    {
        SimulatorProfile profile = Profile();
        profile.destinations = {kCommunity, kSecondCommunity};

        return profile;
    }

    struct Fixture
    {
        explicit Fixture(SimulatorProfile chosenProfile = Profile()) : settings(SettingsWith(std::move(chosenProfile)))
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kSecondCommunity);
            fileSystem.AddDirectory(kLibrary);

            for (const QString& category : Categories())
            {
                fileSystem.AddDirectory(std::filesystem::path(kLibrary) / category.toStdString());

                for (int index = 0; index < kAddonsPerCategory; ++index)
                {
                    fileSystem.AddDirectory(AddonPath(category, index));
                }
            }

            catalog.SetTree(kLibrary, LibraryTree(Ascending(), {}));
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
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        SizeService sizes{catalog, filesystemProbe, clock, runner};
        AddonTreeModel model;
        FakeSimulatorPackages packages;
        AddonTreeViewModel viewModel{session, service, model, packages, sizes, runner, notifier};
        DeletionService deletionService{filesystemProbe, files,        sidecars, linking,
                                        classifier,      processProbe, log,      sizes};
        DeletionViewModel deletion{session, service, deletionService, sizes, runner};
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Junction,
                            Verification::ByStructure};
        ImportService importService{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                                    linking, log,          LinkType::Junction};
        ImportViewModel importViewModel{importService, service, processProbe, session, runner};
        FakePackageList packageList;
        CoverageService coverageService{packageList, processProbe, false};
        FakeSceneryParser sceneryParser;
        FakeSceneryCache sceneryCache;
        SceneryService sceneryService{filesystemProbe, sceneryParser, clock, sceneryCache};
        CoverageViewModel coverage{coverageService, sceneryService, session, clock, runner};
        FakeChartCatalogueParser catalogueParser;
        FakeChartVersions chartVersions;
        DocumentService documentService{filesystemProbe, catalogueParser, chartVersions};
        AddonDocumentsViewModel documents{documentService, sceneryService, session, runner};
    };

    const TreeNode* NodeUnder(const QTreeView& tree, const QModelIndex& position)
    {
        const auto* filter = qobject_cast<const QAbstractProxyModel*>(tree.model());

        return AddonTreeModel::NodeAt(filter->mapToSource(position));
    }

    QModelIndex IndexOf(const QTreeView& tree, const std::filesystem::path& path, const QModelIndex& parent)
    {
        for (int row = 0; row < tree.model()->rowCount(parent); ++row)
        {
            const QModelIndex position = tree.model()->index(row, 0, parent);
            const TreeNode* node = NodeUnder(tree, position);

            if (node != nullptr && ComparablePath(node->path) == ComparablePath(path))
            {
                return position;
            }

            if (const QModelIndex found = IndexOf(tree, path, position); found.isValid())
            {
                return found;
            }
        }

        return {};
    }

    void OpenEveryCategory(QTreeView& tree)
    {
        for (int row = 0; row < tree.model()->rowCount({}); ++row)
        {
            const QModelIndex library = tree.model()->index(row, 0, {});
            tree.expand(library);

            for (int child = 0; child < tree.model()->rowCount(library); ++child)
            {
                tree.expand(tree.model()->index(child, 0, library));
            }
        }
    }

    const QPushButton* MoveButtonOf(const QWidget& page)
    {
        for (const QPushButton* button : page.findChildren<QPushButton*>())
        {
            if (button->text().startsWith(QStringLiteral("Move")))
            {
                return button;
            }
        }

        return nullptr;
    }

    struct Screen
    {
        explicit Screen(Fixture& fixture)
            : page(fixture.viewModel,
                   fixture.deletion,
                   fixture.importViewModel,
                   fixture.coverage,
                   fixture.documents,
                   fixture.model,
                   fixture.notifier)
        {
            page.resize(900, 320);
            page.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&page));

            fixture.viewModel.ShowActiveProfile();

            tree = page.findChild<QTreeView*>();
            OpenEveryCategory(*tree);
        }

        [[nodiscard]] std::vector<std::string> SelectedPaths() const
        {
            std::vector<std::string> paths;

            for (const QModelIndex& position : tree->selectionModel()->selectedRows())
            {
                if (const TreeNode* node = NodeUnder(*tree, position))
                {
                    paths.push_back(ComparablePath(node->path));
                }
            }

            return paths;
        }

        [[nodiscard]] std::string CurrentPath() const
        {
            const TreeNode* node = NodeUnder(*tree, tree->selectionModel()->currentIndex());

            return node == nullptr ? std::string{} : ComparablePath(node->path);
        }

        AddonTreePage page;
        QTreeView* tree = nullptr;
    };
}

void AddonTreePageTest::ARescanPutsTheSelectionAndTheScrollBackWhereTheyWere()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());

    screen.tree->setCurrentIndex(chosen);

    QScrollBar* bar = screen.tree->verticalScrollBar();
    QVERIFY(bar->maximum() > 0);

    bar->setValue(bar->maximum() / 2);
    const int scrolled = bar->value();
    QVERIFY(scrolled > 0);

    f.viewModel.ShowActiveProfile();
    f.viewModel.ShowActiveProfile();

    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{ComparablePath(kChosen)});
    QCOMPARE(screen.CurrentPath(), ComparablePath(kChosen));
    QCOMPARE(screen.tree->verticalScrollBar()->value(), scrolled);
}

void AddonTreePageTest::ARescanKeepsTheCurrentRowOfASelectionThatSpansSeveralAddons()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex first = IndexOf(*screen.tree, kCompanion, {});
    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(first.isValid());
    QVERIFY(chosen.isValid());

    screen.tree->selectionModel()->select(first, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    screen.tree->selectionModel()->setCurrentIndex(chosen, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    QCOMPARE(screen.CurrentPath(), ComparablePath(kChosen));

    f.viewModel.ShowActiveProfile();
    f.viewModel.ShowActiveProfile();

    QCOMPARE(screen.SelectedPaths(), (std::vector<std::string>{ComparablePath(kCompanion), ComparablePath(kChosen)}));
    QCOMPARE(screen.CurrentPath(), ComparablePath(kChosen));
}

void AddonTreePageTest::AnAddonThatMovedRowIsFoundAgainBecauseItIsRememberedByPath()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());

    const int rowBefore = chosen.row();
    screen.tree->setCurrentIndex(chosen);

    std::vector<int> reversed = Ascending();
    std::ranges::reverse(reversed);
    f.catalog.SetTree(kLibrary, LibraryTree(reversed, {}));

    f.viewModel.ShowActiveProfile();

    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{ComparablePath(kChosen)});
    QVERIFY(screen.tree->selectionModel()->selectedRows().front().row() != rowBefore);
}

void AddonTreePageTest::AnAddonThatVanishedLeavesTheTreeWithNothingSelected()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());

    screen.tree->setCurrentIndex(chosen);

    QScrollBar* bar = screen.tree->verticalScrollBar();
    bar->setValue(bar->maximum() / 2);
    QVERIFY(bar->value() > 0);

    f.catalog.SetTree(kLibrary, LibraryTree(Ascending(), kChosen));

    f.viewModel.ShowActiveProfile();

    QVERIFY(!IndexOf(*screen.tree, kChosen, {}).isValid());
    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{});
}

namespace
{
    const TreeNode* AddonOf(const Screen& screen, const std::filesystem::path& path)
    {
        const QModelIndex position = IndexOf(*screen.tree, path, {});

        return position.isValid() ? NodeUnder(*screen.tree, position) : nullptr;
    }

    QString LastStatusOf(const QSignalSpy& spy)
    {
        return spy.isEmpty() ? QString{} : spy.back().front().toString();
    }
}

void AddonTreePageTest::ABatchWithNothingToDoSaysTheSelectionWasAlreadyAsAsked()
{
    Fixture f;
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "addon-17", kChosen);

    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    const TreeNode* addon = AddonOf(screen, kChosen);
    QVERIFY(addon != nullptr);

    f.viewModel.Toggle({addon}, true);

    QCOMPARE(LastStatusOf(status), QString{"Nothing to do: the selection is already that way."});
}

void AddonTreePageTest::ABatchStoppedByTheDiskSaysSoInsteadOfClaimingTheSelectionWasAlreadyRight()
{
    Fixture f;
    const std::filesystem::path link = std::filesystem::path(kCommunity) / "addon-17";
    f.fileSystem.AddLink(link, kChosen);

    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    const TreeNode* addon = AddonOf(screen, kChosen);
    QVERIFY(addon != nullptr);
    QVERIFY(f.fileSystem.RemoveNode(link));

    f.viewModel.Toggle({addon}, false);

    QCOMPARE(LastStatusOf(status),
             QString{"Nothing changed: 1 addon had changed on the disk. The list has been refreshed."});
}

void AddonTreePageTest::ThePageFitsTheNarrowestWindow()
{
    Fixture f;
    Screen screen(f);

    const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(chosen.isValid());
    screen.tree->setCurrentIndex(chosen);
    QCoreApplication::processEvents();

    QVERIFY2(screen.page.findChild<ContextPanel*>()->isVisible(),
             "the guard measured the library without the panel it is meant to fit");

    ItFitsTheNarrowestWindow(screen.page, "The library page with an addon selected");
}

namespace
{
    int TopWithin(const QWidget& page, const QWidget& widget)
    {
        return widget.mapTo(&page, QPoint{}).y();
    }

    int BottomWithin(const QWidget& page, const QWidget& widget)
    {
        return widget.mapTo(&page, QPoint{0, widget.height()}).y();
    }

    int RightEdgeWithin(const QWidget& page, const QWidget& widget)
    {
        return widget.mapTo(&page, QPoint{widget.width(), 0}).x();
    }

    void SelectTheChosenAddon(const Screen& screen)
    {
        const QModelIndex chosen = IndexOf(*screen.tree, kChosen, {});
        QVERIFY(chosen.isValid());
        screen.tree->setCurrentIndex(chosen);
        QCoreApplication::processEvents();
    }
}

void AddonTreePageTest::ThePanelStartsLevelWithTheTree()
{
    Fixture f;
    Screen screen(f);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    SelectTheChosenAddon(screen);

    const auto* panel = screen.page.findChild<ContextPanel*>();

    QVERIFY(panel != nullptr);
    QVERIFY(panel->isVisible());
    QCOMPARE(TopWithin(screen.page, *panel), TopWithin(screen.page, *screen.tree));
}

void AddonTreePageTest::ThePanelTitleStripEndsWhereTheColumnHeaderEnds()
{
    Fixture f;
    Screen screen(f);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    SelectTheChosenAddon(screen);

    const auto* strip = screen.page.findChild<ContextPanel*>()->findChild<QWidget*>(QStringLiteral("PanelHeader"));

    QVERIFY(strip != nullptr);
    QVERIFY(strip->isVisible());
    QCOMPARE(BottomWithin(screen.page, *strip), BottomWithin(screen.page, *screen.tree->header()));
}

void AddonTreePageTest::ThePanelTitleStripLineLandsOnTheRowsOfTheColumnHeaderLine()
{
    Fixture f;
    Screen screen(f);
    ApplyModernistTheme(*qApp);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    SelectTheChosenAddon(screen);

    const auto* strip = screen.page.findChild<ContextPanel*>()->findChild<QWidget*>(QStringLiteral("PanelHeader"));
    auto* header = screen.tree->header();

    QVERIFY(strip != nullptr);
    QVERIFY(strip->isVisible());
    LetTheScrollBarShow(screen.page, *screen.tree);
    QVERIFY(screen.tree->verticalScrollBar()->isVisible());

    const QWidget* cap = ScrollBarCapOf(*screen.tree);

    QVERIFY(cap != nullptr);
    QVERIFY(cap->isVisible());
    MakeTheColumnHeaderOnePixelShorter(*header);
    TheThreeRulesLandOnTheSamePhysicalRows(screen.page, *strip, *header, *cap);
}

void AddonTreePageTest::ThePanelLeftLineRunsFromTheTitleStripToTheBottom()
{
    Fixture f;
    Screen screen(f);
    ApplyModernistTheme(*qApp);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    SelectTheChosenAddon(screen);

    const auto* panel = screen.page.findChild<ContextPanel*>();
    const auto* strip = panel->findChild<QWidget*>(QStringLiteral("PanelHeader"));
    const auto* body = panel->findChild<QWidget*>(QStringLiteral("PanelBody"));

    QVERIFY(strip != nullptr);
    QVERIFY(body != nullptr);
    QVERIFY(strip->isVisible());
    LetTheScrollBarShow(screen.page, *screen.tree);
    QVERIFY(screen.tree->verticalScrollBar()->isVisible());
    TheLeftRuleRunsTheWholeHeightOfThePanel(screen.page, *strip, *body);
}

void AddonTreePageTest::TheScrollBarCapGoesWhenTheScrollBarDoes()
{
    Fixture f;
    Screen screen(f);
    ApplyModernistTheme(*qApp);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    SelectTheChosenAddon(screen);
    LetTheScrollBarGo(screen.page);

    const QWidget* cap = ScrollBarCapOf(*screen.tree);

    QVERIFY(cap != nullptr);
    QVERIFY(!screen.tree->verticalScrollBar()->isVisible());
    QVERIFY(!cap->isVisible());
}

void AddonTreePageTest::TheToolbarSpansTheWholePageOverThePanel()
{
    Fixture f;
    Screen screen(f);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    SelectTheChosenAddon(screen);

    const auto* toolbar = screen.page.findChild<QWidget*>(QStringLiteral("PageToolbar"));

    QVERIFY(screen.page.findChild<ContextPanel*>()->isVisible());
    QVERIFY(toolbar != nullptr);
    QCOMPARE(RightEdgeWithin(screen.page, *toolbar), screen.page.width());
}

void AddonTreePageTest::TheTreeDoesNotJumpWhenThePanelOpensOnANarrowPage()
{
    Fixture f;
    Screen screen(f);
    screen.page.resize(kWidestAPageMayBe, 600);
    QCoreApplication::processEvents();

    const int before = TopWithin(screen.page, *screen.tree);

    SelectTheChosenAddon(screen);

    QVERIFY(screen.page.findChild<ContextPanel*>()->isVisible());
    QCOMPARE(TopWithin(screen.page, *screen.tree), before);
}

namespace
{
    constexpr int kWide = 1440;
    constexpr int kDesignedGapBetweenTheChipsAndTheCheckbox = 16;
    constexpr auto kAircrafts = "Aircrafts";

    void Settle()
    {
        for (int round = 0; round < 3; ++round)
        {
            QCoreApplication::processEvents();
        }
    }

    class ChipTranslator final : public QTranslator
    {
    public:
        [[nodiscard]] bool isEmpty() const override
        {
            return false;
        }

        [[nodiscard]] QString
        translate(const char* context, const char* source, const char* disambiguation, int) const override
        {
            if (QLatin1String(context) != QLatin1String("AddonTreePage") || disambiguation == nullptr
                || QLatin1String(disambiguation) != QLatin1String("several addons"))
            {
                return {};
            }

            const QLatin1String word(source);

            if (word == QLatin1String("All"))
            {
                return QStringLiteral("Todos");
            }

            if (word == QLatin1String("Enabled"))
            {
                return QStringLiteral("Ativados");
            }

            return word == QLatin1String("Disabled") ? QStringLiteral("Desativados") : QString{};
        }
    };

    struct Installed
    {
        explicit Installed(QTranslator& translator) : translator_(translator)
        {
            QCoreApplication::installTranslator(&translator_);
            Settle();
        }

        ~Installed()
        {
            QCoreApplication::removeTranslator(&translator_);
            Settle();
        }

        Installed(const Installed&) = delete;
        Installed& operator=(const Installed&) = delete;

        QTranslator& translator_;
    };

    std::filesystem::path NumberedPath(const int index)
    {
        return std::filesystem::path(kLibrary) / kAircrafts
            / ("addon-" + QStringLiteral("%1").arg(index, 4, 10, QLatin1Char('0')).toStdString());
    }

    std::filesystem::path NumberedLink(const int index)
    {
        return std::filesystem::path(kCommunity) / ("numbered-" + std::to_string(index));
    }

    std::filesystem::path LinkNamed(const QString& category, const int index)
    {
        return std::filesystem::path(kCommunity) / (category.toStdString() + "-" + std::to_string(index));
    }

    void Enable(Fixture& fixture, const QString& category, const int index)
    {
        fixture.fileSystem.AddLink(LinkNamed(category, index), AddonPath(category, index));
    }

    void EnableTheFirst(Fixture& fixture, const QString& category, const int count)
    {
        for (int index = 0; index < count; ++index)
        {
            Enable(fixture, category, index);
        }
    }

    bool IsEnabled(const Fixture& fixture, const QString& category, const int index)
    {
        return fixture.session.Snapshot().enabled.Contains(AddonPath(category, index));
    }

    struct Counted : Fixture
    {
        explicit Counted(const int howMany)
        {
            TreeNode category;
            category.kind = TreeNodeKind::Category;
            category.path = std::filesystem::path(kLibrary) / kAircrafts;

            for (int index = 0; index < howMany; ++index)
            {
                fileSystem.AddDirectory(NumberedPath(index));
                category.children.push_back(AddonNode(NumberedPath(index)));
            }

            TreeNode library;
            library.kind = TreeNodeKind::Library;
            library.path = kLibrary;
            library.children.push_back(std::move(category));

            catalog.SetTree(kLibrary, library);
        }

        void EnableUpTo(const int count)
        {
            while (linked_ < count)
            {
                fileSystem.AddLink(NumberedLink(linked_), NumberedPath(linked_));
                ++linked_;
            }
        }

        void DisableTheLast()
        {
            QVERIFY(fileSystem.RemoveNode(NumberedLink(--linked_)));
        }

        int linked_ = 0;
    };

    QWidget* ToolbarOf(const Screen& screen)
    {
        return screen.page.findChild<QWidget*>(QStringLiteral("PageToolbar"));
    }

    QList<QPushButton*> ActionsOf(const QWidget& bar)
    {
        return bar.findChildren<QPushButton*>(QString{}, Qt::FindDirectChildrenOnly);
    }

    QList<QToolButton*> ChipsOf(const QWidget& bar)
    {
        return bar.findChildren<QToolButton*>(QStringLiteral("FilterChip"));
    }

    QWidget* ChipHolderOf(const QWidget& bar)
    {
        return bar.findChild<QWidget*>(QStringLiteral("LibraryStateFilter"));
    }

    QCheckBox* HideEmptyOf(const QWidget& bar)
    {
        return bar.findChild<QCheckBox*>(QStringLiteral("LibraryHideEmpty"));
    }

    QLineEdit* SearchOf(const QWidget& bar)
    {
        return bar.findChild<QLineEdit*>(QStringLiteral("LibrarySearch"));
    }

    QToolButton* ChipNumber(const Screen& screen, const int position)
    {
        return ChipsOf(*ToolbarOf(screen)).at(position);
    }

    QStringList TextsOf(const QList<QToolButton*>& chips)
    {
        QStringList texts;

        for (const QToolButton* chip : chips)
        {
            texts.append(chip->text());
        }

        return texts;
    }

    QRect PlaceIn(const QWidget& bar, const QWidget& widget)
    {
        return {widget.mapTo(&bar, QPoint{}), widget.size()};
    }

    int Right(const QRect& place)
    {
        return place.x() + place.width();
    }

    int Bottom(const QRect& place)
    {
        return place.y() + place.height();
    }

    int CenterOf(const QRect& place)
    {
        return place.y() + place.height() / 2;
    }

    void ResizeTo(Screen& screen, const int width)
    {
        screen.page.resize(width, 600);
        Settle();
    }

    struct Footprint
    {
        int filter{};
        QList<int> chips{};
        QSize oneLine{};
        int heightAtTheNarrowest{};
        int heightAtTheWide{};

        [[nodiscard]] bool operator==(const Footprint& other) const = default;

        [[nodiscard]] QString Said() const
        {
            QStringList widths;

            for (const int width : chips)
            {
                widths.append(QString::number(width));
            }

            return QStringLiteral("filter %1, chips %2, one line %3x%4, height %5 and %6")
                .arg(filter)
                .arg(widths.join(QLatin1Char('/')))
                .arg(oneLine.width())
                .arg(oneLine.height())
                .arg(heightAtTheNarrowest)
                .arg(heightAtTheWide);
        }
    };

    Footprint FootprintOf(const QWidget& bar)
    {
        Footprint footprint;
        footprint.filter = ChipHolderOf(bar)->sizeHint().width();

        for (const QToolButton* chip : ChipsOf(bar))
        {
            footprint.chips.append(std::max(chip->minimumWidth(), chip->sizeHint().width()));
        }

        footprint.oneLine = bar.layout()->sizeHint();
        footprint.heightAtTheNarrowest = bar.heightForWidth(kWidestAPageMayBe);
        footprint.heightAtTheWide = bar.heightForWidth(kWide);

        return footprint;
    }

    QString NameOf(const TreeNode& node)
    {
        return AsText(node.path.parent_path().filename()) + QLatin1Char('/') + AsText(node.path.filename());
    }

    QStringList ShownOfKind(const QTreeView& tree, const TreeNodeKind kind, const QModelIndex& parent = {})
    {
        QStringList names;

        for (int row = 0; row < tree.model()->rowCount(parent); ++row)
        {
            const QModelIndex position = tree.model()->index(row, 0, parent);

            if (const TreeNode* node = NodeUnder(tree, position); node != nullptr && node->kind == kind)
            {
                names.append(NameOf(*node));
            }

            names.append(ShownOfKind(tree, kind, position));
        }

        return names;
    }

    QStringList ShownAddons(const Screen& screen)
    {
        return ShownOfKind(*screen.tree, TreeNodeKind::Addon);
    }

    QStringList ShownCategories(const Screen& screen)
    {
        return ShownOfKind(*screen.tree, TreeNodeKind::Category);
    }

    QString NameOfAddon(const QString& category, const int index)
    {
        return category + QLatin1Char('/') + QStringLiteral("addon-%1").arg(index, 2, 10, QLatin1Char('0'));
    }

    void Select(const Screen& screen, const std::filesystem::path& path)
    {
        const QModelIndex position = IndexOf(*screen.tree, path, {});
        QVERIFY(position.isValid());
        screen.tree->setCurrentIndex(position);
        Settle();
    }

    std::filesystem::path CategoryPath(const QString& category)
    {
        return std::filesystem::path(kLibrary) / category.toStdString();
    }

    void AnswerYesToTheNextQuestion()
    {
        QTimer::singleShot(0, qApp,
                           []
                           {
                               if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                                   question != nullptr)
                               {
                                   question->button(QMessageBox::Yes)->click();
                               }
                           });
    }

    void PressEnableSelected(const Screen& screen)
    {
        QPushButton* enable = ButtonSaying(screen.page, QStringLiteral("Enable selected"));
        QVERIFY(enable != nullptr);

        AnswerYesToTheNextQuestion();
        enable->click();
        Settle();
    }

    void ClickTheCheckboxOf(Fixture& fixture, const Screen& screen, const std::filesystem::path& path)
    {
        const QModelIndex position = IndexOf(*screen.tree, path, {});
        QVERIFY(position.isValid());

        const auto* filter = qobject_cast<const QAbstractProxyModel*>(screen.tree->model());

        AnswerYesToTheNextQuestion();
        QVERIFY(!fixture.model.setData(filter->mapToSource(position), Qt::Checked, Qt::CheckStateRole));
        Settle();
    }
}

void AddonTreePageTest::TheToolbarIsTwoLinesAt1024WithTheChipsUnderRefreshAndTheSearchAtTheMargin()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    Screen screen(f);
    ResizeTo(screen, kWidestAPageMayBe);

    const QWidget* bar = ToolbarOf(screen);

    QVERIFY2(bar->layout()->sizeHint().width() > kWidestAPageMayBe,
             "the toolbar fits on one line at 1024 px, so this test no longer meets two lines");

    const QList<QPushButton*> actions = ActionsOf(*bar);
    QCOMPARE(actions.size(), 4);

    const QRect refresh = PlaceIn(*bar, *actions.at(0));
    int lineHeight = 0;

    for (const QPushButton* action : actions)
    {
        QCOMPARE(PlaceIn(*bar, *action).y(), refresh.y());
        lineHeight = std::max(lineHeight, action->height());
    }

    const QRect filter = PlaceIn(*bar, *ChipHolderOf(*bar));
    const QRect hideEmpty = PlaceIn(*bar, *HideEmptyOf(*bar));
    const QRect search = PlaceIn(*bar, *SearchOf(*bar));

    QCOMPARE(refresh.topLeft(), QPoint(kPageGutter, kPageGutter));
    QCOMPARE(filter.x(), refresh.x());
    const int secondLineTop = kPageGutter + lineHeight + kToolbarGap;
    const int secondLineBottom = bar->height() - kPageGutter;

    for (const QRect& onTheSecondLine : {filter, hideEmpty, search})
    {
        QVERIFY(onTheSecondLine.y() >= secondLineTop);
        QVERIFY(Bottom(onTheSecondLine) <= secondLineBottom);
    }

    QCOMPARE(Right(search), bar->width() - kPageGutter);
    QCOMPARE(search.x() - Right(hideEmpty), kToolbarGap);
    QVERIFY(hideEmpty.x() > Right(filter));
    QCOMPARE(bar->height(), bar->heightForWidth(bar->width()));
}

void AddonTreePageTest::TheToolbarIsOneLineAt1440WithTheDesignedGaps()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    Screen screen(f);
    ResizeTo(screen, kWide);

    const QWidget* bar = ToolbarOf(screen);

    QVERIFY2(bar->layout()->sizeHint().width() <= kWide,
             "the toolbar needs two lines at 1440 px, so this test no longer meets one line");

    const QList<QPushButton*> actions = ActionsOf(*bar);
    const QList<QToolButton*> chips = ChipsOf(*bar);
    const QRect refresh = PlaceIn(*bar, *actions.at(0));
    const QRect undo = PlaceIn(*bar, *actions.at(3));
    const QRect filter = PlaceIn(*bar, *ChipHolderOf(*bar));
    const QRect lastChip = PlaceIn(*bar, *chips.last());
    const QRect hideEmpty = PlaceIn(*bar, *HideEmptyOf(*bar));
    const QRect search = PlaceIn(*bar, *SearchOf(*bar));

    for (const QRect& onTheLine : {filter, hideEmpty, search})
    {
        QVERIFY(std::abs(CenterOf(onTheLine) - CenterOf(refresh)) <= 1);
        QVERIFY(onTheLine.y() >= kPageGutter);
        QVERIFY(Bottom(onTheLine) <= bar->height() - kPageGutter);
    }

    QCOMPARE(bar->height(), bar->layout()->sizeHint().height());
    QVERIFY(filter.x() - Right(undo) >= kToolbarGap + kToolbarGap);
    QCOMPARE(hideEmpty.x() - Right(lastChip), kDesignedGapBetweenTheChipsAndTheCheckbox);
    QCOMPARE(search.x() - Right(hideEmpty), kToolbarGap);
    QCOMPARE(Right(search), bar->width() - kPageGutter);
}

void AddonTreePageTest::TheToolbarKeepsItsHeightAndBottomWhenAnAddonIsSelected()
{
    ApplyModernistTheme(*qApp);

    for (const int width : {kWidestAPageMayBe, kWide})
    {
        Fixture f;
        Screen screen(f);
        ResizeTo(screen, width);

        const QWidget* bar = ToolbarOf(screen);
        const int heightBefore = bar->height();
        const int bottomBefore = BottomWithin(screen.page, *bar);

        SelectTheChosenAddon(screen);

        QVERIFY(screen.page.findChild<ContextPanel*>()->isVisible());
        QCOMPARE(bar->height(), heightBefore);
        QCOMPARE(BottomWithin(screen.page, *bar), bottomBefore);
    }
}

void AddonTreePageTest::TheChipsShowTheirCountsWithoutPaddingAndStartOnAll()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 20);
    EnableTheFirst(f, QStringLiteral("Sceneries"), 20);
    EnableTheFirst(f, QStringLiteral("Traffic"), 11);
    const Screen screen(f);

    const QList<QToolButton*> chips = ChipsOf(*ToolbarOf(screen));

    QCOMPARE(chips.size(), 3);
    QCOMPARE(TextsOf(chips),
             (QStringList{QStringLiteral("All 60"), QStringLiteral("Enabled 51"), QStringLiteral("Disabled 9")}));
    QVERIFY(chips.at(0)->isChecked());
    QVERIFY(!chips.at(1)->isChecked());
    QVERIFY(!chips.at(2)->isChecked());

    for (const QToolButton* chip : chips)
    {
        QCOMPARE(chip->property("population").toString(), QStringLiteral("some"));
    }
}

void AddonTreePageTest::AChipWithNoAddonsIsMarkedAsEmpty()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    const Screen screen(f);

    const QList<QToolButton*> chips = ChipsOf(*ToolbarOf(screen));

    QCOMPARE(chips.at(0)->property("population").toString(), QStringLiteral("some"));
    QCOMPARE(chips.at(1)->property("population").toString(), QStringLiteral("none"));
    QCOMPARE(chips.at(2)->property("population").toString(), QStringLiteral("some"));

    EnableTheFirst(f, QStringLiteral("Aircrafts"), 1);
    f.viewModel.ShowActiveProfile();

    QCOMPARE(chips.at(1)->property("population").toString(), QStringLiteral("some"));
}

void AddonTreePageTest::EveryChipReservesTheDigitsOfAllSoNoSplitOf62ChangesTheToolbar()
{
    ApplyModernistTheme(*qApp);
    constexpr int kAll = 62;
    Counted f(kAll);
    const Screen screen(f);
    const QWidget* bar = ToolbarOf(screen);

    Settle();

    const Footprint first = FootprintOf(*bar);

    for (int enabled = 0; enabled <= kAll; ++enabled)
    {
        f.EnableUpTo(enabled);
        f.viewModel.ShowActiveProfile();
        Settle();

        QCOMPARE(TextsOf(ChipsOf(*bar)),
                 (QStringList{QStringLiteral("All %1").arg(kAll), QStringLiteral("Enabled %1").arg(enabled),
                              QStringLiteral("Disabled %1").arg(kAll - enabled)}));

        const Footprint now = FootprintOf(*bar);

        QVERIFY2(now == first,
                 qPrintable(QStringLiteral("with %1 enabled the toolbar measures %2, and with none it measured %3")
                                .arg(enabled)
                                .arg(now.Said(), first.Said())));
    }

    for (QToolButton* chip : ChipsOf(*bar))
    {
        chip->click();
        Settle();

        QVERIFY2(
            FootprintOf(*bar) == first,
            qPrintable(QStringLiteral("with %1 checked the toolbar measures %2, and with All checked it measured %3")
                           .arg(chip->text(), FootprintOf(*bar).Said(), first.Said())));
    }
}

void AddonTreePageTest::EveryChipReservesTheDigitsOfAllSoNoSplitOf1204ChangesTheToolbar()
{
    ApplyModernistTheme(*qApp);
    constexpr int kAll = 1204;
    const std::vector<int> aroundTheDigitsThatChange{0,   1,    9,    10,   11,   99,   100, 101,
                                                     999, 1000, 1001, 1199, 1200, 1203, 1204};
    Counted f(kAll);
    const Screen screen(f);
    const QWidget* bar = ToolbarOf(screen);

    Settle();

    const Footprint first = FootprintOf(*bar);

    for (const int enabled : aroundTheDigitsThatChange)
    {
        f.EnableUpTo(enabled);
        f.viewModel.ShowActiveProfile();
        Settle();

        QCOMPARE(TextsOf(ChipsOf(*bar)),
                 (QStringList{QStringLiteral("All %1").arg(kAll), QStringLiteral("Enabled %1").arg(enabled),
                              QStringLiteral("Disabled %1").arg(kAll - enabled)}));

        const Footprint now = FootprintOf(*bar);

        QVERIFY2(now == first,
                 qPrintable(QStringLiteral("with %1 enabled the toolbar measures %2, and with none it measured %3")
                                .arg(enabled)
                                .arg(now.Said(), first.Said())));
    }
}

void AddonTreePageTest::TheChipsAreMeasuredAgainInTheLanguageThatWasSwitchedTo()
{
    ApplyModernistTheme(*qApp);
    Counted f(62);
    f.EnableUpTo(53);
    const Screen screen(f);
    const QWidget* bar = ToolbarOf(screen);

    QCOMPARE(TextsOf(ChipsOf(*bar)),
             (QStringList{QStringLiteral("All 62"), QStringLiteral("Enabled 53"), QStringLiteral("Disabled 9")}));

    Settle();

    const Footprint english = FootprintOf(*bar);

    ChipTranslator portuguese;
    const Installed installed(portuguese);

    QCOMPARE(TextsOf(ChipsOf(*bar)),
             (QStringList{QStringLiteral("Todos 62"), QStringLiteral("Ativados 53"), QStringLiteral("Desativados 9")}));

    const Footprint translated = FootprintOf(*bar);

    QVERIFY(translated.chips.at(2) > english.chips.at(2));

    f.DisableTheLast();
    f.viewModel.ShowActiveProfile();
    Settle();

    QCOMPARE(
        TextsOf(ChipsOf(*bar)),
        (QStringList{QStringLiteral("Todos 62"), QStringLiteral("Ativados 52"), QStringLiteral("Desativados 10")}));
    QVERIFY2(FootprintOf(*bar) == translated,
             qPrintable(QStringLiteral("going from 9 to 10 disabled changed %1 into %2")
                            .arg(translated.Said(), FootprintOf(*bar).Said())));

    for (const QToolButton* chip : ChipsOf(*bar))
    {
        QVERIFY(std::max(chip->minimumWidth(), chip->sizeHint().width())
                >= chip->fontMetrics().horizontalAdvance(chip->text()));
    }
}

void AddonTreePageTest::TabWalksTheActionsThenTheChipsThenTheCheckboxThenTheSearch()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    Screen screen(f);
    ResizeTo(screen, kWide);

    const TreeNode* addon = AddonOf(screen, kChosen);
    QVERIFY(addon != nullptr);
    f.viewModel.Toggle({addon}, true);
    screen.page.RefreshUndoState();

    const QWidget* bar = ToolbarOf(screen);
    const QList<QPushButton*> actions = ActionsOf(*bar);

    QVERIFY2(actions.at(3)->isEnabled(), "Undo must be enabled, or Tab skips it");

    screen.page.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&screen.page));

    actions.at(0)->setFocus();
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(actions.at(0)));

    const QList<QWidget*> inReadingOrder{actions.at(1),       actions.at(2),     actions.at(3),
                                         ChipsOf(*bar).at(0), HideEmptyOf(*bar), SearchOf(*bar)};

    for (QWidget* next : inReadingOrder)
    {
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), next);
    }
}

void AddonTreePageTest::TypingASearchLeavesTheChipCountsAlone()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);
    const QWidget* bar = ToolbarOf(screen);

    const QStringList before = TextsOf(ChipsOf(*bar));
    const qsizetype addonsBefore = ShownAddons(screen).size();

    SearchOf(*bar)->setText(QStringLiteral("addon-1"));
    Settle();

    QCOMPARE(before,
             (QStringList{QStringLiteral("All 60"), QStringLiteral("Enabled 5"), QStringLiteral("Disabled 55")}));
    QCOMPARE(TextsOf(ChipsOf(*bar)), before);
    QVERIFY(ShownAddons(screen).size() < addonsBefore);

    HideEmptyOf(*bar)->setChecked(true);
    Settle();

    QCOMPARE(TextsOf(ChipsOf(*bar)), before);
}

void AddonTreePageTest::TheStateFilterAndTheSearchBothHaveToHoldOnThePage()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);
    const QWidget* bar = ToolbarOf(screen);

    ChipNumber(screen, 1)->click();
    SearchOf(*bar)->setText(QStringLiteral("addon-03"));

    QCOMPARE(ShownAddons(screen), QStringList{NameOfAddon(QStringLiteral("Aircrafts"), 3)});

    SearchOf(*bar)->setText(QStringLiteral("addon-1"));

    QCOMPARE(ShownAddons(screen), QStringList{});

    ChipNumber(screen, 2)->click();
    SearchOf(*bar)->setText(QStringLiteral("addon-03"));

    QCOMPARE(ShownAddons(screen),
             (QStringList{NameOfAddon(QStringLiteral("Sceneries"), 3), NameOfAddon(QStringLiteral("Traffic"), 3)}));

    ChipNumber(screen, 0)->click();

    QCOMPARE(ShownAddons(screen),
             (QStringList{NameOfAddon(QStringLiteral("Aircrafts"), 3), NameOfAddon(QStringLiteral("Sceneries"), 3),
                          NameOfAddon(QStringLiteral("Traffic"), 3)}));
}

void AddonTreePageTest::ACategoryWithoutAMatchingAddonIsHiddenUnderEnabledAndDisabled()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 20);
    const Screen screen(f);

    QCOMPARE(ShownCategories(screen).size(), 3);

    ChipNumber(screen, 1)->click();
    QCOMPARE(ShownCategories(screen), QStringList{QStringLiteral("MSFS 2024/Aircrafts")});

    ChipNumber(screen, 2)->click();
    QCOMPARE(ShownCategories(screen),
             (QStringList{QStringLiteral("MSFS 2024/Sceneries"), QStringLiteral("MSFS 2024/Traffic")}));

    ChipNumber(screen, 0)->click();
    QCOMPARE(ShownCategories(screen).size(), 3);
}

namespace
{
    class CountTranslator final : public QTranslator
    {
    public:
        [[nodiscard]] bool isEmpty() const override
        {
            return false;
        }

        [[nodiscard]] QString
        translate(const char* context, const char* source, const char* disambiguation, const int n) const override
        {
            if (QLatin1String(context) != QLatin1String("AddonTreeFilterModel") || disambiguation == nullptr)
            {
                return {};
            }

            const QLatin1String text(source);
            const bool single = n == 1;

            if (text == QLatin1String("%1 of %2"))
            {
                return QStringLiteral("%1 de %2");
            }

            if (text == QLatin1String("%1 of %n category"))
            {
                return single ? QStringLiteral("%1 de %n categoria") : QStringLiteral("%1 de %n categorias");
            }

            if (text != QLatin1String("%1 of %n addon"))
            {
                return {};
            }

            return single ? QStringLiteral("%1 de %n addon") : QStringLiteral("%1 de %n addons");
        }
    };

    QString CountText(const Screen& screen, const std::filesystem::path& path)
    {
        const QModelIndex position = IndexOf(*screen.tree, path, {});

        return position.isValid() ? screen.tree->model()->data(position, QuietSuffixRole).toString()
                                  : QStringLiteral("not shown");
    }

    QString LibraryCountText(const Screen& screen)
    {
        return screen.tree->model()->data(screen.tree->model()->index(0, 0, {}), QuietSuffixRole).toString();
    }
}

void AddonTreePageTest::WhileFilteringEachCountReadsShownOfTotalAndTheTotalsReturnAfterwards()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);
    const QWidget* bar = ToolbarOf(screen);
    const std::filesystem::path aircrafts = CategoryPath(QStringLiteral("Aircrafts"));

    const QString totalOfTheCategory = CountText(screen, aircrafts);
    const QString totalOfTheLibrary = LibraryCountText(screen);

    ChipNumber(screen, 1)->click();
    Settle();

    QCOMPARE(CountText(screen, aircrafts), QStringLiteral("5 of 20"));
    QCOMPARE(CountText(screen, CategoryPath(QStringLiteral("Sceneries"))), QStringLiteral("not shown"));
    QCOMPARE(LibraryCountText(screen), QStringLiteral("1 of 3 category · 5 of 60 addon"));

    ChipNumber(screen, 2)->click();
    Settle();

    QCOMPARE(CountText(screen, aircrafts), QStringLiteral("15 of 20"));
    QCOMPARE(CountText(screen, CategoryPath(QStringLiteral("Sceneries"))), QStringLiteral("20 of 20"));
    QCOMPARE(LibraryCountText(screen), QStringLiteral("3 of 3 category · 55 of 60 addon"));

    ChipNumber(screen, 0)->click();
    SearchOf(*bar)->setText(QStringLiteral("addon-03"));
    Settle();

    QCOMPARE(CountText(screen, aircrafts), QStringLiteral("1 of 20"));
    QCOMPARE(LibraryCountText(screen), QStringLiteral("3 of 3 category · 3 of 60 addon"));

    SearchOf(*bar)->setText({});
    Settle();

    QCOMPARE(CountText(screen, aircrafts), totalOfTheCategory);
    QCOMPARE(LibraryCountText(screen), totalOfTheLibrary);
    QVERIFY(!totalOfTheCategory.contains(QStringLiteral(" of ")));
    QVERIFY(!totalOfTheLibrary.contains(QStringLiteral(" of ")));
}

void AddonTreePageTest::TheCountsFollowAnAddonToggledUnderTheStateFilter()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    Screen screen(f);

    const TreeNode* enabled = AddonOf(screen, AddonPath(QStringLiteral("Aircrafts"), 2));
    const TreeNode* disabled = AddonOf(screen, AddonPath(QStringLiteral("Aircrafts"), 9));
    QVERIFY(enabled != nullptr);
    QVERIFY(disabled != nullptr);

    ChipNumber(screen, 1)->click();
    Settle();

    QCOMPARE(CountText(screen, CategoryPath(QStringLiteral("Aircrafts"))), QStringLiteral("5 of 20"));

    f.viewModel.Toggle({enabled}, false);
    Settle();

    QCOMPARE(CountText(screen, CategoryPath(QStringLiteral("Aircrafts"))), QStringLiteral("4 of 20"));
    QCOMPARE(LibraryCountText(screen), QStringLiteral("1 of 3 category · 4 of 60 addon"));

    f.viewModel.Toggle({disabled}, true);
    Settle();

    QCOMPARE(CountText(screen, CategoryPath(QStringLiteral("Aircrafts"))), QStringLiteral("5 of 20"));
    QCOMPARE(LibraryCountText(screen), QStringLiteral("1 of 3 category · 5 of 60 addon"));
}

void AddonTreePageTest::TheCountsWhileFilteringSpeakPortuguese()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);

    ChipNumber(screen, 1)->click();
    Settle();

    CountTranslator portuguese;
    const Installed installed(portuguese);

    QCOMPARE(CountText(screen, CategoryPath(QStringLiteral("Aircrafts"))), QStringLiteral("5 de 20"));
    QCOMPARE(LibraryCountText(screen), QStringLiteral("1 de 3 categorias · 5 de 60 addons"));

    ChipNumber(screen, 2)->click();
    Settle();

    QCOMPARE(LibraryCountText(screen), QStringLiteral("3 de 3 categorias · 55 de 60 addons"));
}

void AddonTreePageTest::TheFilterDropsTheSelectionOfAnAddonItHides()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);
    const std::filesystem::path hiddenLater = AddonPath(QStringLiteral("Aircrafts"), 10);

    Select(screen, hiddenLater);
    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{ComparablePath(hiddenLater)});

    ChipNumber(screen, 1)->click();
    Settle();

    QCOMPARE(screen.SelectedPaths(), std::vector<std::string>{});
}

void AddonTreePageTest::WhenTheCheckedChipRunsOutTheFilterReturnsToAllAndSaysSo()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    ChipNumber(screen, 1)->click();
    QCOMPARE(ShownAddons(screen).size(), 5);

    Select(screen, CategoryPath(QStringLiteral("Aircrafts")));
    QPushButton* disable = ButtonSaying(screen.page, QStringLiteral("Disable selected"));
    QVERIFY(disable != nullptr);
    disable->click();
    Settle();

    QCOMPARE(f.model.EnabledCount(), std::size_t{0});
    QVERIFY(ChipNumber(screen, 0)->isChecked());
    QVERIFY(!ChipNumber(screen, 1)->isChecked());
    QCOMPARE(ShownAddons(screen).size(), 60);
    QCOMPARE(LastStatusOf(status), QString{"No addon is enabled now, so the filter was cleared."});
}

void AddonTreePageTest::WhenTheDisabledChipRunsOutTheFilterReturnsToAllAndSaysSo()
{
    ApplyModernistTheme(*qApp);
    Fixture f;

    for (const QString& category : Categories())
    {
        for (int index = 0; index < kAddonsPerCategory; ++index)
        {
            if (category != QStringLiteral("Traffic") || index != 7)
            {
                Enable(f, category, index);
            }
        }
    }

    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    ChipNumber(screen, 2)->click();
    QCOMPARE(ShownAddons(screen), QStringList{NameOfAddon(QStringLiteral("Traffic"), 7)});

    Select(screen, AddonPath(QStringLiteral("Traffic"), 7));
    PressEnableSelected(screen);

    QCOMPARE(f.model.EnabledCount(), std::size_t{60});
    QVERIFY(ChipNumber(screen, 0)->isChecked());
    QVERIFY(!ChipNumber(screen, 2)->isChecked());
    QCOMPARE(ShownAddons(screen).size(), 60);
    QCOMPARE(LastStatusOf(status), QString{"No addon is disabled now, so the filter was cleared."});
}

void AddonTreePageTest::AZeroCountChipTheUserClicksStaysChecked()
{
    ApplyModernistTheme(*qApp);
    Fixture f;

    for (const QString& category : Categories())
    {
        for (int index = 0; index < kAddonsPerCategory; ++index)
        {
            Enable(f, category, index);
        }
    }

    const Screen screen(f);
    QSignalSpy status(&screen.page, &AddonTreePage::StatusChanged);

    QCOMPARE(ChipNumber(screen, 2)->text(), QStringLiteral("Disabled 0"));

    ChipNumber(screen, 2)->click();
    Settle();

    QVERIFY(ChipNumber(screen, 2)->isChecked());
    QVERIFY(!ChipNumber(screen, 0)->isChecked());
    QCOMPARE(ChipNumber(screen, 2)->property("population").toString(), QStringLiteral("none"));
    QCOMPARE(ShownAddons(screen).size(), 0);
    QVERIFY(status.isEmpty());

    const QImage photo = ChipNumber(screen, 2)->grab().toImage();
    const QColor accent = TonesOf(CurrentColorScheme()).accent;

    QCOMPARE(photo.pixelColor(4, photo.height() / 2).rgb(), accent.rgb());
}

void AddonTreePageTest::EnableSelectedOnACategoryLeavesTheAddonsTheStateFilterHides()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);

    ChipNumber(screen, 1)->click();
    Select(screen, CategoryPath(QStringLiteral("Aircrafts")));
    PressEnableSelected(screen);

    for (int index = 0; index < kAddonsPerCategory; ++index)
    {
        QCOMPARE(IsEnabled(f, QStringLiteral("Aircrafts"), index), index < 5);
    }

    QCOMPARE(f.model.EnabledCount(), std::size_t{5});
}

void AddonTreePageTest::TheCheckboxOfACategoryLeavesTheAddonsTheStateFilterHides()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);

    ChipNumber(screen, 1)->click();
    ClickTheCheckboxOf(f, screen, CategoryPath(QStringLiteral("Aircrafts")));

    for (int index = 5; index < kAddonsPerCategory; ++index)
    {
        QVERIFY2(!IsEnabled(f, QStringLiteral("Aircrafts"), index),
                 qPrintable(QStringLiteral("the hidden addon %1 was reached").arg(index)));
    }

    QCOMPARE(f.model.EnabledCount(), std::size_t{0});
}

void AddonTreePageTest::EnableSelectedOnACategoryLeavesTheAddonsASearchHides()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);

    SearchOf(*ToolbarOf(screen))->setText(QStringLiteral("addon-1"));
    Settle();

    Select(screen, CategoryPath(QStringLiteral("Aircrafts")));
    PressEnableSelected(screen);

    for (int index = 0; index < kAddonsPerCategory; ++index)
    {
        QCOMPARE(IsEnabled(f, QStringLiteral("Aircrafts"), index), index < 5 || index >= 10);
    }
}

void AddonTreePageTest::WithoutAnyFilterEnableSelectedReachesEveryAddonOfTheCategory()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);

    Select(screen, CategoryPath(QStringLiteral("Aircrafts")));
    PressEnableSelected(screen);

    for (int index = 0; index < kAddonsPerCategory; ++index)
    {
        QVERIFY(IsEnabled(f, QStringLiteral("Aircrafts"), index));
    }

    QVERIFY(!IsEnabled(f, QStringLiteral("Sceneries"), 0));
}

void AddonTreePageTest::WithoutAnyFilterTheCheckboxOfACategoryReachesEveryAddonOfIt()
{
    ApplyModernistTheme(*qApp);
    Fixture f;
    EnableTheFirst(f, QStringLiteral("Aircrafts"), 5);
    const Screen screen(f);

    ClickTheCheckboxOf(f, screen, CategoryPath(QStringLiteral("Aircrafts")));

    for (int index = 0; index < kAddonsPerCategory; ++index)
    {
        QVERIFY(IsEnabled(f, QStringLiteral("Aircrafts"), index));
    }

    QVERIFY(!IsEnabled(f, QStringLiteral("Sceneries"), 0));
}

void AddonTreePageTest::TheMoveButtonCountsTheSelectedAddonsThatHaveACategoryToGoTo()
{
    Fixture f;
    const Screen screen(f);

    const QModelIndex first = IndexOf(*screen.tree, kCompanion, {});
    const QModelIndex second = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    const QPushButton* move = MoveButtonOf(screen.page);
    QVERIFY(move != nullptr);

    screen.tree->selectionModel()->select(first, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    QVERIFY(move->isEnabled());
    QCOMPARE(move->text(), QStringLiteral("Move to…"));

    screen.tree->selectionModel()->select(second, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    QVERIFY(move->isEnabled());
    QCOMPARE(move->text(), QStringLiteral("Move 2 addon to…"));
}

void AddonTreePageTest::TheMoveButtonStaysOffWhenTheOnlyCategoryIsTheOneTheAddonsSitIn()
{
    Fixture f;

    TreeNode library;
    library.kind = TreeNodeKind::Library;
    library.path = kLibrary;
    library.children.push_back(CategoryNode(QStringLiteral("Aircrafts"), Ascending(), {}));
    f.catalog.SetTree(kLibrary, library);

    const Screen screen(f);

    const QModelIndex first = IndexOf(*screen.tree, kCompanion, {});
    const QModelIndex second = IndexOf(*screen.tree, kChosen, {});
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    const QPushButton* move = MoveButtonOf(screen.page);
    QVERIFY(move != nullptr);

    screen.tree->selectionModel()->select(first, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    screen.tree->selectionModel()->select(second, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    QVERIFY(!move->isEnabled());
    QCOMPARE(move->text(), QStringLiteral("Move to…"));
}

namespace
{
    QStringList OfferedByTheMenuOn(const Screen& screen, const std::filesystem::path& path)
    {
        const QModelIndex position = IndexOf(*screen.tree, path, {});
        QStringList offered;

        QTimer::singleShot(0, qApp,
                           [&offered]
                           {
                               if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                                   menu != nullptr)
                               {
                                   for (const QAction* action : menu->actions())
                                   {
                                       offered.push_back(action->text());
                                   }

                                   menu->close();
                               }
                           });
        Q_EMIT screen.tree->customContextMenuRequested(screen.tree->visualRect(position).center());

        return offered;
    }

    const QString kRelink = QStringLiteral("Relink in the profile destination");
    const QString kKeepTheDestination = QStringLiteral("Keep the destination they are linked in");
}

void AddonTreePageTest::ASelectionOffersRelinkWhenAnyMemberStrayedEvenIfTheClickedOneDidNot()
{
    ApplyModernistTheme(*qApp);
    Fixture f(ProfileWithTwoDestinations());
    f.fileSystem.AddLink(std::filesystem::path(kSecondCommunity) / "strayed",
                         AddonPath(QStringLiteral("Aircrafts"), 0));
    const Screen screen(f);

    const QModelIndex strayed = IndexOf(*screen.tree, AddonPath(QStringLiteral("Aircrafts"), 0), {});
    const QModelIndex innocent = IndexOf(*screen.tree, AddonPath(QStringLiteral("Aircrafts"), 1), {});
    QVERIFY(strayed.isValid());
    QVERIFY(innocent.isValid());
    screen.tree->selectionModel()->select(strayed, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    screen.tree->selectionModel()->select(innocent, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    Settle();
    QCOMPARE(screen.SelectedPaths().size(), std::size_t{2});

    QVERIFY(OfferedByTheMenuOn(screen, AddonPath(QStringLiteral("Aircrafts"), 1)).contains(kRelink));

    screen.tree->selectionModel()->select(innocent, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    Settle();

    QVERIFY(!OfferedByTheMenuOn(screen, AddonPath(QStringLiteral("Aircrafts"), 1)).contains(kRelink));
}

void AddonTreePageTest::TheFilterLeavesTheOfferToKeepTheDestinationOfACategoryThatStrayed()
{
    ApplyModernistTheme(*qApp);
    Fixture f(ProfileWithTwoDestinations());
    f.fileSystem.AddLink(std::filesystem::path(kSecondCommunity) / "strayed",
                         AddonPath(QStringLiteral("Aircrafts"), 0));
    const Screen screen(f);

    const QStringList before = OfferedByTheMenuOn(screen, CategoryPath(QStringLiteral("Aircrafts")));
    QVERIFY(before.contains(kRelink));
    QVERIFY(before.contains(kKeepTheDestination));

    ChipNumber(screen, 2)->click();
    Settle();

    const QStringList filtered = OfferedByTheMenuOn(screen, CategoryPath(QStringLiteral("Aircrafts")));
    QVERIFY(!filtered.contains(kRelink));
    QVERIFY(filtered.contains(kKeepTheDestination));
}

QTEST_MAIN(AddonTreePageTest)

#include "tst_addon_tree_page.moc"
