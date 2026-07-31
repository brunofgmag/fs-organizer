#include <QtCore/QDateTime>
#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QRadioButton>
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
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/PresetsPage.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/PresetViewModel.h"
#include "viewmodel/SessionNotifier.h"

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
};

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
        node.addon = Addon{path, Manifest{}};

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
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }

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

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = kProfileId;

            session.ShowActiveProfile();

            viewModel.Create(QStringLiteral("Voo de linha"));
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
        ProfileService service{catalog, classifier, linking, log, identities, LinkType::Junction};
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        Session session{service, organizer, settings, processProbe, runner, notifier};
        FakePresetRepository presets;
        PresetService presetService{presets, service};
        PresetViewModel viewModel{session, presetService};
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
    QVERIFY(apply->text().contains(QStringLiteral("liga")));
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

    QCOMPARE(heading->mapTo(&page, QPoint()).y(), entries->horizontalHeader()->mapTo(&page, QPoint()).y());
    QCOMPARE(heading->height(), entries->horizontalHeader()->height());
    QCOMPARE(heading->font(), entries->horizontalHeader()->font());
    QTRY_COMPARE(names->viewport()->mapTo(&page, QPoint()).y(), entries->viewport()->mapTo(&page, QPoint()).y());
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
    QCOMPARE(names->columnCount(), 3);
    QCOMPARE(names->rowCount(), 1);

    QCOMPARE(names->horizontalHeaderItem(0)->text(), QStringLiteral("Preset"));
    QCOMPARE(names->horizontalHeaderItem(1)->text(), QStringLiteral("Conteúdo"));
    QCOMPARE(names->horizontalHeaderItem(2)->text(), QStringLiteral("Atualizado"));

    QCOMPARE(names->item(0, 0)->text(), QStringLiteral("Voo de linha"));
    QCOMPARE(names->item(0, 1)->text(), QStringLiteral("1 addon(s) · 1 categoria(s)"));
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

    filter->setText(QStringLiteral("nada casa com isto"));

    QCOMPARE(names->currentRow(), -1);
}

QTEST_MAIN(PresetsPageTest)

#include "tst_presets_page.moc"
