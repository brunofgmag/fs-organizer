#include <QtTest/QtTest>

#include "domain/legacy/LegacyPreset.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class LegacyPresetTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ANamedAddonBecomesAnEnableEntry();
        static void AnAddonMarkedWithAnAsteriskBecomesADisableEntry();
        static void AFolderIsExpandedIntoTheAddonsUnderItRightNow();
        static void AnAddonThatIsNotInAnyLibraryIsReportedInsteadOfDropped();
        static void AFolderThatIsNotInAnyLibraryIsReportedInsteadOfDropped();
        static void TheSameAddonNamedTwiceIsEnteredOnce();
        static void AnAddonBothNamedAndMarkedForDisablingIsDisabledOnce();
        static void TheNameOfThePresetSurvivesTheImport();
        static void ASelectionWithoutAnyLineImportsAnEmptyPreset();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kAircrafts = "D:/MSFS 2024/Aircrafts";
    constexpr auto kAircraftA = "D:/MSFS 2024/Aircrafts/aircraft-a";
    constexpr auto kAircraftB = "D:/MSFS 2024/Aircrafts/aircraft-b";
    constexpr auto kSceneryZ = "D:/MSFS 2024/Sceneries/scenery-z";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    std::vector<TreeNode> Libraries()
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {CategoryNode(kAircrafts, {AddonNode(kAircraftA), AddonNode(kAircraftB)}),
                            CategoryNode("D:/MSFS 2024/Sceneries", {AddonNode(kSceneryZ)})};

        return {std::move(library)};
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.libraries = {Library{.id = "library-1", .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }
}

void LegacyPresetTest::ANamedAddonBecomesAnEnableEntry()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.enabledAddonNames = {"aircraft-a"};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QCOMPARE(imported.preset.entries.size(), std::size_t{1});
    QCOMPARE(imported.preset.entries.front().addonId.folderName, std::string{"aircraft-a"});
    QCOMPARE(imported.preset.entries.front().addonId.libraryId, std::string{"library-1"});
    QCOMPARE(imported.preset.entries.front().action, PresetAction::Enable);
}

void LegacyPresetTest::AnAddonMarkedWithAnAsteriskBecomesADisableEntry()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.disabledAddonNames = {"aircraft-b"};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QCOMPARE(imported.preset.entries.size(), std::size_t{1});
    QCOMPARE(imported.preset.entries.front().addonId.folderName, std::string{"aircraft-b"});
    QCOMPARE(imported.preset.entries.front().action, PresetAction::Disable);
}

void LegacyPresetTest::AFolderIsExpandedIntoTheAddonsUnderItRightNow()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.enabledFolders = {R"(d:\msfs 2024\aircrafts)"};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QCOMPARE(imported.preset.entries.size(), std::size_t{2});
    QCOMPARE(imported.preset.entries[0].addonId.folderName, std::string{"aircraft-a"});
    QCOMPARE(imported.preset.entries[1].addonId.folderName, std::string{"aircraft-b"});
    QVERIFY(imported.unresolvedFolders.empty());
}

void LegacyPresetTest::AnAddonThatIsNotInAnyLibraryIsReportedInsteadOfDropped()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.enabledAddonNames = {"aircraft-a", "aircraft-que-sumiu"};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QCOMPARE(imported.preset.entries.size(), std::size_t{1});
    QCOMPARE(imported.unresolvedAddonNames.size(), std::size_t{1});
    QCOMPARE(imported.unresolvedAddonNames.front(), std::string{"aircraft-que-sumiu"});
}

void LegacyPresetTest::AFolderThatIsNotInAnyLibraryIsReportedInsteadOfDropped()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.enabledFolders = {R"(d:\msfs 2024\liveries)"};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QVERIFY(imported.preset.entries.empty());
    QCOMPARE(imported.unresolvedFolders.size(), std::size_t{1});
    QCOMPARE(imported.unresolvedFolders.front(), std::filesystem::path{R"(d:\msfs 2024\liveries)"});
}

void LegacyPresetTest::TheSameAddonNamedTwiceIsEnteredOnce()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.enabledAddonNames = {"aircraft-a"};
    selection.enabledFolders = {kAircrafts};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QCOMPARE(imported.preset.entries.size(), std::size_t{2});
}

void LegacyPresetTest::AnAddonBothNamedAndMarkedForDisablingIsDisabledOnce()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.enabledAddonNames = {"aircraft-a"};
    selection.disabledAddonNames = {"aircraft-a"};

    const ImportedPreset imported = ImportLegacyPreset(selection, Profile(), libraries);

    QCOMPARE(imported.preset.entries.size(), std::size_t{1});
    QCOMPARE(imported.preset.entries.front().action, PresetAction::Disable);
}

void LegacyPresetTest::TheNameOfThePresetSurvivesTheImport()
{
    const std::vector<TreeNode> libraries = Libraries();
    LegacyPresetSelection selection;
    selection.name = "Voo curto";

    QCOMPARE(ImportLegacyPreset(selection, Profile(), libraries).preset.name, std::string{"Voo curto"});
}

void LegacyPresetTest::ASelectionWithoutAnyLineImportsAnEmptyPreset()
{
    const std::vector<TreeNode> libraries = Libraries();

    const ImportedPreset imported = ImportLegacyPreset({}, Profile(), libraries);

    QVERIFY(imported.preset.entries.empty());
    QVERIFY(imported.unresolvedAddonNames.empty());
    QVERIFY(imported.unresolvedFolders.empty());
}

QTEST_APPLESS_MAIN(LegacyPresetTest)

#include "tst_legacy_preset.moc"
