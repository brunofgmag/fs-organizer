#include <QtTest/QtTest>

#include "application/LibraryOrganizer.h"
#include "domain/journal/OperationLog.h"
#include "domain/support/PathSegment.h"
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
        static void ANameLongerThanAFolderNameIsRefusedForItsLengthAndNotItsCharacters();
        static void ANameAlreadyTakenIsRefused();
        static void CreatingCapturesTheEnabledSetAndListsThePreset();
        static void ThePreviewSeparatesWhatMovesFromWhatTheAppNeverLinked();
        static void SettingARowToDisableIsStoredOnThePreset();
        static void ApplyingRefreshesTheSessionAndReportsTheUnresolved();
        static void ALibraryIsNamedByItsLabelAndNotByItsIdentifier();
        static void AWriteTheStoreRefusesIsExplainedInsteadOfPassingInSilence();
        static void ARowCarriesTheContentAndTheDayThePresetWasWritten();
        static void ARowWithoutAWriteDateLeavesTheColumnEmptyInsteadOfInventingOne();
        static void ARowSaysWhetherThePresetIsSatisfiedAndHowMuchItWouldChange();
        static void TheChangeCountFollowsTheModeTheScreenIsShowing();
        static void TheReturnPresetHasItsOwnRowAndIsNotAmongThePresetsOfTheUser();
        static void TheOmittedAddonsAreNamedWithTheirCategory();
        static void NothingIsOmittedOutsideReplace();
        static void AnApplicationTheReturnPresetRefusesIsExplainedAndChangesNothing();
        static void TheUndoOfThisScreenIsTheSameBatchUndoTheLibraryOffers();
        static void WithStartupManagementOffThePreviewCountsWhatWillNotBeApplied();
        static void TurningOnTheStartupFlagOfAPresetIsStored();
        static void ApplyingSaysWhatTheStartupHalfLeftUndoneInsteadOfPassingInSilence();
        static void ASatisfiedPresetStillCountsWhatDisableWouldChange();
        static void GoingBackDoesNotOverwriteTheReturnPreset();
        static void AGoverningPresetRowIsNotSatisfiedWhenTheStartupFileDisagrees();
        static void TheStartupRowsCarryTheLabelFromTheFileAndSettingAnActionIsStored();
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
        Session session{service, organizer, settings, processProbe, runner, notifier};
        FakePresetRepository repository;
        PresetService presets{repository, service, startup.service};
        PresetViewModel viewModel{session, presets, service};
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

void PresetViewModelTest::ANameLongerThanAFolderNameIsRefusedForItsLengthAndNotItsCharacters()
{
    Fixture f;

    f.viewModel.Create(QString(static_cast<int>(kASegmentStopsAt), QLatin1Char('n')));

    QCOMPARE(f.viewModel.Names().size(), 1);

    const QSignalSpy refused(&f.viewModel, &PresetViewModel::Refused);

    f.viewModel.Create(QString(static_cast<int>(kASegmentStopsAt) + 1, QLatin1Char('n')));

    QCOMPARE(refused.count(), 1);
    QCOMPARE(f.viewModel.Names().size(), 1);
    QVERIFY(!refused.front().front().toString().contains(QLatin1Char('*')));
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

    const QList<PresetRow> rows = f.viewModel.Rows(ApplyMode::Replace);

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

    const QList<PresetRow> rows = f.viewModel.Rows(ApplyMode::Replace);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.front().content, QStringLiteral("1 addon · 1 category"));
    QVERIFY(rows.front().updated.isEmpty());
}

void PresetViewModelTest::ARowSaysWhetherThePresetIsSatisfiedAndHowMuchItWouldChange()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();

    f.viewModel.Create("Agora");

    QCOMPARE(f.viewModel.Rows(ApplyMode::Replace).size(), 1);
    QVERIFY(f.viewModel.Rows(ApplyMode::Replace).front().satisfied);
    QCOMPARE(f.viewModel.Rows(ApplyMode::Replace).front().changes, std::size_t{0});

    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    QVERIFY(!f.viewModel.Rows(ApplyMode::Replace).front().satisfied);
    QCOMPARE(f.viewModel.Rows(ApplyMode::Replace).front().changes, std::size_t{1});
}

void PresetViewModelTest::TheChangeCountFollowsTheModeTheScreenIsShowing()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};
    QVERIFY(f.repository.Save(kProfileId, preset));

    QCOMPARE(f.viewModel.Rows(ApplyMode::Replace).front().changes, std::size_t{1});
    QCOMPARE(f.viewModel.Rows(ApplyMode::Cumulative).front().changes, std::size_t{0});
    QVERIFY(!f.viewModel.Rows(ApplyMode::Cumulative).front().satisfied);
}

