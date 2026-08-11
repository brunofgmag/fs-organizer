#include <QtTest/QtTest>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

#include "application/LegacyConfigImporter.h"
#include "application/LibraryOrganizer.h"
#include "application/PresetService.h"
#include "application/Session.h"
#include "domain/journal/OperationLog.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLegacyConfigSource.h"
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
#include "tests/doubles/RecordingSessionObserver.h"
#include "view/legacy/LegacyImportDialog.h"

namespace
{
    class LegacyImportDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EachInstallationBecomesALineWithItsLibrariesUnderIt();
        static void ALibraryAlreadyRegisteredIsShownWithoutAnythingToCheck();
        static void ALibraryWhoseRootIsGoneIsListedAndOffersNothing();
        static void AConfigurationThatCouldNotBeReadSaysSoInsteadOfDisappearing();
        static void ImportingWhatIsCheckedRegistersTheLibraryAndSaysWhatHappened();
        static void APresetOfTheOldProgramIsOfferedAndLandsInTheProfile();
        static void ANameThePresetCitesAndNoLibraryHasIsReportedInsteadOfVanishing();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kAddon = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kLegacy2024 = "C:/ProgramData/MSFS Addons Linker 2024";
    constexpr auto kLegacy2020 = "C:/ProgramData/MSFS Addons Linker";
    constexpr auto kOtherLibrary = "D:/MSFS 2024 Extra";

    TreeNode LibraryTree()
    {
        TreeNode addon;
        addon.kind = TreeNodeKind::Addon;
        addon.path = kAddon;
        addon.addon = Addon{.folderPath = kAddon, .manifest = Manifest{}};

        TreeNode aircrafts;
        aircrafts.kind = TreeNodeKind::Category;
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {std::move(addon)};

        TreeNode root;
        root.kind = TreeNodeKind::Library;
        root.path = kLibrary;
        root.children = {std::move(aircrafts)};

        return root;
    }

    LegacyInstallation InstallationAt(const std::filesystem::path& folder,
                                      const std::vector<std::filesystem::path>& addonPaths)
    {
        LegacyInstallation installation;
        installation.folder = folder;
        installation.addonPaths = addonPaths;

        return installation;
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

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);
            fileSystem.AddDirectory(kLibrary);
            fileSystem.AddDirectory(kAddon);
            fileSystem.AddDirectory(kOtherLibrary);
            catalog.SetTree(kLibrary, LibraryTree());
            catalog.SetTree(kOtherLibrary, TreeNode{});

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
        RecordingSessionObserver observer;
        Session session{service, organizer, settings, settings.stored, processProbe, runner, observer};
        FakeLegacyConfigSource legacy;
        LegacyConfigImporter importer{legacy, filesystemProbe};
        FakePresetRepository presetRepository;
        PresetService presets{presetRepository, service, startup.service};
        LegacyImportViewModel viewModel{session, importer, presets};
    };

    QTreeWidget& TreeOf(const LegacyImportDialog& dialog)
    {
        return *dialog.findChild<QTreeWidget*>(QStringLiteral("LegacyProposal"));
    }

    QPushButton* ButtonLabelled(const QWidget& widget, const QString& text)
    {
        const auto buttons = widget.findChildren<QPushButton*>();
        const auto found = std::ranges::find_if(buttons,
                                                [&text](const QPushButton* button)
                                                {
                                                    return button->text() == text;
                                                });

        return found == buttons.end() ? nullptr : *found;
    }
}

void LegacyImportDialogTest::EachInstallationBecomesALineWithItsLibrariesUnderIt()
{
    Fixture f;
    f.legacy.Add(InstallationAt(kLegacy2024, {"D:/MSFS 2024 Extra/Aircrafts", "D:/MSFS 2024 Extra/Sceneries"}));
    f.legacy.Add(InstallationAt(kLegacy2020, {"D:/MSFS 2020/Aircrafts", "D:/MSFS 2020/Sceneries"}));

    const LegacyImportDialog dialog(f.viewModel);
    const QTreeWidget& tree = TreeOf(dialog);

    QCOMPARE(tree.topLevelItemCount(), 2);
    QCOMPARE(tree.topLevelItem(0)->childCount(), 1);
    QCOMPARE(tree.topLevelItem(0)->child(0)->childCount(), 2);
}

