#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableWidget>

#include "application/LibraryOrganizer.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/PresetsPage.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/PresetViewModel.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class PresetsPageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BuildingAndTearingDownAloneDoesNotCrash();
        static void SelectingAPresetFillsThePanelPreview();
        static void ApplyingFromThePanelGoesThroughTheViewModel();
        static void TheFirstPresetStartsBelowTheTableHeaderAndNotInsideIt();
        static void TheNameTableWritesTheContentAndTheDayBesideEachPreset();
        static void FilteringHidesTheNamesThatDoNotMatchAndKeepsASelectionThatSurvives();
        static void FilteringPastTheSelectedPresetMovesTheSelectionInsteadOfStranding();
        static void ALanguageChangeReachesTheApplyButtonAndTheModeExplanation();
        static void WhatSupportsTheNameIsQuietInBothTables();
        static void TheNameTableSaysWhatEachPresetWouldChangeAndTagsTheSatisfiedOnes();
        static void TheReturnPresetSitsInItsOwnTableAndAppearsOnlyAfterAnApplication();
        static void ThePanelBreaksThePlanIntoTheSixCountsTheGlossaryFixes();
        static void TheOmittedCountAndItsButtonLeaveThePanelOutsideReplace();
        static void TheOmittedAddonsAreListedOnlyWhenAsked();
        static void TheStartupSectionStaysHiddenUntilThePresetGovernsStartup();
        static void TheWayBackIsTheBatchUndoAndFallsBackToTheReturnPreset();
        static void TheTwoHalvesSwapWhatTheRightSideShows();
        static void TheContentTabCountsTheEntriesOfTheSelectedPreset();
        static void ASatisfiedPresetStillShowsWhatDisableWouldChange();
        static void AFilterThatMatchesNothingLeavesNoStaleCountBehind();
        static void ChoosingTheReturnPresetSticksAndItsEntriesAreNotEditable();
        static void TheStartupTabEditsTheStartupEntriesOfAGoverningPreset();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kProfileId = "msfs2024";

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
        aircrafts.children = {AddonNode(kAddon)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    class MarkingTranslator final : public QTranslator
    {
    public:
        [[nodiscard]] bool isEmpty() const override
        {
            return false;
        }

        [[nodiscard]] QString translate(const char*, const char* source, const char*, int) const override
        {
            return QStringLiteral("<%1>").arg(QString::fromUtf8(source));
        }
    };

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAircrafts);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddLink(std::filesystem::path(kCommunity) / "aerosoft-crj", kAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            session.ShowActiveProfile();

            viewModel.Create(QStringLiteral("Voo de linha"));
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
        FakePresetRepository presets;
        PresetService presetService{presets, service, startup.service};
        PresetViewModel viewModel{session, presetService, service};
    };
}

void PresetsPageTest::BuildingAndTearingDownAloneDoesNotCrash()
{
    Fixture f;
    {
        PresetsPage page(f.viewModel, f.notifier);
    }
}

void PresetsPageTest::SelectingAPresetFillsThePanelPreview()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->rowCount(), 1);
    QCOMPARE(names->currentRow(), 0);

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    QVERIFY(apply != nullptr);
    QVERIFY(apply->isEnabled());
    QVERIFY(apply->text().contains(QStringLiteral("enables")));
}

void PresetsPageTest::ApplyingFromThePanelGoesThroughTheViewModel()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* cumulative = page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"));
    QVERIFY(cumulative != nullptr);
    cumulative->click();

    const QSignalSpy applied(&f.viewModel, &PresetViewModel::Applied);

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    apply->click();

    QCOMPARE(applied.count(), 1);
}

void PresetsPageTest::TheFirstPresetStartsBelowTheTableHeaderAndNotInsideIt()
{
    ApplyModernistTheme(*qApp);

    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);
    page.resize(1200, 600);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));

    QVERIFY(names != nullptr);
    QVERIFY(entries != nullptr);

    QHeaderView* heading = names->horizontalHeader();

    QCOMPARE(heading->height(), entries->horizontalHeader()->height());
    QCOMPARE(heading->font(), entries->horizontalHeader()->font());

    for (const QTableWidget* table : {names, entries})
    {
        const QHeaderView* header = table->horizontalHeader();

        QTRY_VERIFY2(
            table->viewport()->mapTo(&page, QPoint()).y() >= header->mapTo(&page, QPoint()).y() + header->height(),
            qPrintable(QStringLiteral("%1 draws its first row inside its own header").arg(table->objectName())));
    }
}

