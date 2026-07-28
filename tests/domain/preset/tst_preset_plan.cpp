#include <QtTest/QtTest>

#include "domain/preset/PresetPlan.h"
#include "tests/support/PathPrinting.h"

class PresetPlanTest : public QObject
{
    Q_OBJECT

private slots:
    static void ReplaceEnablesThePresetAndDisablesWhatItDoesNotName();
    static void CumulativeLeavesAloneWhatThePresetDoesNotName();
    static void ADisableEntryTurnsTheAddonOffInCumulative();
    static void TheDisableModeTurnsOffTheEnableEntriesAndIgnoresTheDisableOnes();
    static void AnEntryIsMatchedWhateverTheCaseOfItsFolderNameAndLibrary();
    static void AnEntryWhoseAddonIsGoneIsReported();
    static void AnEntryFromALibraryTheProfileNoLongerHoldsIsReported();
    static void ReplaceSweepsEveryLibraryOfTheProfile();
    static void TheEntriesOfWhatIsEnabledNameEveryEnabledAddonAndNothingElse();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kAircraftA = "D:/MSFS 2024/Aircrafts/aircraft-a";
    constexpr auto kAircraftB = "D:/MSFS 2024/Aircrafts/aircraft-b";
    constexpr auto kAircraftC = "D:/MSFS 2024/Aircrafts/aircraft-c";
    constexpr auto kSecondLibrary = "E:/Cenarios";
    constexpr auto kSceneryZ = "E:/Cenarios/scenery-z";

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
        aircrafts.path = "D:/MSFS 2024/Aircrafts";
        aircrafts.children = {AddonNode(kAircraftA), AddonNode(kAircraftB), AddonNode(kAircraftC)};

        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {std::move(aircrafts)};

        return library;
    }

    TreeNode SecondLibraryTree()
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kSecondLibrary;
        library.children = {AddonNode(kSceneryZ)};

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{"library-1", kLibrary, "MSFS 2024"}};

        return profile;
    }

    PresetEntry Enabling(const std::string& folderName)
    {
        return {AddonId{"library-1", folderName}, PresetAction::Enable};
    }

    PresetEntry Disabling(const std::string& folderName)
    {
        return {AddonId{"library-1", folderName}, PresetAction::Disable};
    }
}

void PresetPlanTest::ReplaceEnablesThePresetAndDisablesWhatItDoesNotName()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB, kAircraftC}};
    const Preset preset{"Voo curto", {Enabling("aircraft-a"), Enabling("aircraft-b")}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Replace, Profile(), libraries, enabled);

    QCOMPARE(plan.toEnable.size(), std::size_t{1});
    QCOMPARE(plan.toEnable.front()->path, std::filesystem::path{kAircraftA});
    QCOMPARE(plan.toDisable.size(), std::size_t{1});
    QCOMPARE(plan.toDisable.front()->path, std::filesystem::path{kAircraftC});
    QCOMPARE(plan.alreadyInPlace.size(), std::size_t{1});
    QCOMPARE(plan.alreadyInPlace.front()->path, std::filesystem::path{kAircraftB});
    QVERIFY(plan.unresolved.empty());
}

void PresetPlanTest::CumulativeLeavesAloneWhatThePresetDoesNotName()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB, kAircraftC}};
    const Preset preset{"Voo curto", {Enabling("aircraft-a"), Enabling("aircraft-b")}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Cumulative, Profile(), libraries, enabled);

    QCOMPARE(plan.toEnable.size(), std::size_t{1});
    QCOMPARE(plan.toEnable.front()->path, std::filesystem::path{kAircraftA});
    QVERIFY(plan.toDisable.empty());
    QVERIFY(plan.unresolved.empty());
}

void PresetPlanTest::ADisableEntryTurnsTheAddonOffInCumulative()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB, kAircraftC}};
    const Preset preset{"Voo curto", {Enabling("aircraft-a"), Disabling("aircraft-c")}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Cumulative, Profile(), libraries, enabled);

    QCOMPARE(plan.toEnable.size(), std::size_t{1});
    QCOMPARE(plan.toEnable.front()->path, std::filesystem::path{kAircraftA});
    QCOMPARE(plan.toDisable.size(), std::size_t{1});
    QCOMPARE(plan.toDisable.front()->path, std::filesystem::path{kAircraftC});
}

