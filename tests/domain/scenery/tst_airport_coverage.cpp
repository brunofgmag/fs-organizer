#include <QtTest/QtTest>

#include <string>
#include <vector>

#include "domain/model/SceneryFolder.h"
#include "domain/scenery/AirportCoverage.h"
#include "domain/support/PathUtils.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class AirportCoverageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheCodesOfAnAddonComeFromEveryFileItCarries();
        static void AnAddonWithNoAirportRecordIsNotAnAirportAndItsNameDoesNotPromoteIt();
        static void AnAddonWhoseIdentifierDidNotDecodeSaysSoInsteadOfSayingItIsNotAnAirport();
        static void TheSameAddonReachedByTheLibraryAndByTheLinkCountsOnce();
        static void TwoAddonsOfTheSameAirportMakeAGroup();
        static void AnAddonWithoutACodeNeverJoinsAGroup();
        static void TheSceneryFolderIsRecognisedWhateverTheCaseAndNothingElseIs();
    };

    const LibraryId kLibrary = "library-1";

    [[nodiscard]] SceneryOfAnAddon AddonAt(const std::string& folderName, std::vector<SceneryCodes> files)
    {
        return {.addon = {.libraryId = kLibrary, .folderName = folderName},
                .resolvedPath = PathFromUtf8("D:/Library/Sceneries/" + folderName),
                .files = std::move(files)};
    }

    [[nodiscard]] SceneryCodes Carrying(std::vector<std::string> codes)
    {
        return {.reading = SceneryReading::Read, .codes = std::move(codes), .anIdentifierDidNotDecode = false};
    }

    [[nodiscard]] SceneryCodes CarryingNoRecord()
    {
        return {.reading = SceneryReading::Read, .codes = {}, .anIdentifierDidNotDecode = false};
    }

    [[nodiscard]] SceneryCodes ThatDidNotDecode()
    {
        return {.reading = SceneryReading::Read, .codes = {}, .anIdentifierDidNotDecode = true};
    }

    [[nodiscard]] QStringList Listed(const std::vector<std::string>& codes)
    {
        QStringList text;
        for (const std::string& code : codes)
        {
            text << QString::fromStdString(code);
        }

        return text;
    }
}

void AirportCoverageTest::TheCodesOfAnAddonComeFromEveryFileItCarries()
{
    const std::vector<AirportsOfAnAddon> airports =
        AirportsOfEachAddon({AddonAt("two-airports", {Carrying({"EHAM"}), Carrying({"LPMA", "EHAM"})})});

    QCOMPARE(airports.size(), std::size_t{1});
    QCOMPARE(airports.front().evidence, AirportEvidence::TheCodeWasRead);
    QCOMPARE(Listed(airports.front().codes), QStringList({"EHAM", "LPMA"}));
}

void AirportCoverageTest::AnAddonWithNoAirportRecordIsNotAnAirportAndItsNameDoesNotPromoteIt()
{
    const std::vector<AirportsOfAnAddon> airports =
        AirportsOfEachAddon({AddonAt("someone-eham-model-library", {CarryingNoRecord(), CarryingNoRecord()})});

    QCOMPARE(airports.front().evidence, AirportEvidence::ItCarriesNoAirportRecord);
    QVERIFY2(airports.front().codes.empty(),
             "the folder name carries EHAM and the files carry no record, so the addon carries no code");
}

void AirportCoverageTest::AnAddonWhoseIdentifierDidNotDecodeSaysSoInsteadOfSayingItIsNotAnAirport()
{
    const std::vector<AirportsOfAnAddon> airports =
        AirportsOfEachAddon({AddonAt("unread", {CarryingNoRecord(), ThatDidNotDecode()})});

    QCOMPARE(airports.front().evidence, AirportEvidence::ARecordWasNotRead);
    QVERIFY(airports.front().codes.empty());

    const std::vector<AirportsOfAnAddon> readAnyway =
        AirportsOfEachAddon({AddonAt("read-anyway", {Carrying({"EHAM"}), ThatDidNotDecode()})});

    QCOMPARE(readAnyway.front().evidence, AirportEvidence::TheCodeWasRead);
}

void AirportCoverageTest::TheSameAddonReachedByTheLibraryAndByTheLinkCountsOnce()
{
    SceneryOfAnAddon throughTheLibrary = AddonAt("eham", {Carrying({"EHAM"})});
    SceneryOfAnAddon throughTheLink = AddonAt("eham", {Carrying({"EHAM"})});
    throughTheLink.resolvedPath = PathFromUtf8("D:\\Library\\Sceneries\\.\\eham");

    const std::vector<AirportsOfAnAddon> airports = AirportsOfEachAddon({throughTheLibrary, throughTheLink});

    QCOMPARE(airports.size(), std::size_t{1});
    QCOMPARE(Listed(airports.front().codes), QStringList({"EHAM"}));

    QVERIFY2(GroupsOfTheSameAirport(airports).empty(),
             "counting the addon and its junction as two invents a conflict of an addon with itself");
}

void AirportCoverageTest::TwoAddonsOfTheSameAirportMakeAGroup()
{
    const std::vector<AirportGroup> groups = GroupsOfTheSameAirport(AirportsOfEachAddon(
        {AddonAt("one-eham", {Carrying({"EHAM"})}), AddonAt("another-eham", {Carrying({"EHAM", "LPMA"})})}));

    QCOMPARE(groups.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(groups.front().code), QStringLiteral("EHAM"));
    QCOMPARE(groups.front().addons.size(), std::size_t{2});
    QVERIFY2(groups.front().addons.front() == AddonId({.libraryId = kLibrary, .folderName = "one-eham"}),
             "the group is keyed by AddonId, which carries no category, so two addons filed apart still meet here");
}

void AirportCoverageTest::AnAddonWithoutACodeNeverJoinsAGroup()
{
    const std::vector<AirportGroup> groups = GroupsOfTheSameAirport(
        AirportsOfEachAddon({AddonAt("no-record", {CarryingNoRecord()}),
                             AddonAt("also-no-record", {CarryingNoRecord()}), AddonAt("unread", {ThatDidNotDecode()}),
                             AddonAt("also-unread", {ThatDidNotDecode()}), AddonAt("one-eham", {Carrying({"EHAM"})})}));

    QVERIFY2(groups.empty(),
             "an empty code never matches, and the two states that produce no code are both empty here");
}

void AirportCoverageTest::TheSceneryFolderIsRecognisedWhateverTheCaseAndNothingElseIs()
{
    QVERIFY(ItIsTheSceneryFolderOfAnAddon(PathFromUtf8("D:/Library/Sceneries/someone-airport/scenery")));
    QVERIFY(ItIsTheSceneryFolderOfAnAddon(PathFromUtf8("D:/Library/Sceneries/someone-airport/Scenery")));
    QVERIFY(ItIsTheSceneryFolderOfAnAddon(PathFromUtf8("D:/Library/Sceneries/someone-airport/SCENERY")));

    QVERIFY2(!ItIsTheSceneryFolderOfAnAddon(PathFromUtf8("D:/Library/Sceneries")),
             "the category folder of a library is named Sceneries, and it is not the scenery folder of an addon");
    QVERIFY(!ItIsTheSceneryFolderOfAnAddon(PathFromUtf8("D:/Library/Sceneries/someone-airport/scenery-old")));
    QVERIFY(!ItIsTheSceneryFolderOfAnAddon(PathFromUtf8("D:/Library/Sceneries/someone-airport/Contrail")));
}

QTEST_APPLESS_MAIN(AirportCoverageTest)

#include "tst_airport_coverage.moc"
