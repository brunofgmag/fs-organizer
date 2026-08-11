#include <QtTest/QtTest>

#include <string>
#include <vector>

#include "application/CoverageService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakePackageList.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class CoverageServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFeatureIsBornOffAndReadsNothingWhileItIs();
        static void TurnedOnItNamesThePackageThatCoversTheSameAirport();
        static void OnlyTheEntriesTheUserTurnedOffAreOfferedBackForRelighting();
        static void WritingIsRefusedWithTheSimulatorRunningAndReadingIsNot();
        static void WritingIsRefusedWhileTheFeatureIsOff();
        static void TheWarningBetweenTwoAddonsOfTheLibraryDoesNotDependOnTheFile();
    };

    const LibraryId kLibrary = "library-1";

    [[nodiscard]] AddonId Named(const std::string& folderName)
    {
        return {.libraryId = kLibrary, .folderName = folderName};
    }

    [[nodiscard]] SceneryOfAnAddon AddonAt(const std::string& folderName, std::vector<std::string> codes)
    {
        return {.addon = Named(folderName),
                .resolvedPath = PathFromUtf8("D:/Library/Sceneries/" + folderName),
                .files = {{.reading = SceneryReading::Read, .codes = std::move(codes)}}};
    }

    void FillWithTheReferenceList(FakePackageList& packages)
    {
        packages.Carry("fs24-asobo-vcockpits-core", PackageActivation::Activated);
        packages.CarryAnAirport("fs24-asobo-airport-eham-amsterdam", "EHAM", PackageActivation::Activated);
        packages.CarryAnAirport("fs24-asobo-airport-lpma-madeira", "LPMA", PackageActivation::UserDisabled);
        packages.Carry("communityfs20-ag-airport-bgno-nord", PackageActivation::SystemDisabled);
    }
}

void CoverageServiceTest::TheFeatureIsBornOffAndReadsNothingWhileItIs()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    const CoverageService service(packages, processProbe, false);

    QVERIFY2(!service.Managing(), "in a new profile this one is born off, unlike the switch of the startup file");
    QVERIFY(service.TurnedOff().empty());
    QVERIFY(service
                .WhatTheSimulatorAlsoCovers(
                    {{.addon = Named("payware-eham"), .evidence = AirportEvidence::TheCodeWasRead, .codes = {"EHAM"}}})
                .empty());
}

void CoverageServiceTest::TurnedOnItNamesThePackageThatCoversTheSameAirport()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    const CoverageService service(packages, processProbe, true);

    const std::vector<AirportTheSimulatorAlsoCovers> covered = service.WhatTheSimulatorAlsoCovers(
        AirportsOfEachAddon({AddonAt("payware-eham", {"EHAM"}), AddonAt("payware-lpma", {"LPMA"})}));

    QCOMPARE(covered.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(covered.front().packageName), QStringLiteral("fs24-asobo-airport-eham-amsterdam"));
    QVERIFY2(covered.front().addon == Named("payware-eham"),
             "LPMA is shipped too and the user already turned it off, so there is nothing left to warn about");
}

void CoverageServiceTest::OnlyTheEntriesTheUserTurnedOffAreOfferedBackForRelighting()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    const CoverageService service(packages, processProbe, true);

    const std::vector<TurnedOffPackage> turnedOff = service.TurnedOff();

    QCOMPARE(turnedOff.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(turnedOff.front().name), QStringLiteral("fs24-asobo-airport-lpma-madeira"));
    QVERIFY2(QString::fromStdString(turnedOff.front().code) == QStringLiteral("LPMA"),
             "the row carries the code when the app knows it, and what the simulator disabled by itself is not the "
             "user's to light back up");
}

void CoverageServiceTest::WritingIsRefusedWithTheSimulatorRunningAndReadingIsNot()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    FakeProcessProbe processProbe;
    processProbe.ReportTheSimulatorAsRunning();

    CoverageService service(packages, processProbe, true);

    QCOMPARE(service.Switch("fs24-asobo-airport-eham-amsterdam", false), FileResult::TheSimulatorIsRunning);
    QVERIFY(packages.switched.empty());
    QVERIFY(service.RunningSimulator().has_value());
    QVERIFY2(service.TurnedOff().size() == std::size_t{1}, "reading stays allowed while the simulator runs");
}

void CoverageServiceTest::WritingIsRefusedWhileTheFeatureIsOff()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    CoverageService service(packages, processProbe, false);

    QCOMPARE(service.Switch("fs24-asobo-airport-eham-amsterdam", false), FileResult::ThePackageListIsLeftLoose);
    QVERIFY(packages.switched.empty());

    service.Manage(true);

    QCOMPARE(service.Switch("fs24-asobo-airport-eham-amsterdam", false), FileResult::Completed);
    QCOMPARE(packages.switched.size(), std::size_t{1});
}

void CoverageServiceTest::TheWarningBetweenTwoAddonsOfTheLibraryDoesNotDependOnTheFile()
{
    const std::vector<AirportsOfAnAddon> addons =
        AirportsOfEachAddon({AddonAt("one-eham", {"EHAM"}), AddonAt("another-eham", {"EHAM"})});

    QVERIFY2(PairsOfTheSameAirport(addons, {}).size() == std::size_t{1},
             "this axis reads no file of the simulator, so turning the feature off leaves it working");
}

QTEST_APPLESS_MAIN(CoverageServiceTest)

#include "tst_coverage_service.moc"
