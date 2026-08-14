#include <QtTest/QtTest>

#include "domain/bisection/BisectionDrift.h"
#include "domain/linking/EntryClassifier.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class BisectionDriftTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ADiskThatDidNotMoveHasNoDivergence();
        static void AJunctionWeLeftAndIsNoLongerThereIsADivergence();
        static void AFolderThatAppearedInTheDestinationIsADivergenceTheEnabledSetCannotSee();
        static void AnEntryThatCameToPointSomewhereElseIsADivergence();
        static void AnAddonThatLeftTheLibraryIsADivergence();
        static void AnAddonThatJoinedTheLibraryIsADivergence();
        static void AJunctionDeletedAndPutBackIdenticalReadsAsNoChange();
        static void TheDivergencesComeOutInAStableOrder();
        static void AnAddonThatOnlyJoinedTheLibraryLoadedNothing();
        static void AnythingOtherThanAJoinedAddonMeansSomethingThatLoadedMoved();
        static void AJoinedAddonAlongsideAnotherKindDoesNotCountAsHarmless();
    };
}

namespace
{
    constexpr auto kFenix = "D:/MSFS 2024/Aircrafts/fenix-a320";
    constexpr auto kPmdg = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77er";
    constexpr auto kBrussels = "D:/MSFS 2024/Airports/aerosoft-airport-ebbr-brussels";
    constexpr auto kFenixLink = "E:/Flight Simulator 2024/Community/fenix-a320";
    constexpr auto kPmdgLink = "E:/Flight Simulator 2024/Community/pmdg-aircraft-77er";
    constexpr auto kStranger = "E:/Flight Simulator 2024/Community/somebody-elses-folder";

    DestinationEntry OurLink(const std::filesystem::path& path, const std::filesystem::path& target)
    {
        return {.path = path, .target = target, .classification = EntryClassification::Managed};
    }

    DestinationEntry APhysicalFolder(const std::filesystem::path& path)
    {
        return {.path = path, .target = {}, .classification = EntryClassification::Unmanaged};
    }

    DiskAsItWas TheDiskWithBothLinks()
    {
        return {.entries = {OurLink(kFenixLink, kFenix), OurLink(kPmdgLink, kPmdg)},
                .libraryAddons = {kFenix, kPmdg, kBrussels}};
    }
}

void BisectionDriftTest::ADiskThatDidNotMoveHasNoDivergence()
{
    QVERIFY(DriftBetween(TheDiskWithBothLinks(), TheDiskWithBothLinks()).empty());
}

void BisectionDriftTest::AJunctionWeLeftAndIsNoLongerThereIsADivergence()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.entries.pop_back();

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{1});
    QCOMPARE(drift.front().kind, DriftKind::ALinkWeLeftIsGone);
    QCOMPARE(drift.front().path, std::filesystem::path{kPmdgLink});
}

void BisectionDriftTest::AFolderThatAppearedInTheDestinationIsADivergenceTheEnabledSetCannotSee()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.entries.push_back(APhysicalFolder(kStranger));

    QCOMPARE(EnabledAddonFolders(now.entries), EnabledAddonFolders(TheDiskWithBothLinks().entries));

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{1});
    QCOMPARE(drift.front().kind, DriftKind::AnEntryWeDidNotLeaveIsThere);
    QCOMPARE(drift.front().path, std::filesystem::path{kStranger});
}

void BisectionDriftTest::AnEntryThatCameToPointSomewhereElseIsADivergence()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.entries.back() = OurLink(kPmdgLink, kBrussels);

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{1});
    QCOMPARE(drift.front().kind, DriftKind::AnEntryPointsSomewhereElse);
    QCOMPARE(drift.front().path, std::filesystem::path{kPmdgLink});
}

void BisectionDriftTest::AnAddonThatLeftTheLibraryIsADivergence()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.libraryAddons = {kFenix, kPmdg};

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{1});
    QCOMPARE(drift.front().kind, DriftKind::AnAddonLeftTheLibrary);
    QCOMPARE(drift.front().path, std::filesystem::path{kBrussels});
}

void BisectionDriftTest::AnAddonThatJoinedTheLibraryIsADivergence()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.libraryAddons.push_back("D:/MSFS 2024/Airports/aerosoft-airport-eddf");

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{1});
    QCOMPARE(drift.front().kind, DriftKind::AnAddonJoinedTheLibrary);
}

void BisectionDriftTest::AJunctionDeletedAndPutBackIdenticalReadsAsNoChange()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.entries.pop_back();
    now.entries.push_back(OurLink(kPmdgLink, kPmdg));

    QVERIFY(DriftBetween(TheDiskWithBothLinks(), now).empty());
}

void BisectionDriftTest::TheDivergencesComeOutInAStableOrder()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.entries.pop_back();
    now.entries.push_back(APhysicalFolder(kStranger));
    now.libraryAddons = {kFenix, kPmdg};

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{3});
    QCOMPARE(drift[0].kind, DriftKind::ALinkWeLeftIsGone);
    QCOMPARE(drift[1].kind, DriftKind::AnEntryWeDidNotLeaveIsThere);
    QCOMPARE(drift[2].kind, DriftKind::AnAddonLeftTheLibrary);
}

void BisectionDriftTest::AnAddonThatOnlyJoinedTheLibraryLoadedNothing()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.libraryAddons.push_back("D:/MSFS 2024/Airports/aerosoft-airport-eddf");

    QVERIFY(NothingThatLoadedMoved(DriftBetween(TheDiskWithBothLinks(), now)));
}

void BisectionDriftTest::AnythingOtherThanAJoinedAddonMeansSomethingThatLoadedMoved()
{
    DiskAsItWas withoutALink = TheDiskWithBothLinks();
    withoutALink.entries.pop_back();

    DiskAsItWas withAStranger = TheDiskWithBothLinks();
    withAStranger.entries.push_back(APhysicalFolder(kStranger));

    DiskAsItWas withoutAnAddon = TheDiskWithBothLinks();
    withoutAnAddon.libraryAddons.pop_back();

    DiskAsItWas pointingElsewhere = TheDiskWithBothLinks();
    pointingElsewhere.entries.back().target = kBrussels;

    for (const DiskAsItWas& now : {withoutALink, withAStranger, withoutAnAddon, pointingElsewhere})
    {
        const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

        QVERIFY(!drift.empty());
        QVERIFY(!NothingThatLoadedMoved(drift));
    }
}

void BisectionDriftTest::AJoinedAddonAlongsideAnotherKindDoesNotCountAsHarmless()
{
    DiskAsItWas now = TheDiskWithBothLinks();
    now.libraryAddons.push_back("D:/MSFS 2024/Airports/aerosoft-airport-eddf");
    now.entries.pop_back();

    const std::vector<Divergence> drift = DriftBetween(TheDiskWithBothLinks(), now);

    QCOMPARE(drift.size(), std::size_t{2});
    QVERIFY(!NothingThatLoadedMoved(drift));
}

QTEST_APPLESS_MAIN(BisectionDriftTest)

#include "tst_bisection_drift.moc"
