#include <QtTest/QtTest>

#include "domain/tree/ToggleDirection.h"
#include "tests/support/PathPrinting.h"

class ToggleDirectionTest : public QObject
{
    Q_OBJECT

private slots:
    static void EnablesWhenSomeAddonIsFreeToEnable();
    static void DisablesWhenEverythingEnableableIsAlreadyEnabled();
    static void TriesToEnableWhenEverythingIsBlockedAndOff();
    static void ARealFolderAtTheEffectiveDestinationBlocks();
    static void AnEntryLinkingToTheAddonDoesNotBlockIt();
};

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kLiveryA = "D:/MSFS 2024/Liveries/livery-a";
    constexpr auto kLiveryB = "D:/MSFS 2024/Liveries/livery-b";
    constexpr auto kLiveryC = "D:/MSFS 2024/Liveries/livery-c";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;

        return node;
    }

    TreeNode Liveries()
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = "D:/MSFS 2024/Liveries";
        node.children = {AddonNode(kLiveryA), AddonNode(kLiveryB), AddonNode(kLiveryC)};

        return node;
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

    DestinationEntry RealFolder(const std::filesystem::path& path)
    {
        return {path, {}, EntryClassification::Unmanaged};
    }

    DestinationEntry OurLink(const std::filesystem::path& path, const std::filesystem::path& target)
    {
        return {path, target, EntryClassification::Managed};
    }
}

void ToggleDirectionTest::EnablesWhenSomeAddonIsFreeToEnable()
{
    const TreeNode category = Liveries();
    const EnabledAddons enabled({kLiveryA});
    const std::vector<DestinationEntry> entries = {
        OurLink("E:/Flight Simulator 2024/Community/livery-a", kLiveryA),
        RealFolder("E:/Flight Simulator 2024/Community/livery-b")
    };

    QVERIFY(ShouldEnable(Profile(), entries, enabled, {&category}));
}

void ToggleDirectionTest::DisablesWhenEverythingEnableableIsAlreadyEnabled()
{
    const TreeNode category = Liveries();
    const EnabledAddons enabled({kLiveryA, kLiveryC});
    const std::vector<DestinationEntry> entries = {
        OurLink("E:/Flight Simulator 2024/Community/livery-a", kLiveryA),
        RealFolder("E:/Flight Simulator 2024/Community/livery-b"),
        OurLink("E:/Flight Simulator 2024/Community/livery-c", kLiveryC)
    };

    QVERIFY(!ShouldEnable(Profile(), entries, enabled, {&category}));
}

void ToggleDirectionTest::TriesToEnableWhenEverythingIsBlockedAndOff()
{
    const TreeNode category = Liveries();
    const std::vector<DestinationEntry> entries = {
        RealFolder("E:/Flight Simulator 2024/Community/livery-a"),
        RealFolder("E:/Flight Simulator 2024/Community/livery-b"),
        RealFolder("E:/Flight Simulator 2024/Community/livery-c")
    };

    QVERIFY(ShouldEnable(Profile(), entries, EnabledAddons{}, {&category}));
}

void ToggleDirectionTest::ARealFolderAtTheEffectiveDestinationBlocks()
{
    const std::vector<DestinationEntry> entries = {
        RealFolder("E:/Flight Simulator 2024/Community/livery-a")
    };

    QVERIFY(DestinationBlocks(Profile(), entries, kLiveryA));
    QVERIFY(!DestinationBlocks(Profile(), entries, kLiveryB));
}

void ToggleDirectionTest::AnEntryLinkingToTheAddonDoesNotBlockIt()
{
    const std::vector<DestinationEntry> entries = {
        OurLink("E:/Flight Simulator 2024/Community/livery-a", kLiveryA)
    };

    QVERIFY(!DestinationBlocks(Profile(), entries, kLiveryA));
}

QTEST_APPLESS_MAIN(ToggleDirectionTest)

#include "tst_toggle_direction.moc"
