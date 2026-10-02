#include <QtTest/QtTest>

#include <string>
#include <vector>

#include "application/CoverageService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeOperationJournal.h"
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
        static void ASwitchLeavesOneLineWithThePackageNameAndTheOutcome();
        static void AnActivationLeavesTheLineThatSaysActivated();
        static void ABatchLeavesOneLinePerPackageWithTheOutcomeOfTheBatch();
        static void ARefusedSwitchLeavesItsLineWithTheReason();
        static void AnEmptyBatchLeavesNothing();
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

    struct Journaled
    {
        FakeOperationJournal journal{};
        FakeClock clock{};
        OperationLog log{journal, clock};
    };

    [[nodiscard]] std::vector<std::string> LabelsOf(const FakeOperationJournal& journal)
    {
        std::vector<std::string> labels;

        for (const OperationRecord& record : journal.appended)
        {
            labels.push_back(record.label);
        }

        return labels;
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
    const Journaled journaled;
    const CoverageService service(packages, processProbe, journaled.log, false);

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
    const Journaled journaled;
    const CoverageService service(packages, processProbe, journaled.log, true);

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
    const Journaled journaled;
    const CoverageService service(packages, processProbe, journaled.log, true);

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

    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, true);

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
    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, false);

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

void CoverageServiceTest::ASwitchLeavesOneLineWithThePackageNameAndTheOutcome()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, true);

    QCOMPARE(service.Switch("fs24-asobo-airport-eham-amsterdam", false), FileResult::Completed);
    QCOMPARE(service.Switch("not-in-the-list", false), FileResult::TheDiskDisagreesWithTheScan);

    QCOMPARE(journaled.journal.appended.size(), std::size_t{2});

    const OperationRecord& first = journaled.journal.appended.front();

    QCOMPARE(first.kind, OperationKind::TurnOffTheSimulatorPackage);
    QCOMPARE(first.label, std::string("fs24-asobo-airport-eham-amsterdam"));
    QCOMPARE(std::get<FileResult>(first.outcome), FileResult::Completed);
    QVERIFY(first.addonId.folderName.empty());
    QCOMPARE(first.timestamp, journaled.clock.now);

    const OperationRecord& second = journaled.journal.appended.back();

    QCOMPARE(second.label, std::string("not-in-the-list"));
    QCOMPARE(std::get<FileResult>(second.outcome), FileResult::TheDiskDisagreesWithTheScan);
}

void CoverageServiceTest::AnActivationLeavesTheLineThatSaysActivated()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, true);

    QCOMPARE(service.Switch("fs24-asobo-airport-lpma-madeira", true), FileResult::Completed);

    QCOMPARE(journaled.journal.appended.size(), std::size_t{1});
    QCOMPARE(journaled.journal.appended.front().kind, OperationKind::TurnOnTheSimulatorPackage);
}

void CoverageServiceTest::ABatchLeavesOneLinePerPackageWithTheOutcomeOfTheBatch()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);
    packages.answer = FileResult::CouldNotWriteTheStartupFile;

    const FakeProcessProbe processProbe;
    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, true);

    QCOMPARE(service.SwitchAll({"fs24-asobo-vcockpits-core", "fs24-asobo-airport-eham-amsterdam"}, false),
             FileResult::CouldNotWriteTheStartupFile);

    QCOMPARE(LabelsOf(journaled.journal),
             (std::vector<std::string>{"fs24-asobo-vcockpits-core", "fs24-asobo-airport-eham-amsterdam"}));

    for (const OperationRecord& record : journaled.journal.appended)
    {
        QCOMPARE(record.kind, OperationKind::TurnOffTheSimulatorPackage);
        QCOMPARE(std::get<FileResult>(record.outcome), FileResult::CouldNotWriteTheStartupFile);
    }
}

void CoverageServiceTest::ARefusedSwitchLeavesItsLineWithTheReason()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    FakeProcessProbe processProbe;
    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, false);

    QCOMPARE(service.Switch("fs24-asobo-vcockpits-core", false), FileResult::ThePackageListIsLeftLoose);

    service.Manage(true);
    processProbe.ReportTheSimulatorAsRunning();

    QCOMPARE(service.SwitchAll({"fs24-asobo-vcockpits-core", "fs24-asobo-airport-eham-amsterdam"}, true),
             FileResult::TheSimulatorIsRunning);

    QVERIFY(packages.switched.empty());
    QCOMPARE(journaled.journal.appended.size(), std::size_t{3});
    QCOMPARE(std::get<FileResult>(journaled.journal.appended[0].outcome), FileResult::ThePackageListIsLeftLoose);
    QCOMPARE(std::get<FileResult>(journaled.journal.appended[1].outcome), FileResult::TheSimulatorIsRunning);
    QCOMPARE(std::get<FileResult>(journaled.journal.appended[2].outcome), FileResult::TheSimulatorIsRunning);
}

void CoverageServiceTest::AnEmptyBatchLeavesNothing()
{
    FakePackageList packages;
    FillWithTheReferenceList(packages);

    const FakeProcessProbe processProbe;
    const Journaled journaled;
    CoverageService service(packages, processProbe, journaled.log, true);

    QCOMPARE(service.SwitchAll({}, false), FileResult::Completed);
    QVERIFY(journaled.journal.appended.empty());
}

QTEST_APPLESS_MAIN(CoverageServiceTest)

#include "tst_coverage_service.moc"
