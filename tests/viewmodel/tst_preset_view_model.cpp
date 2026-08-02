#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
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
#include "viewmodel/PresetViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class PresetViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ABlankNameIsRefusedBeforeAnythingIsWritten();
        static void ANameThatCannotBecomeAFileNameIsRefused();
        static void ANameAlreadyTakenIsRefused();
        static void CreatingCapturesTheEnabledSetAndListsThePreset();
        static void ThePreviewSeparatesWhatMovesFromWhatTheAppNeverLinked();
        static void SettingARowToDisableIsStoredOnThePreset();
        static void ApplyingRefreshesTheSessionAndReportsTheUnresolved();
        static void ALibraryIsNamedByItsLabelAndNotByItsIdentifier();
        static void AWriteTheStoreRefusesIsExplainedInsteadOfPassingInSilence();
        static void ARowCarriesTheContentAndTheDayThePresetWasWritten();
        static void ARowWithoutAWriteDateLeavesTheColumnEmptyInsteadOfInventingOne();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kOtherAddon = "D:/MSFS 2024/Aircrafts/fenix-a320";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kLibraryId = "library-1";
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
        aircrafts.children = {AddonNode(kAddon), AddonNode(kOtherAddon)};

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
        profile.libraries = {Library{.id = kLibraryId, .path = kLibrary, .label = "MSFS 2024"}};

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
            fileSystem.AddDirectory(kOtherAddon);
            catalog.SetTree(kLibrary, LibraryTree());

            settings.stored.profiles = {Profile()};
            settings.stored.activeProfileId = kProfileId;

            session.ShowActiveProfile();
        }

        void LinkIn(const std::filesystem::path& addonFolder)
        {
            fileSystem.AddLink(std::filesystem::path(kCommunity) / addonFolder.filename(), addonFolder);
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
        FakePresetRepository repository;
        PresetService presets{repository, service};
        PresetViewModel viewModel{session, presets};
    };
}

void PresetViewModelTest::ABlankNameIsRefusedBeforeAnythingIsWritten()
{
    Fixture f;
    const QSignalSpy refused(&f.viewModel, &PresetViewModel::Refused);

    f.viewModel.Create("   ");

    QCOMPARE(refused.count(), 1);
    QVERIFY(f.viewModel.Names().isEmpty());
}

void PresetViewModelTest::ANameThatCannotBecomeAFileNameIsRefused()
{
    Fixture f;
    const QSignalSpy refused(&f.viewModel, &PresetViewModel::Refused);

    f.viewModel.Create("Voo curto 1/2");

    QCOMPARE(refused.count(), 1);
    QVERIFY(f.viewModel.Names().isEmpty());
}

void PresetViewModelTest::ANameAlreadyTakenIsRefused()
{
    Fixture f;
    f.viewModel.Create("Voo curto");

    const QSignalSpy refused(&f.viewModel, &PresetViewModel::Refused);

    f.viewModel.Create("VOO CURTO");

    QCOMPARE(refused.count(), 1);
    QCOMPARE(f.viewModel.Names().size(), 1);
}

void PresetViewModelTest::CreatingCapturesTheEnabledSetAndListsThePreset()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();

    const QSignalSpy changed(&f.viewModel, &PresetViewModel::Changed);

    f.viewModel.Create("  Voo curto  ");

    QCOMPARE(changed.count(), 1);
    QCOMPARE(f.viewModel.Names(), QStringList{"Voo curto"});

    const std::optional<Preset> saved = f.viewModel.Load("Voo curto");

    QVERIFY(saved.has_value());
    QCOMPARE(saved->entries.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(saved->entries.front().addonId.folderName), QString{"aerosoft-crj"});
}

