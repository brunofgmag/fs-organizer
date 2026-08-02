#include <QtTest/QtTest>

#include "domain/linking/RepairPlan.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class RepairPlanTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void OnlyBrokenEntriesEnterThePlan();
        static void UnavailableEntriesNeverEnterThePlan();
        static void ALinkIntoALibraryIsGroupedApartFromAThirdPartyOne();
        static void RepointIsOfferedOnlyWhenAValidAddonSharesTheBaseName();
        static void TheBaseNameMatchIgnoresCase();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;

        return node;
    }

    TreeNode EmptyCategory(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;
        node.children = {AddonNode("D:/MSFS 2024/Utilities/flybywire-externaltools-simbridge"),
                         EmptyCategory("D:/MSFS 2024/Utilities/fs2crew-cmd-center")};

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

    DestinationEntry Entry(const std::filesystem::path& path,
                           const std::filesystem::path& target,
                           const EntryClassification classification)
    {
        return {.path = path, .target = target, .classification = classification};
    }
}

void RepairPlanTest::OnlyBrokenEntriesEnterThePlan()
{
    const std::vector<DestinationEntry> entries = {
        Entry("E:/Flight Simulator 2024/Community/alive", "D:/MSFS 2024/Sceneries/alive", EntryClassification::Managed),
        Entry("E:/Flight Simulator 2024/Community/gone", "D:/Removed/gone", EntryClassification::Broken),
        Entry("E:/Flight Simulator 2024/Community/physical", {}, EntryClassification::Unmanaged),
        Entry("E:/Flight Simulator 2024/Community/foreign", "C:/Elsewhere/foreign", EntryClassification::External)};

    const std::vector<RepairCandidate> plan = PlanRepairs(Profile(), entries, {LibraryTree()});

    QCOMPARE(plan.size(), std::size_t{1});
    QCOMPARE(plan.front().entry.path, std::filesystem::path("E:/Flight Simulator 2024/Community/gone"));
}

void RepairPlanTest::UnavailableEntriesNeverEnterThePlan()
{
    const std::vector<DestinationEntry> entries = {Entry("E:/Flight Simulator 2024/Community/unplugged",
                                                         "X:/Library/unplugged", EntryClassification::Unavailable)};

    QVERIFY(PlanRepairs(Profile(), entries, {LibraryTree()}).empty());
}

void RepairPlanTest::ALinkIntoALibraryIsGroupedApartFromAThirdPartyOne()
{
    const std::vector<DestinationEntry> entries = {
        Entry("E:/Flight Simulator 2024/Community/ours", "D:/MSFS 2024/Sceneries/ours", EntryClassification::Broken),
        Entry("E:/Flight Simulator 2024/Community/theirs", "C:/OtherTool/theirs", EntryClassification::Broken)};

    const std::vector<RepairCandidate> plan = PlanRepairs(Profile(), entries, {LibraryTree()});

    QCOMPARE(plan.size(), std::size_t{2});
    QVERIFY(plan[0].targetsLibrary);
    QVERIFY(!plan[1].targetsLibrary);
}

void RepairPlanTest::RepointIsOfferedOnlyWhenAValidAddonSharesTheBaseName()
{
    const std::vector<DestinationEntry> entries = {
        Entry("E:/Flight Simulator 2024/Community/flybywire-externaltools-simbridge",
              "D:/Old Library/flybywire-externaltools-simbridge", EntryClassification::Broken),
        Entry("E:/Flight Simulator 2024/Community/fs2crew-cmd-center", "D:/Old Library/fs2crew-cmd-center",
              EntryClassification::Broken)};

    const std::vector<RepairCandidate> plan = PlanRepairs(Profile(), entries, {LibraryTree()});

    QCOMPARE(plan.size(), std::size_t{2});
    QCOMPARE(plan[0].repointTo,
             std::optional<std::filesystem::path>("D:/MSFS 2024/Utilities/flybywire-externaltools-simbridge"));
    QCOMPARE(plan[1].repointTo, std::optional<std::filesystem::path>{});
}

void RepairPlanTest::TheBaseNameMatchIgnoresCase()
{
    const std::vector<DestinationEntry> entries = {
        Entry("E:/Flight Simulator 2024/Community/FlyByWire-ExternalTools-SimBridge",
              "D:/Old Library/FlyByWire-ExternalTools-SimBridge", EntryClassification::Broken)};

    const std::vector<RepairCandidate> plan = PlanRepairs(Profile(), entries, {LibraryTree()});

    QCOMPARE(plan.size(), std::size_t{1});
    QCOMPARE(plan.front().repointTo,
             std::optional<std::filesystem::path>("D:/MSFS 2024/Utilities/flybywire-externaltools-simbridge"));
}

QTEST_APPLESS_MAIN(RepairPlanTest)

#include "tst_repair_plan.moc"
