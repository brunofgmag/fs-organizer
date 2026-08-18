#include <QtTest/QtTest>

#include <chrono>

#include "domain/journal/LinksTheAppMade.h"
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

QTEST_APPLESS_MAIN(LinksTheAppMadeTest)

#include "tst_links_the_app_made.moc"