void PresetViewModelTest::ThePreviewSeparatesWhatMovesFromWhatTheAppNeverLinked()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/some-other-package");
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable},
                      PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aircraft-que-sumiu"},
                                  .action = PresetAction::Enable}};

    const PresetPreview preview = f.viewModel.Preview(preset, ApplyMode::Replace);

    QCOMPARE(preview.toEnable, std::size_t{1});
    QCOMPARE(preview.toDisable, std::size_t{1});
    QCOMPARE(preview.alreadyInPlace, std::size_t{0});
    QCOMPARE(preview.unresolved, std::size_t{1});
    QCOMPARE(preview.leftAlone, std::size_t{1});
    QCOMPARE(preview.notNamedByThePreset, std::size_t{1});
}

void PresetViewModelTest::SettingARowToDisableIsStoredOnThePreset()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();
    f.viewModel.Create("Voo curto");

    QVERIFY(f.viewModel.SetAction("Voo curto", 0, AddonId{kLibraryId, "aerosoft-crj"}, PresetAction::Disable));

    const std::optional<Preset> saved = f.viewModel.Load("Voo curto");

    QVERIFY(saved.has_value());
    QCOMPARE(saved->entries.size(), std::size_t{1});
    QVERIFY(saved->entries.front().action == PresetAction::Disable);
}

void PresetViewModelTest::ApplyingRefreshesTheSessionAndReportsTheUnresolved()
{
    Fixture f;

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable},
                      PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aircraft-que-sumiu"},
                                  .action = PresetAction::Enable}};

    const QSignalSpy applied(&f.viewModel, &PresetViewModel::Applied);

    f.viewModel.Apply(preset, ApplyMode::Cumulative);

    QCOMPARE(applied.count(), 1);
    QCOMPARE(applied.front().front().toStringList(), QStringList{"aircraft-que-sumiu"});
    QVERIFY(f.session.Snapshot().enabled.Contains(kAddon));
}

void PresetViewModelTest::ALibraryIsNamedByItsLabelAndNotByItsIdentifier()
{
    const Fixture f;

    QCOMPARE(f.viewModel.LibraryLabel(kLibraryId), QString{"MSFS 2024"});
    QCOMPARE(f.viewModel.LibraryLabel("library-que-o-perfil-perdeu"), QString{"library-que-o-perfil-perdeu"});
}

void PresetViewModelTest::AWriteTheStoreRefusesIsExplainedInsteadOfPassingInSilence()
{
    Fixture f;
    f.viewModel.Create("Voo curto");

    f.repository.RefuseEveryWrite();

    const QSignalSpy refused(&f.viewModel, &PresetViewModel::Refused);

    f.viewModel.Rename("Voo curto", "Voo longo");

    QCOMPARE(refused.count(), 1);
    QCOMPARE(f.viewModel.Names(), QStringList{"Voo curto"});

    f.viewModel.Create("Treino");

    QCOMPARE(refused.count(), 2);
    QCOMPARE(f.viewModel.Names(), QStringList{"Voo curto"});
}

void PresetViewModelTest::ARowCarriesTheContentAndTheDayThePresetWasWritten()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    f.viewModel.Create("Voo curto");

    const QDateTime carnival(QDate(2026, 2, 17), QTime(9, 30));
    f.repository.SayItWasWrittenAt(
        "Voo curto", std::chrono::system_clock::time_point(std::chrono::milliseconds(carnival.toMSecsSinceEpoch())));

    const QList<PresetRow> rows = f.viewModel.Rows();

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.front().name, QStringLiteral("Voo curto"));
    QCOMPARE(rows.front().content, QStringLiteral("2 addon · 1 category"));
    QCOMPARE(rows.front().updated, QStringLiteral("17/02/2026"));
}

void PresetViewModelTest::ARowWithoutAWriteDateLeavesTheColumnEmptyInsteadOfInventingOne()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();

    f.viewModel.Create("Voo curto");

    const QList<PresetRow> rows = f.viewModel.Rows();

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.front().content, QStringLiteral("1 addon · 1 category"));
    QVERIFY(rows.front().updated.isEmpty());
}

QTEST_MAIN(PresetViewModelTest)

#include "tst_preset_view_model.moc"
