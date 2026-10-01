#include <vector>

#include <QtTest/QtTest>

#include "domain/linking/EntryClassifier.h"
#include "domain/linking/LinksByTarget.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/DestinationDivergence.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class LinksByTargetTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void PointingAtAgreesWithLinksPointingAtForEveryAddon();
        static void AnAddonNobodyPointsAtGetsAnEmptyAnswer();
        static void TheLinksComeInTheOrderOfTheEntries();
        static void AnIndexOfNothingAnswersEmpty();
        static void TheComparableSpellingAnswersLikeThePath();
        static void TheEnabledAddonsOfACategoryBuildOneIndexWhateverTheirCount();
        static void TheEnabledAddonsOfACategoryAdoptTheParentOfTheFirstLink();
        static void TheEnabledAddonsOfACategoryDisagreeWhenTwoDestinationsHoldThem();
    };
}

namespace
{
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCrj = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kAtr = "D:/MSFS 2024/Aircrafts/hype-atr";
    constexpr auto kEglc = "D:/MSFS 2024/Sceneries/orbx-eglc";
    constexpr auto kLfmn = "D:/MSFS 2024/Sceneries/orbx-lfmn";
    constexpr auto kGone = "D:/MSFS 2024/Sceneries/gone";
    constexpr auto kNeverLinked = "D:/MSFS 2024/Sceneries/never-linked";
    constexpr auto kVendor = "C:/Vendor/gsx-pro";

    DestinationEntry LinkAt(const std::filesystem::path& path,
                            const std::filesystem::path& target,
                            const EntryClassification classification)
    {
        return {.path = path, .target = target, .classification = classification};
    }

    std::vector<DestinationEntry> EntriesInTwoDestinations()
    {
        return {LinkAt("E:/Flight Simulator 2024/Community/aerosoft-crj", kCrj, EntryClassification::Duplicated),
                LinkAt("E:/Flight Simulator 2024/Community/hype-atr", kAtr, EntryClassification::Managed),
                LinkAt("E:/Flight Simulator 2024/Community2024/aerosoft-crj", kCrj, EntryClassification::Duplicated),
                LinkAt("E:/Flight Simulator 2024/Community2024/orbx-eglc", R"(d:\msfs 2024\sceneries\ORBX-EGLC\)",
                       EntryClassification::Managed),
                LinkAt("E:/Flight Simulator 2024/Community/gone", kGone, EntryClassification::Broken),
                LinkAt("E:/Flight Simulator 2024/Community/gsx-pro", kVendor, EntryClassification::External),
                LinkAt("E:/Flight Simulator 2024/Community/orbx-lfmn", kLfmn, EntryClassification::Divergent),
                LinkAt("E:/Flight Simulator 2024/Community/plain-folder", {}, EntryClassification::Unmanaged),
                LinkAt("E:/Flight Simulator 2024/Community/vanished", kVendor, EntryClassification::Vanished)};
    }

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;

        return node;
    }

    TreeNode CategoryOf(const std::vector<std::filesystem::path>& addons)
    {
        TreeNode category;
        category.kind = TreeNodeKind::Category;
        category.path = kLibrary;

        for (const std::filesystem::path& addon : addons)
        {
            category.children.push_back(AddonNode(addon));
        }

        return category;
    }
}

void LinksByTargetTest::PointingAtAgreesWithLinksPointingAtForEveryAddon()
{
    const std::vector<DestinationEntry> entries = EntriesInTwoDestinations();
    const LinksByTarget index(entries);

    const std::vector<std::filesystem::path> everyAddon = {kCrj, kAtr, kEglc, kLfmn, kGone, kVendor, kNeverLinked};

    for (const std::filesystem::path& addon : everyAddon)
    {
        QCOMPARE(index.PointingAt(addon), LinksPointingAt(entries, addon));
    }

    QCOMPARE(index.PointingAt(kCrj).size(), std::size_t{2});
    QCOMPARE(index.PointingAt(kEglc).size(), std::size_t{1});
    QCOMPARE(index.PointingAt(kLfmn).size(), std::size_t{1});
    QVERIFY(index.PointingAt(kGone).empty());
    QVERIFY(index.PointingAt(kVendor).empty());
}

void LinksByTargetTest::AnAddonNobodyPointsAtGetsAnEmptyAnswer()
{
    const LinksByTarget index(EntriesInTwoDestinations());

    QVERIFY(index.PointingAt(kNeverLinked).empty());
    QVERIFY(index.PointingAt("").empty());
}

void LinksByTargetTest::TheLinksComeInTheOrderOfTheEntries()
{
    const LinksByTarget index(EntriesInTwoDestinations());

    const std::vector<std::filesystem::path> expected = {"E:/Flight Simulator 2024/Community/aerosoft-crj",
                                                         "E:/Flight Simulator 2024/Community2024/aerosoft-crj"};

    QCOMPARE(index.PointingAt(kCrj), expected);
}

void LinksByTargetTest::AnIndexOfNothingAnswersEmpty()
{
    QVERIFY(LinksByTarget{}.PointingAt(kCrj).empty());
    QVERIFY(LinksByTarget(std::vector<DestinationEntry>{}).PointingAt(kCrj).empty());
}

void LinksByTargetTest::TheComparableSpellingAnswersLikeThePath()
{
    const LinksByTarget index(EntriesInTwoDestinations());

    for (const std::filesystem::path& addon : {std::filesystem::path(kCrj), std::filesystem::path(kEglc)})
    {
        QCOMPARE(index.PointingAtComparable(ComparablePath(addon)), index.PointingAt(addon));
    }
}

void LinksByTargetTest::TheEnabledAddonsOfACategoryBuildOneIndexWhateverTheirCount()
{
    const std::vector<DestinationEntry> entries = EntriesInTwoDestinations();
    const TreeNode category = CategoryOf({kCrj, kAtr, kEglc, kLfmn, kGone, kNeverLinked});

    const std::size_t before = LinksByTarget::TimesItWasBuilt();

    static_cast<void>(WhereTheEnabledAddonsPoint(category, entries));

    QCOMPARE(LinksByTarget::TimesItWasBuilt() - before, std::size_t{1});
}

void LinksByTargetTest::TheEnabledAddonsOfACategoryAdoptTheParentOfTheFirstLink()
{
    const std::vector<DestinationEntry> entries = {
        LinkAt("E:/Flight Simulator 2024/Community/aerosoft-crj", kCrj, EntryClassification::Duplicated),
        LinkAt(R"(E:\Flight Simulator 2024\community\aerosoft-crj)", kCrj, EntryClassification::Duplicated),
        LinkAt("E:/Flight Simulator 2024/Community/hype-atr", kAtr, EntryClassification::Managed)};

    const DestinationAgreement agreement = WhereTheEnabledAddonsPoint(CategoryOf({kCrj, kAtr}), entries);

    QVERIFY(agreement.Adoptable());
    QCOMPARE(QString::fromStdString(agreement.destination.generic_string()), QString(kCommunity));
}

void LinksByTargetTest::TheEnabledAddonsOfACategoryDisagreeWhenTwoDestinationsHoldThem()
{
    const DestinationAgreement agreement =
        WhereTheEnabledAddonsPoint(CategoryOf({kCrj, kAtr}), EntriesInTwoDestinations());

    QVERIFY(!agreement.unanimous);
    QVERIFY(!agreement.Adoptable());
    QVERIFY(agreement.destination.empty());
}

QTEST_MAIN(LinksByTargetTest)
#include "tst_links_by_target.moc"
