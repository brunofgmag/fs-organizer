#include <QtTest/QtTest>

#include "application/LegacyConfigImporter.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLegacyConfigSource.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class LegacyConfigImporterTest : public QObject
{
    Q_OBJECT

private slots:
    static void EveryInstallationTheSourceFoundIsProposed();
    static void ALibraryWhoseRootIsOnDiskIsAvailable();
    static void ALibraryWhoseRootIsGoneStaysListedAndIsMarkedUnavailable();
    static void AnInstallationWithoutEntriesIsStillListed();
    static void AConfigurationThatCouldNotBeReadIsNotTheSameAsOneWithoutEntries();
    static void TheProposalIsReconciledAgainstTheProfileInUse();
    static void TheCategoriesAreReconciledAgainstTheTreeThatWasScanned();
    static void ThePresetsFolderTravelsWithTheProposal();
    static void ThePresetsOfTheInstallationAreResolvedAgainstWhatWasScanned();
};

namespace
{
    LegacyInstallation InstallationAt(const std::filesystem::path& folder,
                                      const std::vector<std::filesystem::path>& addonPaths)
    {
        LegacyInstallation installation;
        installation.folder = folder;
        installation.addonPaths = addonPaths;

        return installation;
    }

    SimulatorProfile ProfileHolding(const std::vector<std::filesystem::path>& libraryPaths)
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";

        for (const std::filesystem::path& path : libraryPaths)
        {
            profile.libraries.push_back(Library{"library-1", path, "MSFS 2024"});
        }

        return profile;
    }

    std::vector<TreeNode> LibraryScannedAt(const std::filesystem::path& root,
                                           const std::vector<std::filesystem::path>& categories)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = root;

        for (const std::filesystem::path& category : categories)
        {
            TreeNode addon;
            addon.kind = TreeNodeKind::Addon;
            addon.path = category / "an-addon";

            TreeNode node;
            node.kind = TreeNodeKind::Category;
            node.path = category;
            node.children.push_back(std::move(addon));

            library.children.push_back(std::move(node));
        }

        return {library};
    }

    std::vector<TreeNode> LibraryHolding(const std::filesystem::path& root,
                                         const std::vector<std::filesystem::path>& addons)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = root;

        for (const std::filesystem::path& addon : addons)
        {
            TreeNode node;
            node.kind = TreeNodeKind::Addon;
            node.path = addon;
            node.addon = Addon{addon, Manifest{}};
            library.children.push_back(node);
        }

        return {library};
    }
}

void LegacyConfigImporterTest::EveryInstallationTheSourceFoundIsProposed()
{
    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.Add(InstallationAt("C:/ProgramData/MSFS Addons Linker 2024", {"D:/MSFS 2024/Aircrafts"}));
    source.Add(InstallationAt("C:/ProgramData/MSFS Addons Linker", {"D:/MSFS 2020/Aircrafts"}));

    const std::vector<LegacyMigration> proposed = LegacyConfigImporter(source, probe).Propose({}, {});

    QCOMPARE(proposed.size(), std::size_t{2});
    QCOMPARE(ComparablePath(proposed[0].folder), ComparablePath("C:/ProgramData/MSFS Addons Linker 2024"));
    QCOMPARE(ComparablePath(proposed[1].folder), ComparablePath("C:/ProgramData/MSFS Addons Linker"));
}