void PresetsPageTest::TheTwoHalvesSwapWhatTheRightSideShows()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* content = page.findChild<QPushButton*>(QStringLiteral("PresetContentTab"));
    auto* plan = page.findChild<QPushButton*>(QStringLiteral("PresetPlanTab"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));
    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    QVERIFY(content != nullptr && plan != nullptr && entries != nullptr && apply != nullptr);

    QVERIFY(content->isChecked());
    QVERIFY(!plan->isChecked());

    auto* shown = qobject_cast<QStackedWidget*>(entries->parentWidget());
    QVERIFY(shown != nullptr);
    QCOMPARE(shown->currentWidget(), entries);

    plan->click();

    QVERIFY(plan->isChecked());
    QVERIFY(!content->isChecked());
    QVERIFY(shown->currentWidget() != entries);
    QVERIFY(shown->currentWidget()->isAncestorOf(apply));
}

void PresetsPageTest::TheContentTabCountsTheEntriesOfTheSelectedPreset()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* content = page.findChild<QPushButton*>(QStringLiteral("PresetContentTab"));
    auto* planFor = page.findChild<QLabel*>(QStringLiteral("PresetPlanFor"));
    QVERIFY(content != nullptr && planFor != nullptr);

    QCOMPARE(content->text(), QStringLiteral("Content · 1"));
    QCOMPARE(planFor->text(), QStringLiteral("Voo de linha"));
}

void PresetsPageTest::TheNameTableWritesTheContentAndTheDayBesideEachPreset()
{
    Fixture f;
    f.presets.SayItWasWrittenAt("Voo de linha",
                                std::chrono::system_clock::time_point(std::chrono::milliseconds(
                                    QDateTime(QDate(2026, 2, 17), QTime(9, 30)).toMSecsSinceEpoch())));

    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->columnCount(), 4);
    QCOMPARE(names->rowCount(), 1);

    QCOMPARE(names->horizontalHeaderItem(0)->text(), QStringLiteral("Preset"));
    QCOMPARE(names->horizontalHeaderItem(1)->text(), QStringLiteral("Content"));
    QCOMPARE(names->horizontalHeaderItem(2)->text(), QStringLiteral("Changed"));

    QCOMPARE(names->item(0, 0)->text(), QStringLiteral("Voo de linha"));
    QCOMPARE(names->item(0, 1)->text(), QStringLiteral("1 addon · 1 category"));
    QCOMPARE(names->item(0, 2)->text(), QStringLiteral("17/02/2026"));
}

void PresetsPageTest::FilteringHidesTheNamesThatDoNotMatchAndKeepsASelectionThatSurvives()
{
    Fixture f;
    f.viewModel.Create(QStringLiteral("Bush flying"));

    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* filter = page.findChild<QLineEdit*>();
    QVERIFY(names != nullptr);
    QVERIFY(filter != nullptr);
    QCOMPARE(names->rowCount(), 2);

    const auto rowNamed = [names](const QString& name)
    {
        for (int row = 0; row < names->rowCount(); ++row)
        {
            if (names->item(row, 0)->text() == name)
            {
                return row;
            }
        }

        return -1;
    };

    const int bush = rowNamed(QStringLiteral("Bush flying"));
    const int line = rowNamed(QStringLiteral("Voo de linha"));

    names->setCurrentCell(bush, 0);

    filter->setText(QStringLiteral("bush"));

    QVERIFY(!names->isRowHidden(bush));
    QVERIFY(names->isRowHidden(line));
    QCOMPARE(names->currentRow(), bush);

    filter->clear();

    QVERIFY(!names->isRowHidden(bush));
    QVERIFY(!names->isRowHidden(line));
}

