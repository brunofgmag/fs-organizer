#include <QtTest/QtTest>

#include <chrono>
#include <map>
#include <ranges>
#include <string>

#include "domain/journal/LinksTheAppMade.h"
#include "domain/support/PathUtils.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class LinksTheAppMadeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryPlaceTheAppLinkedIsRememberedWithTheFolderItPointedAt();
        static void ALinkTheAppTookAwayIsNotRememberedAsStillOurs();
        static void ALinkThatNeverGotMadeIsNotRemembered();
        static void MovingTheAddonInTheLibraryMovesWhatTheLinkPointedAt();
        static void TheLastWordOnAPlaceWins();
        static void AChainOfMovesFollowsTheLinkEveryTime();
        static void FoldingOneRecordAtATimeAnswersLikeTheWholeHistoryAfterEveryPrefix();
    };

    const std::filesystem::path kLibraryCopy = "D:/Library/Utilities/navigraph-nav-base";
    const std::filesystem::path kPlace = "E:/Sim/Community/navigraph-nav-base";

    [[nodiscard]] OperationRecord Link(const OperationKind kind,
                                       const std::filesystem::path& source,
                                       const std::filesystem::path& target,
                                       const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(std::chrono::system_clock::time_point{}, kind, AddonId{}, source, target,
                                       failure);
    }

    [[nodiscard]] OperationRecord Moved(const std::filesystem::path& source, const std::filesystem::path& target)
    {
        return OperationRecord::OfImport(std::chrono::system_clock::time_point{}, OperationKind::MoveAddon, AddonId{},
                                         source, target, FileResult::Completed);
    }

    [[nodiscard]] std::vector<LinkTheAppMade>
    TheWholeHistoryFoldedFromScratch(const std::vector<OperationRecord>& history)
    {
        std::map<std::string, LinkTheAppMade> made;

        for (const OperationRecord& record : history)
        {
            if (!Succeeded(record.outcome))
            {
                continue;
            }

            if (CreatesALink(record.kind))
            {
                made.insert_or_assign(ComparablePath(record.target),
                                      LinkTheAppMade{.place = record.target, .libraryCopy = record.source});
            }
            else if (record.kind == OperationKind::DisableAddon || record.kind == OperationKind::RemoveBrokenLink)
            {
                made.erase(ComparablePath(record.target));
            }
            else if (record.kind == OperationKind::MoveAddon)
            {
                for (LinkTheAppMade& link : made | std::views::values)
                {
                    if (ComparablePath(link.libraryCopy) == ComparablePath(record.source))
                    {
                        link.libraryCopy = record.target;
                    }
                }
            }
        }

        std::vector<LinkTheAppMade> links;
        for (const LinkTheAppMade& link : made | std::views::values)
        {
            links.push_back(link);
        }

        return links;
    }

    void CompareLinks(const std::vector<LinkTheAppMade>& actual, const std::vector<LinkTheAppMade>& expected)
    {
        QCOMPARE(actual.size(), expected.size());

        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            QCOMPARE(actual[index].place, expected[index].place);
            QCOMPARE(actual[index].libraryCopy, expected[index].libraryCopy);
        }
    }
}

void LinksTheAppMadeTest::EveryPlaceTheAppLinkedIsRememberedWithTheFolderItPointedAt()
{
    const std::vector<LinkTheAppMade> made =
        WhereTheAppMadeLinks({Link(OperationKind::EnableAddon, kLibraryCopy, kPlace)});

    QCOMPARE(made.size(), std::size_t{1});
    QCOMPARE(made.front().place, kPlace);
    QCOMPARE(made.front().libraryCopy, kLibraryCopy);
}

void LinksTheAppMadeTest::ALinkTheAppTookAwayIsNotRememberedAsStillOurs()
{
    const std::vector<LinkTheAppMade> made =
        WhereTheAppMadeLinks({Link(OperationKind::EnableAddon, kLibraryCopy, kPlace),
                              Link(OperationKind::DisableAddon, kLibraryCopy, kPlace)});

    QVERIFY2(made.empty(), "a link the user turned off left nothing of ours in that place");
}