void LegacyConfigImporterTest::ALibraryWhoseRootIsOnDiskIsAvailable()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/MSFS 2024");
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.Add(
        InstallationAt("C:/ProgramData/MSFS Addons Linker 2024", {"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}));

    const std::vector<LegacyMigration> proposed = LegacyConfigImporter(source, probe).Propose({}, {});

    QCOMPARE(proposed.front().libraries.size(), std::size_t{1});
    QVERIFY(proposed.front().libraries.front().rootExists);
    QCOMPARE(proposed.front().libraries.front().proposal.categories.size(), std::size_t{2});
}

void LegacyConfigImporterTest::ALibraryWhoseRootIsGoneStaysListedAndIsMarkedUnavailable()
{
    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.Add(
        InstallationAt("C:/ProgramData/MSFS Addons Linker", {"D:/MSFS 2020/Aircrafts", "D:/MSFS 2020/Sceneries"}));

    const std::vector<LegacyMigration> proposed = LegacyConfigImporter(source, probe).Propose({}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(proposed.front().libraries.size(), std::size_t{1});
    QVERIFY(!proposed.front().libraries.front().rootExists);
    QCOMPARE(proposed.front().libraries.front().proposal.categories.size(), std::size_t{2});
}

void LegacyConfigImporterTest::AnInstallationWithoutEntriesIsStillListed()
{
    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.Add(InstallationAt("C:/ProgramData/MSFS Addons Linker", {}));

    const std::vector<LegacyMigration> proposed = LegacyConfigImporter(source, probe).Propose({}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QVERIFY(proposed.front().libraries.empty());
    QVERIFY(proposed.front().configurationWasRead);
}

void LegacyConfigImporterTest::AConfigurationThatCouldNotBeReadIsNotTheSameAsOneWithoutEntries()
{
    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.AddWithUnreadableConfiguration("C:/ProgramData/MSFS Addons Linker");

    const std::vector<LegacyMigration> proposed = LegacyConfigImporter(source, probe).Propose({}, {});

    QCOMPARE(proposed.size(), std::size_t{1});
    QCOMPARE(ComparablePath(proposed.front().folder), ComparablePath("C:/ProgramData/MSFS Addons Linker"));
    QVERIFY(!proposed.front().configurationWasRead);
    QVERIFY(proposed.front().libraries.empty());
}

void LegacyConfigImporterTest::TheProposalIsReconciledAgainstTheProfileInUse()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/MSFS 2024");
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.Add(
        InstallationAt("C:/ProgramData/MSFS Addons Linker 2024", {"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}));

    const std::vector<LegacyMigration> proposed =
        LegacyConfigImporter(source, probe).Propose(ProfileHolding({"D:/MSFS 2024"}), {});

    QCOMPARE(proposed.front().libraries.front().proposal.state, ProposedState::AlreadyPresent);
}

void LegacyConfigImporterTest::TheCategoriesAreReconciledAgainstTheTreeThatWasScanned()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/MSFS 2024");
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    source.Add(
        InstallationAt("C:/ProgramData/MSFS Addons Linker 2024", {"D:/MSFS 2024/Aircrafts", "D:/MSFS 2024/Sceneries"}));

    const std::vector<LegacyMigration> proposed =
        LegacyConfigImporter(source, probe)
            .Propose(ProfileHolding({"D:/MSFS 2024"}), LibraryScannedAt("D:/MSFS 2024", {"D:/MSFS 2024/Aircrafts"}));

    const std::vector<ProposedCategory>& categories = proposed.front().libraries.front().proposal.categories;

    QCOMPARE(categories[0].state, ProposedState::AlreadyPresent);
    QCOMPARE(categories[1].state, ProposedState::New);
}

void LegacyConfigImporterTest::ThePresetsFolderTravelsWithTheProposal()
{
    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    LegacyInstallation installation = InstallationAt("C:/ProgramData/MSFS Addons Linker 2024", {});
    installation.presetsPath = "C:/ProgramData/MSFS Addons Linker 2024/Presets";
    source.Add(installation);

    const std::vector<LegacyMigration> proposed = LegacyConfigImporter(source, probe).Propose({}, {});

    QCOMPARE(ComparablePath(proposed.front().presetsPath),
             ComparablePath("C:/ProgramData/MSFS Addons Linker 2024/Presets"));
}

void LegacyConfigImporterTest::ThePresetsOfTheInstallationAreResolvedAgainstWhatWasScanned()
{
    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe probe(fileSystem);
    FakeLegacyConfigSource source;
    LegacyPresetSelection selection;
    selection.name = "Voo curto";
    selection.enabledAddonNames = {"aircraft-a", "aircraft-que-sumiu"};
    source.PlacePreset("C:/ProgramData/MSFS Addons Linker 2024/Presets", selection);

    const std::vector<TreeNode> scanned = LibraryHolding("D:/MSFS 2024", {"D:/MSFS 2024/Aircrafts/aircraft-a"});
    const std::vector<ImportedPreset> imported =
        LegacyConfigImporter(source, probe)
            .ImportPresets("C:/ProgramData/MSFS Addons Linker 2024/Presets", ProfileHolding({"D:/MSFS 2024"}), scanned);

    QCOMPARE(imported.size(), std::size_t{1});
    QCOMPARE(imported.front().preset.name, std::string{"Voo curto"});
    QCOMPARE(imported.front().preset.entries.size(), std::size_t{1});
    QCOMPARE(imported.front().preset.entries.front().addonId.folderName, std::string{"aircraft-a"});
    QCOMPARE(imported.front().unresolvedAddonNames.size(), std::size_t{1});
}

QTEST_APPLESS_MAIN(LegacyConfigImporterTest)

#include "tst_legacy_config_importer.moc"