void PresetViewModelTest::TheReturnPresetHasItsOwnRowAndIsNotAmongThePresetsOfTheUser()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();

    QVERIFY(!f.viewModel.ReturnRow(ApplyMode::Replace).has_value());

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "fenix-a320"},
                                  .action = PresetAction::Enable}};
    QVERIFY(f.repository.Save(kProfileId, preset));

    f.viewModel.Apply(preset, ApplyMode::Replace);

    const std::optional<PresetRow> back = f.viewModel.ReturnRow(ApplyMode::Replace);

    QVERIFY(back.has_value());
    QCOMPARE(back->changes, std::size_t{2});
    QVERIFY(!back->satisfied);
    QCOMPARE(f.viewModel.Names(), QStringList{"Voo curto"});
    QCOMPARE(f.viewModel.Rows(ApplyMode::Replace).size(), 1);
}

void PresetViewModelTest::TheOmittedAddonsAreNamedWithTheirCategory()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};

    const QList<OmittedAddon> omitted = f.viewModel.Omitted(preset, ApplyMode::Replace);

    QCOMPARE(omitted.size(), 1);
    QCOMPARE(omitted.front().name, QStringLiteral("fenix-a320"));
    QCOMPARE(omitted.front().category, QStringLiteral("Aircrafts"));
}

void PresetViewModelTest::NothingIsOmittedOutsideReplace()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};

    QVERIFY(f.viewModel.Omitted(preset, ApplyMode::Cumulative).isEmpty());
    QCOMPARE(f.viewModel.Preview(preset, ApplyMode::Cumulative).notNamedByThePreset, std::size_t{0});
}

void PresetViewModelTest::AnApplicationTheReturnPresetRefusesIsExplainedAndChangesNothing()
{
    Fixture f;
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();
    f.repository.RefuseEveryWrite();

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};

    const QSignalSpy refused(&f.viewModel, &PresetViewModel::Refused);
    const QSignalSpy applied(&f.viewModel, &PresetViewModel::Applied);

    f.viewModel.Apply(preset, ApplyMode::Replace);

    QCOMPARE(refused.count(), 1);
    QCOMPARE(applied.count(), 0);
    QVERIFY(f.session.Snapshot().enabled.Contains(kOtherAddon));
    QVERIFY(!f.session.Snapshot().enabled.Contains(kAddon));
}

void PresetViewModelTest::TheUndoOfThisScreenIsTheSameBatchUndoTheLibraryOffers()
{
    Fixture f;
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    QVERIFY(!f.viewModel.CanUndo());

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};

    f.viewModel.Apply(preset, ApplyMode::Replace);

    QVERIFY(f.viewModel.CanUndo());
    QVERIFY(f.session.Snapshot().enabled.Contains(kAddon));
    QVERIFY(!f.session.Snapshot().enabled.Contains(kOtherAddon));

    f.viewModel.UndoLastBatch();

    QVERIFY(!f.viewModel.CanUndo());
    QVERIFY(!f.session.Snapshot().enabled.Contains(kAddon));
    QVERIFY(f.session.Snapshot().enabled.Contains(kOtherAddon));
}

void PresetViewModelTest::WithStartupManagementOffThePreviewCountsWhatWillNotBeApplied()
{
    Fixture f;
    f.startup.entries.Carry(
        StartupEntry{.label = "Fenix", .path = "D:/MSFS 2024/Aircrafts/fenix-a320/launcher.exe", .enabled = false});
    f.startup.service.Manage(false);
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.startupEntries = {
        PresetStartupEntry{.path = "D:/MSFS 2024/Aircrafts/fenix-a320/launcher.exe", .action = PresetAction::Enable}};
    preset.governsStartup = true;

    const PresetPreview preview = f.viewModel.Preview(preset, ApplyMode::Replace);

    QCOMPARE(preview.startupAsked, std::size_t{1});
    QCOMPARE(preview.notApplied, std::size_t{1});
    QCOMPARE(preview.startupToApply, std::size_t{0});
}

void PresetViewModelTest::TurningOnTheStartupFlagOfAPresetIsStored()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();
    f.viewModel.Create("Voo curto");

    QVERIFY(f.viewModel.GovernStartup("Voo curto", true));

    const std::optional<Preset> saved = f.viewModel.Load("Voo curto");

    QVERIFY(saved.has_value());
    QVERIFY(saved->governsStartup);
    QCOMPARE(saved->entries.size(), std::size_t{1});
}