void PresetsPageTest::FilteringPastTheSelectedPresetMovesTheSelectionInsteadOfStranding()
{
    Fixture f;
    f.viewModel.Create(QStringLiteral("Bush flying"));

    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* filter = page.findChild<QLineEdit*>();

    const auto rowNamed = [names](const QString& name)
    {
        for (int row = 0; row < names->rowCount(); ++row)
        {
            if (names->item(row, 0)->text() == name)
            {
                return row;
            }
        }

        return -1;
    };

    names->setCurrentCell(rowNamed(QStringLiteral("Bush flying")), 0);

    filter->setText(QStringLiteral("linha"));

    QCOMPARE(names->currentRow(), rowNamed(QStringLiteral("Voo de linha")));
    QVERIFY(!names->isRowHidden(names->currentRow()));

    filter->setText(QStringLiteral("nothing matches this"));

    QCOMPARE(names->currentRow(), -1);
}

void PresetsPageTest::ALanguageChangeReachesTheApplyButtonAndTheModeExplanation()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    names->setCurrentCell(0, 0);

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    auto* explained = page.findChild<QLabel*>(QStringLiteral("ModeExplained"));
    QVERIFY(apply != nullptr && explained != nullptr);

    const QString applyBefore = apply->text();
    const QString explainedBefore = explained->text();
    QVERIFY(!applyBefore.isEmpty());
    QVERIFY(!explainedBefore.isEmpty());

    MarkingTranslator marking;
    QCoreApplication::installTranslator(&marking);
    QCoreApplication::processEvents();

    QVERIFY2(apply->text() != applyBefore, "the apply button kept its old text after the language change");
    QVERIFY2(explained->text() != explainedBefore, "the mode explanation kept its old text after the language change");

    QCoreApplication::removeTranslator(&marking);
    QCoreApplication::processEvents();

    QCOMPARE(apply->text(), applyBefore);
    QCOMPARE(explained->text(), explainedBefore);
}

void PresetsPageTest::WhatSupportsTheNameIsQuietInBothTables()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));
    QVERIFY(names != nullptr);
    QVERIFY(entries != nullptr);
    QCOMPARE(names->rowCount(), 1);
    QCOMPARE(entries->rowCount(), 1);

    QVERIFY(!names->item(0, 0)->data(QuietRole).toBool());
    QVERIFY(names->item(0, 1)->data(QuietRole).toBool());
    QVERIFY(names->item(0, 2)->data(QuietRole).toBool());

    QVERIFY(!entries->item(0, 0)->data(QuietRole).toBool());
    QVERIFY(entries->item(0, 1)->data(QuietRole).toBool());
}

void PresetsPageTest::TheNameTableSaysWhatEachPresetWouldChangeAndTagsTheSatisfiedOnes()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->columnCount(), 4);
    QCOMPARE(names->horizontalHeaderItem(3)->text(), QStringLiteral("Would change"));

    QCOMPARE(names->item(0, 3)->text(), QStringLiteral("0 change"));
    QCOMPARE(names->item(0, 3)->data(TagTextRole).toString(), QStringLiteral("Satisfied"));

    f.fileSystem.RemoveNode(std::filesystem::path(kCommunity) / "aerosoft-crj");
    f.session.RefreshEntries();

    QCOMPARE(names->item(0, 3)->text(), QStringLiteral("1 change"));
    QVERIFY(names->item(0, 3)->data(TagTextRole).toString().isEmpty());
    QVERIFY(!names->item(0, 0)->text().isEmpty());
}

void PresetsPageTest::TheReturnPresetSitsInItsOwnTableAndAppearsOnlyAfterAnApplication()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* back = page.findChild<QTableWidget*>(QStringLiteral("PresetReturn"));
    QVERIFY(back != nullptr);
    QVERIFY(back->isHidden());

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();

    auto* apply = page.findChild<QPushButton*>(QStringLiteral("PresetApply"));
    QVERIFY(apply != nullptr);
    apply->click();

    QVERIFY(!back->isHidden());
    QCOMPARE(back->rowCount(), 1);
    QCOMPARE(back->item(0, 0)->text(), QStringLiteral("Back to the previous set"));

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QCOMPARE(names->rowCount(), 1);
    QCOMPARE(names->item(0, 0)->text(), QStringLiteral("Voo de linha"));
}