void LegacyImportDialogTest::ALibraryAlreadyRegisteredIsShownWithoutAnythingToCheck()
{
    Fixture f;
    f.legacy.Add(InstallationAt(kLegacy2024, {"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}));

    const LegacyImportDialog dialog(f.viewModel);
    const QTreeWidgetItem* library = TreeOf(dialog).topLevelItem(0)->child(0);

    QVERIFY(!library->flags().testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(!library->text(1).isEmpty());
}

void LegacyImportDialogTest::ALibraryWhoseRootIsGoneIsListedAndOffersNothing()
{
    Fixture f;
    f.legacy.Add(InstallationAt(kLegacy2020, {"D:/MSFS 2020/Aircrafts", "D:/MSFS 2020/Sceneries"}));

    const LegacyImportDialog dialog(f.viewModel);
    const QTreeWidgetItem* library = TreeOf(dialog).topLevelItem(0)->child(0);

    QVERIFY(!library->flags().testFlag(Qt::ItemIsUserCheckable));
    QCOMPARE(library->childCount(), 2);
    QVERIFY(!library->child(0)->flags().testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(!ButtonLabelled(dialog, QStringLiteral("Import"))->isEnabled());
}

void LegacyImportDialogTest::AConfigurationThatCouldNotBeReadSaysSoInsteadOfDisappearing()
{
    Fixture f;
    f.legacy.AddWithUnreadableConfiguration(kLegacy2024);

    const LegacyImportDialog dialog(f.viewModel);
    const QTreeWidgetItem* installation = TreeOf(dialog).topLevelItem(0);

    QCOMPARE(installation->childCount(), 0);
    QVERIFY(!installation->text(1).isEmpty());
}

void LegacyImportDialogTest::ImportingWhatIsCheckedRegistersTheLibraryAndSaysWhatHappened()
{
    Fixture f;
    f.legacy.Add(InstallationAt(kLegacy2024, {"D:/MSFS 2024 Extra/Aircrafts", "D:/MSFS 2024 Extra/Sceneries"}));

    LegacyImportDialog dialog(f.viewModel);
    const QSignalSpy said(&dialog, &LegacyImportDialog::StatusChanged);

    QVERIFY(ButtonLabelled(dialog, QStringLiteral("Import"))->isEnabled());
    ButtonLabelled(dialog, QStringLiteral("Import"))->click();

    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(said.count(), 1);
    QCOMPARE(f.session.Profile().libraries.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles.front().libraries.size(), std::size_t{2});
}

void LegacyImportDialogTest::APresetOfTheOldProgramIsOfferedAndLandsInTheProfile()
{
    Fixture f;
    LegacyInstallation installation = InstallationAt(kLegacy2024, {"D:/MSFS 2024/Aircrafts"});
    installation.presetsPath = "C:/ProgramData/MSFS Addons Linker 2024/Presets";
    f.legacy.Add(installation);

    LegacyPresetSelection selection;
    selection.name = "Voo curto";
    selection.enabledAddonNames = {"pmdg-aircraft-77w"};
    f.legacy.PlacePreset("C:/ProgramData/MSFS Addons Linker 2024/Presets", selection);

    LegacyImportDialog dialog(f.viewModel);

    ButtonLabelled(dialog, QStringLiteral("Import"))->click();

    QCOMPARE(f.presets.List("msfs2024").size(), std::size_t{1});
    QCOMPARE(f.presets.Load("msfs2024", "Voo curto")->entries.size(), std::size_t{1});
}

void LegacyImportDialogTest::ANameThePresetCitesAndNoLibraryHasIsReportedInsteadOfVanishing()
{
    Fixture f;
    LegacyInstallation installation = InstallationAt(kLegacy2024, {"D:/MSFS 2024/Aircrafts"});
    installation.presetsPath = "C:/ProgramData/MSFS Addons Linker 2024/Presets";
    f.legacy.Add(installation);

    LegacyPresetSelection selection;
    selection.name = "Voo curto";
    selection.enabledAddonNames = {"pmdg-aircraft-77w", "an-addon-that-is-nowhere"};
    f.legacy.PlacePreset("C:/ProgramData/MSFS Addons Linker 2024/Presets", selection);

    LegacyImportDialog dialog(f.viewModel);
    const QSignalSpy said(&dialog, &LegacyImportDialog::StatusChanged);

    ButtonLabelled(dialog, QStringLiteral("Import"))->click();

    QCOMPARE(said.count(), 1);
    QVERIFY(said.front().front().toString().contains(QStringLiteral("was not found in any library")));
}

QTEST_MAIN(LegacyImportDialogTest)

#include "tst_legacy_import_dialog.moc"