void PresetViewModelTest::ApplyingSaysWhatTheStartupHalfLeftUndoneInsteadOfPassingInSilence()
{
    Fixture f;
    f.startup.service.Manage(false);

    Preset preset;
    preset.name = "Voo curto";
    preset.startupEntries = {
        PresetStartupEntry{.path = "D:/MSFS 2024/Aircrafts/fenix-a320/launcher.exe", .action = PresetAction::Enable}};
    preset.governsStartup = true;

    const QSignalSpy applied(&f.viewModel, &PresetViewModel::Applied);

    f.viewModel.Apply(preset, ApplyMode::Cumulative);

    QCOMPARE(applied.count(), 1);
    QVERIFY(applied.front().at(0).toStringList().isEmpty());
    QVERIFY(applied.front().at(1).toString().contains(QStringLiteral("startup management is off")));
}

void PresetViewModelTest::ASatisfiedPresetStillCountsWhatDisableWouldChange()
{
    Fixture f;
    f.LinkIn(kAddon);
    f.session.RefreshEntries();

    f.viewModel.Create("Agora");

    const PresetRow asReplace = f.viewModel.Rows(ApplyMode::Replace).front();

    QVERIFY(asReplace.satisfied);
    QCOMPARE(asReplace.changes, std::size_t{0});

    const PresetRow asDisable = f.viewModel.Rows(ApplyMode::Disable).front();

    QVERIFY(asDisable.satisfied);
    QCOMPARE(asDisable.changes, std::size_t{1});
}

void PresetViewModelTest::GoingBackDoesNotOverwriteTheReturnPreset()
{
    Fixture f;
    f.LinkIn(kOtherAddon);
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "aerosoft-crj"},
                                  .action = PresetAction::Enable}};

    f.viewModel.Apply(preset, ApplyMode::Replace);

    const std::optional<Preset> back = f.viewModel.ReturnPreset();
    QVERIFY(back.has_value());
    QCOMPARE(back->entries.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(back->entries.front().addonId.folderName), QStringLiteral("fenix-a320"));

    f.viewModel.ApplyReturn(*back);

    const std::optional<Preset> anchor = f.viewModel.ReturnPreset();
    QVERIFY(anchor.has_value());
    QCOMPARE(anchor->entries.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(anchor->entries.front().addonId.folderName), QStringLiteral("fenix-a320"));

    QVERIFY(f.session.Snapshot().enabled.Contains(kOtherAddon));
    QVERIFY(!f.session.Snapshot().enabled.Contains(kAddon));
}

void PresetViewModelTest::AGoverningPresetRowIsNotSatisfiedWhenTheStartupFileDisagrees()
{
    Fixture f;
    const std::filesystem::path launcher = "D:/MSFS 2024/Aircrafts/fenix-a320/launcher.exe";
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = launcher, .enabled = false});
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.startupEntries = {PresetStartupEntry{.path = launcher, .action = PresetAction::Enable}};
    preset.governsStartup = true;
    QVERIFY(f.repository.Save(kProfileId, preset));

    const QList<PresetRow> rows = f.viewModel.Rows(ApplyMode::Replace);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.front().changes, std::size_t{0});
    QVERIFY(!rows.front().satisfied);
}

void PresetViewModelTest::TheStartupRowsCarryTheLabelFromTheFileAndSettingAnActionIsStored()
{
    Fixture f;
    const std::filesystem::path launcher = "D:/MSFS 2024/Aircrafts/fenix-a320/launcher.exe";
    f.startup.entries.Carry(StartupEntry{.label = "Fenix", .path = launcher, .enabled = true});
    f.session.RefreshEntries();

    Preset preset;
    preset.name = "Voo curto";
    preset.governsStartup = true;
    preset.startupEntries = {PresetStartupEntry{.path = launcher, .action = PresetAction::Enable}};
    QVERIFY(f.repository.Save(kProfileId, preset));

    const QList<PresetStartupRow> rows = f.viewModel.StartupRows(preset);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.front().label, QStringLiteral("Fenix"));
    QVERIFY(rows.front().action == PresetAction::Enable);

    QVERIFY(f.viewModel.SetStartupAction("Voo curto", 0, launcher, PresetAction::Disable));

    const std::optional<Preset> saved = f.viewModel.Load("Voo curto");

    QVERIFY(saved.has_value());
    QVERIFY(saved->startupEntries.front().action == PresetAction::Disable);
}

QTEST_MAIN(PresetViewModelTest)

#include "tst_preset_view_model.moc"