void PresetPlanTest::TheDisableModeTurnsOffTheEnableEntriesAndIgnoresTheDisableOnes()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB, kAircraftC}};
    const Preset preset{"Voo curto", {Enabling("aircraft-b"), Disabling("aircraft-c")}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Disable, Profile(), libraries, enabled);

    QCOMPARE(plan.toDisable.size(), std::size_t{1});
    QCOMPARE(plan.toDisable.front()->path, std::filesystem::path{kAircraftB});
    QVERIFY(plan.toEnable.empty());
    QVERIFY(plan.unresolved.empty());
}

void PresetPlanTest::AnEntryIsMatchedWhateverTheCaseOfItsFolderNameAndLibrary()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB}};
    const Preset preset{"Voo curto", {{AddonId{"LIBRARY-1", "Aircraft-A"}, PresetAction::Enable}}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Cumulative, Profile(), libraries, enabled);

    QVERIFY(plan.unresolved.empty());
    QCOMPARE(plan.toEnable.size(), std::size_t{1});
    QCOMPARE(plan.toEnable.front()->path, std::filesystem::path{kAircraftA});
}

void PresetPlanTest::AnEntryWhoseAddonIsGoneIsReported()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB}};
    const Preset preset{"Voo curto", {Enabling("aircraft-a"), Enabling("aircraft-gone")}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Cumulative, Profile(), libraries, enabled);

    QCOMPARE(plan.unresolved.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(plan.unresolved.front().folderName), QString{"aircraft-gone"});
    QCOMPARE(plan.toEnable.size(), std::size_t{1});
    QCOMPARE(plan.toEnable.front()->path, std::filesystem::path{kAircraftA});
}

void PresetPlanTest::AnEntryFromALibraryTheProfileNoLongerHoldsIsReported()
{
    const std::vector<TreeNode> libraries{LibraryTree()};
    const EnabledAddons enabled{{kAircraftB}};
    const Preset preset{"Voo curto", {{AddonId{"library-2", "aircraft-a"}, PresetAction::Enable}}};

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Cumulative, Profile(), libraries, enabled);

    QCOMPARE(plan.unresolved.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(plan.unresolved.front().libraryId), QString{"library-2"});
    QVERIFY(plan.toEnable.empty());
}

void PresetPlanTest::ReplaceSweepsEveryLibraryOfTheProfile()
{
    const std::vector<TreeNode> libraries{LibraryTree(), SecondLibraryTree()};
    const EnabledAddons enabled{{kAircraftB, kSceneryZ}};
    const Preset preset{"Voo curto", {Enabling("aircraft-b")}};

    SimulatorProfile profile = Profile();
    profile.libraries.push_back(Library{"library-2", kSecondLibrary, "Cenários"});

    const PresetPlan plan = PlanPresetApplication(preset, ApplyMode::Replace, profile, libraries, enabled);

    QCOMPARE(plan.toDisable.size(), std::size_t{1});
    QCOMPARE(plan.toDisable.front()->path, std::filesystem::path{kSceneryZ});
}

void PresetPlanTest::TheEntriesOfWhatIsEnabledNameEveryEnabledAddonAndNothingElse()
{
    const std::vector<TreeNode> libraries{LibraryTree(), SecondLibraryTree()};
    const EnabledAddons enabled{{kAircraftB, kSceneryZ}};

    SimulatorProfile profile = Profile();
    profile.libraries.push_back(Library{"library-2", kSecondLibrary, "Cenários"});

    const std::vector<PresetEntry> entries = EntriesForWhatIsEnabled(profile, libraries, enabled);

    QCOMPARE(entries.size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(entries.front().addonId.libraryId), QString{"library-1"});
    QCOMPARE(QString::fromStdString(entries.front().addonId.folderName), QString{"aircraft-b"});
    QCOMPARE(QString::fromStdString(entries.back().addonId.libraryId), QString{"library-2"});
    QCOMPARE(QString::fromStdString(entries.back().addonId.folderName), QString{"scenery-z"});

    for (const PresetEntry& entry : entries)
    {
        QVERIFY(entry.action == PresetAction::Enable);
    }
}

QTEST_MAIN(PresetPlanTest)

#include "tst_preset_plan.moc"