void LinksTheAppMadeTest::ALinkThatNeverGotMadeIsNotRemembered()
{
    const std::vector<LinkTheAppMade> made = WhereTheAppMadeLinks(
        {Link(OperationKind::EnableAddon, kLibraryCopy, kPlace, LinkFailure::DestinationHoldsRealFolder)});

    QVERIFY(made.empty());
}

void LinksTheAppMadeTest::MovingTheAddonInTheLibraryMovesWhatTheLinkPointedAt()
{
    const std::filesystem::path landed = "D:/Library/Navdata/navigraph-nav-base";

    const std::vector<LinkTheAppMade> made =
        WhereTheAppMadeLinks({Link(OperationKind::EnableAddon, kLibraryCopy, kPlace), Moved(kLibraryCopy, landed)});

    QCOMPARE(made.size(), std::size_t{1});
    QCOMPARE(made.front().libraryCopy, landed);
}

void LinksTheAppMadeTest::TheLastWordOnAPlaceWins()
{
    const std::filesystem::path other = "D:/Library/Aircrafts/navigraph-nav-base";

    const std::vector<LinkTheAppMade> made = WhereTheAppMadeLinks(
        {Link(OperationKind::EnableAddon, kLibraryCopy, kPlace), Link(OperationKind::RepointLink, other, kPlace)});

    QCOMPARE(made.size(), std::size_t{1});
    QCOMPARE(made.front().libraryCopy, other);
}

void LinksTheAppMadeTest::AChainOfMovesFollowsTheLinkEveryTime()
{
    const std::filesystem::path second = "D:/Library/Navdata/navigraph-nav-base";
    const std::filesystem::path third = "D:/Library/Archive/navigraph-nav-base";

    LinksTheAppMadeSoFar made;
    made.Fold(Link(OperationKind::EnableAddon, kLibraryCopy, kPlace));
    made.Fold(Moved(kLibraryCopy, second));
    made.Fold(Moved(second, third));

    const std::vector<LinkTheAppMade> links = made.Links();

    QCOMPARE(links.size(), std::size_t{1});
    QCOMPARE(links.front().place, kPlace);
    QCOMPARE(links.front().libraryCopy, third);
}

void LinksTheAppMadeTest::FoldingOneRecordAtATimeAnswersLikeTheWholeHistoryAfterEveryPrefix()
{
    const std::filesystem::path otherPlace = "E:/Sim/Community/other-addon";
    const std::filesystem::path otherCopy = "D:/Library/Utilities/other-addon";
    const std::filesystem::path moved = "D:/Library/Navdata/navigraph-nav-base";
    const std::filesystem::path movedAgain = "D:/Library/Archive/NAVIGRAPH-nav-base";

    const std::vector<OperationRecord> history{
        Link(OperationKind::EnableAddon, kLibraryCopy, kPlace),
        Link(OperationKind::EnableAddon, otherCopy, otherPlace),
        Link(OperationKind::EnableAddon, kLibraryCopy, "E:/Sim/Community2024/navigraph-nav-base"),
        Moved(kLibraryCopy, moved),
        Link(OperationKind::DisableAddon, otherCopy, otherPlace),
        Link(OperationKind::EnableAddon, otherCopy, otherPlace, LinkFailure::DestinationHoldsRealFolder),
        Moved(moved, movedAgain),
        Link(OperationKind::RemoveBrokenLink, movedAgain, "E:/Sim/Community2024/navigraph-nav-base"),
        Link(OperationKind::EnableAddon, otherCopy, otherPlace),
        Moved(otherCopy, "D:/Library/Aircrafts/other-addon"),
        Link(OperationKind::RepointLink, "D:/Library/Elsewhere/other-addon", otherPlace),
    };

    LinksTheAppMadeSoFar incremental;
    std::vector<OperationRecord> prefix;

    for (const OperationRecord& record : history)
    {
        incremental.Fold(record);
        prefix.push_back(record);

        CompareLinks(incremental.Links(), TheWholeHistoryFoldedFromScratch(prefix));
        CompareLinks(WhereTheAppMadeLinks(prefix), TheWholeHistoryFoldedFromScratch(prefix));
    }
}

QTEST_APPLESS_MAIN(LinksTheAppMadeTest)

#include "tst_links_the_app_made.moc"