void PresetsPageTest::ThePanelBreaksThePlanIntoTheSixCountsTheGlossaryFixes()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    const auto valueOf = [&page](const QString& name)
    {
        auto* label = page.findChild<QLabel*>(name);

        return label == nullptr ? QString{} : label->text();
    };

    QCOMPARE(valueOf(QStringLiteral("PlanToEnable")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanToDisable")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanAlreadyInPlace")), QStringLiteral("1"));
    QCOMPARE(valueOf(QStringLiteral("PlanUnresolved")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanNotNamed")), QStringLiteral("0"));
    QCOMPARE(valueOf(QStringLiteral("PlanNotApplied")), QStringLiteral("0"));
}

void PresetsPageTest::TheOmittedCountAndItsButtonLeaveThePanelOutsideReplace()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* omitted = page.findChild<QLabel*>(QStringLiteral("PlanNotNamed"));
    auto* show = page.findChild<QPushButton*>(QStringLiteral("PresetShowOmitted"));
    QVERIFY(omitted != nullptr && show != nullptr);
    QVERIFY(!omitted->isHidden());

    auto* cumulative = page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"));
    QVERIFY(cumulative != nullptr);
    cumulative->click();

    QVERIFY(omitted->isHidden());
    QVERIFY(show->isHidden());
}

void PresetsPageTest::TheOmittedAddonsAreListedOnlyWhenAsked()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/fenix-a320");
    f.fileSystem.AddLink(std::filesystem::path(kCommunity) / "fenix-a320", "D:/MSFS 2024/Aircrafts/fenix-a320");

    TreeNode aircrafts;
    aircrafts.kind = TreeNodeKind::Category;
    aircrafts.path = kAircrafts;
    aircrafts.children = {AddonNode(kAddon), AddonNode("D:/MSFS 2024/Aircrafts/fenix-a320")};

    TreeNode library;
    library.kind = TreeNodeKind::Library;
    library.path = kLibrary;
    library.children = {std::move(aircrafts)};

    f.catalog.SetTree(kLibrary, library);
    f.session.ShowActiveProfile();

    PresetsPage page(f.viewModel, f.notifier);

    auto* show = page.findChild<QPushButton*>(QStringLiteral("PresetShowOmitted"));
    auto* omitted = page.findChild<QLabel*>(QStringLiteral("PlanNotNamed"));
    QVERIFY(show != nullptr && omitted != nullptr);

    QCOMPARE(omitted->text(), QStringLiteral("1"));
    QVERIFY(show->isEnabled());

    QCOMPARE(f.viewModel.Omitted(*f.viewModel.Load(QStringLiteral("Voo de linha")), ApplyMode::Replace).size(), 1);
}

void PresetsPageTest::TheStartupSectionStaysHiddenUntilThePresetGovernsStartup()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* governs = page.findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup"));
    auto* section = page.findChild<QWidget*>(QStringLiteral("PresetStartupSection"));
    QVERIFY(governs != nullptr && section != nullptr);

    QVERIFY(!governs->isChecked());
    QVERIFY(section->isHidden());

    governs->click();

    QVERIFY(section->isHidden() == false);

    const std::optional<Preset> saved = f.viewModel.Load(QStringLiteral("Voo de linha"));
    QVERIFY(saved.has_value());
    QVERIFY(saved->governsStartup);
}

void PresetsPageTest::TheWayBackIsTheBatchUndoAndFallsBackToTheReturnPreset()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* back = page.findChild<QPushButton*>(QStringLiteral("PresetGoBack"));
    QVERIFY(back != nullptr);
    QVERIFY(!back->isEnabled());

    f.fileSystem.RemoveNode(std::filesystem::path(kCommunity) / "aerosoft-crj");
    f.session.RefreshEntries();

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();
    page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->click();

    QVERIFY(back->isEnabled());
    QCOMPARE(back->text(), QStringLiteral("Back to the previous set"));
    QVERIFY(back->toolTip().contains(QStringLiteral("batch")));

    f.service.ForgetUndo();
    f.session.RefreshEntries();

    QVERIFY(back->isEnabled());
    QCOMPARE(back->text(), QStringLiteral("Back to the previous set"));
    QVERIFY(back->toolTip().contains(QStringLiteral("return preset")));
}

void PresetsPageTest::ASatisfiedPresetStillShowsWhatDisableWouldChange()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    QVERIFY(names != nullptr);
    QCOMPARE(names->item(0, 3)->data(TagTextRole).toString(), QStringLiteral("Satisfied"));
    QCOMPARE(names->item(0, 3)->text(), QStringLiteral("0 change"));

    page.findChild<QRadioButton*>(QStringLiteral("ModeDisable"))->click();

    QCOMPARE(names->item(0, 3)->data(TagTextRole).toString(), QStringLiteral("Satisfied"));
    QCOMPARE(names->item(0, 3)->text(), QStringLiteral("1 change"));
}

void PresetsPageTest::AFilterThatMatchesNothingLeavesNoStaleCountBehind()
{
    Fixture f;
    PresetsPage page(f.viewModel, f.notifier);

    auto* already = page.findChild<QLabel*>(QStringLiteral("PlanAlreadyInPlace"));
    auto* filter = page.findChild<QLineEdit*>();
    QVERIFY(already != nullptr && filter != nullptr);

    QCOMPARE(already->text(), QStringLiteral("1"));

    filter->setText(QStringLiteral("nada casa com isto"));

    QCOMPARE(already->text(), QStringLiteral("0"));
    QCOMPARE(page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->text(), QStringLiteral("Apply"));
}

void PresetsPageTest::ChoosingTheReturnPresetSticksAndItsEntriesAreNotEditable()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/MSFS 2024/Aircrafts/fenix-a320");
    PresetsPage page(f.viewModel, f.notifier);

    page.findChild<QRadioButton*>(QStringLiteral("ModeCumulative"))->click();
    page.findChild<QPushButton*>(QStringLiteral("PresetApply"))->click();

    auto* back = page.findChild<QTableWidget*>(QStringLiteral("PresetReturn"));
    auto* names = page.findChild<QTableWidget*>(QStringLiteral("PresetNames"));
    auto* planFor = page.findChild<QLabel*>(QStringLiteral("PresetPlanFor"));
    auto* entries = page.findChild<QTableWidget*>(QStringLiteral("PresetEntries"));
    QVERIFY(back != nullptr && names != nullptr && planFor != nullptr && entries != nullptr);

    back->setCurrentCell(0, 0);

    QCOMPARE(planFor->text(), QStringLiteral("Back to the previous set"));
    QCOMPARE(names->currentRow(), -1);
    QCOMPARE(back->currentRow(), 0);
    QVERIFY(!entries->item(0, 2)->flags().testFlag(Qt::ItemIsUserCheckable));

    f.session.RefreshEntries();

    QCOMPARE(planFor->text(), QStringLiteral("Back to the previous set"));
    QCOMPARE(back->currentRow(), 0);
    QCOMPARE(names->currentRow(), -1);

    names->setCurrentCell(0, 0);

    QCOMPARE(planFor->text(), QStringLiteral("Voo de linha"));
    QCOMPARE(back->currentRow(), -1);
    QVERIFY(entries->item(0, 2)->flags().testFlag(Qt::ItemIsUserCheckable));
}

void PresetsPageTest::TheStartupTabEditsTheStartupEntriesOfAGoverningPreset()
{
    Fixture f;
    const std::filesystem::path launcher = "D:/MSFS 2024/Aircrafts/aerosoft-crj/launcher.exe";
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = launcher, .enabled = true});
    f.session.RefreshEntries();

    PresetsPage page(f.viewModel, f.notifier);

    auto* governs = page.findChild<QCheckBox*>(QStringLiteral("PresetGovernsStartup"));
    auto* startupEntries = page.findChild<QTableWidget*>(QStringLiteral("PresetStartupEntries"));
    QVERIFY(governs != nullptr && startupEntries != nullptr);

    QVERIFY(!governs->isChecked());
    QCOMPARE(startupEntries->rowCount(), 0);

    governs->click();

    QVERIFY(governs->isChecked());
    QCOMPARE(startupEntries->rowCount(), 1);
    QCOMPARE(startupEntries->item(0, 0)->text(), QStringLiteral("Fenix"));
    QCOMPARE(startupEntries->item(0, 2)->checkState(), Qt::Checked);
    QVERIFY(startupEntries->item(0, 2)->flags().testFlag(Qt::ItemIsUserCheckable));

    startupEntries->item(0, 2)->setCheckState(Qt::Unchecked);

    const std::optional<Preset> saved = f.viewModel.Load(QStringLiteral("Voo de linha"));

    QVERIFY(saved.has_value());
    QCOMPARE(saved->startupEntries.size(), std::size_t{1});
    QVERIFY(saved->startupEntries.front().action == PresetAction::Disable);
}

QTEST_MAIN(PresetsPageTest)

#include "tst_presets_page.moc"
