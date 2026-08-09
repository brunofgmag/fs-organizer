#include <QtTest/QtTest>

#include "domain/journal/JournalEntries.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class JournalEntriesTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFiveStepsOfOneImportBecomeASingleEntry();
        static void AnImportThatStoppedHalfwayIsAnEntryWithTheStepsItGotThrough();
        static void TwoImportsInARowNeverMergeIntoOne();
        static void LinkOperationsStayOnePerEntry();
        static void AnEnableThatFollowsNoCopyIsNotPartOfARun();
        static void AnEntryFailsWhenAnyOfItsStepsFailed();
        static void ADisableAndAnEnableOverTheSamePlaceBecomeOneSwap();
        static void AnAddonThatLeavesAndComesBackToItsOwnPlaceIsNotASwap();
        static void ADisableAndAnEnableInDifferentPlacesStayTwoEntries();
        static void ASwapThatFailedHalfwaySaysWhereItStopped();
        static void AQuarantineFollowedByARestoreOverThatPlaceStaysTwoEntries();
        static void AQuarantineFollowedByARestoreOverTheOccupantIsOneSwap();
    };
}

namespace
{
    const std::filesystem::path kSource = "E:/Sim/Community/simbridge";
    const std::filesystem::path kTarget = "D:/Library/Utils/simbridge";

    std::chrono::system_clock::time_point Moment()
    {
        return std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000}};
    }

    OperationRecord
    Step(const OperationKind kind, const std::string& folder, const FileResult result = FileResult::Completed)
    {
        return OperationRecord::OfImport(Moment(), kind, AddonId{.libraryId = "lib-1", .folderName = folder}, kSource,
                                         kTarget, result);
    }

    OperationRecord
    Link(const OperationKind kind, const std::string& folder, const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(Moment(), kind, AddonId{.libraryId = "lib-1", .folderName = folder}, kTarget,
                                       kSource, failure);
    }

    std::vector<OperationRecord> AFinishedImportOf(const std::string& folder)
    {
        return {
            Step(OperationKind::ImportCopyToStaging, folder), Step(OperationKind::ImportVerifyStaging, folder),
            Step(OperationKind::ImportMoveIntoPlace, folder), Step(OperationKind::ImportRemoveSource, folder),
            Link(OperationKind::EnableAddon, folder),
        };
    }
}

void JournalEntriesTest::TheFiveStepsOfOneImportBecomeASingleEntry()
{
    const std::vector<JournalEntry> entries = GroupOperations(AFinishedImportOf("simbridge"));

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().steps.size(), std::size_t{5});
    QVERIFY(entries.front().IsAnImportRun());
    QVERIFY(entries.front().Succeeded());
    QCOMPARE(entries.front().First().kind, OperationKind::ImportCopyToStaging);
    QCOMPARE(entries.front().Last().kind, OperationKind::EnableAddon);
}

void JournalEntriesTest::AnImportThatStoppedHalfwayIsAnEntryWithTheStepsItGotThrough()
{
    const std::vector<OperationRecord> records{
        Step(OperationKind::ImportCopyToStaging, "simbridge"),
        Step(OperationKind::ImportVerifyStaging, "simbridge", FileResult::VerificationFailed),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().steps.size(), std::size_t{2});
    QVERIFY(!entries.front().Succeeded());
}

void JournalEntriesTest::TwoImportsInARowNeverMergeIntoOne()
{
    std::vector<OperationRecord> records = AFinishedImportOf("simbridge");
    const std::vector<OperationRecord> second = AFinishedImportOf("fss-aircraft-727");
    records.insert(records.end(), second.begin(), second.end());

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{2});
    QCOMPARE(entries[0].First().addonId.folderName, std::string{"simbridge"});
    QCOMPARE(entries[1].First().addonId.folderName, std::string{"fss-aircraft-727"});
    QCOMPARE(entries[1].steps.size(), std::size_t{5});
}

void JournalEntriesTest::LinkOperationsStayOnePerEntry()
{
    const std::vector<OperationRecord> records{
        Link(OperationKind::EnableAddon, "pmdg-aircraft-77w"),
        Link(OperationKind::DisableAddon, "aerosoft-crj"),
        Link(OperationKind::RemoveBrokenLink, "tlc-bgjn"),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{3});
    QVERIFY(!entries.front().IsAnImportRun());
}

void JournalEntriesTest::AnEnableThatFollowsNoCopyIsNotPartOfARun()
{
    const std::vector<OperationRecord> records{
        Step(OperationKind::QuarantineFromDestination, "simbridge"),
        Link(OperationKind::EnableAddon, "simbridge"),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{2});
}

void JournalEntriesTest::AnEntryFailsWhenAnyOfItsStepsFailed()
{
    std::vector<OperationRecord> records = AFinishedImportOf("simbridge");
    records.back() = Link(OperationKind::EnableAddon, "simbridge", LinkFailure::CouldNotCreateLink);

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QVERIFY(!entries.front().Succeeded());
    QVERIFY(!StepSucceeded(entries.front().Last()));
    QVERIFY(StepSucceeded(entries.front().First()));
}

namespace
{
    OperationRecord LinkAt(const OperationKind kind,
                           const std::string& folder,
                           const std::filesystem::path& linkPath,
                           const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(Moment(), kind, AddonId{.libraryId = "lib-1", .folderName = folder},
                                       std::filesystem::path{"D:/Library/Aircrafts"} / folder, linkPath, failure);
    }

    const std::filesystem::path kPlace = "E:/Sim/Community/pmdg-aircraft-77w";
}

void JournalEntriesTest::ADisableAndAnEnableOverTheSamePlaceBecomeOneSwap()
{
    const std::vector<OperationRecord> records{
        LinkAt(OperationKind::DisableAddon, "pmdg-aircraft-77w", kPlace),
        LinkAt(OperationKind::EnableAddon, "fenix-a320", kPlace),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QVERIFY(entries.front().IsASwap());
    QVERIFY(!entries.front().IsAnImportRun());
    QVERIFY(entries.front().HasSteps());
    QVERIFY(entries.front().Succeeded());
    QCOMPARE(entries.front().First().addonId.folderName, std::string{"pmdg-aircraft-77w"});
    QCOMPARE(entries.front().Last().addonId.folderName, std::string{"fenix-a320"});
}

void JournalEntriesTest::AnAddonThatLeavesAndComesBackToItsOwnPlaceIsNotASwap()
{
    const std::vector<OperationRecord> records{
        LinkAt(OperationKind::DisableAddon, "pmdg-aircraft-77w", kPlace),
        LinkAt(OperationKind::EnableAddon, "pmdg-aircraft-77w", kPlace),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{2});
}

void JournalEntriesTest::ADisableAndAnEnableInDifferentPlacesStayTwoEntries()
{
    const std::vector<OperationRecord> records{
        LinkAt(OperationKind::DisableAddon, "pmdg-aircraft-77w", kPlace),
        LinkAt(OperationKind::EnableAddon, "fenix-a320", "E:/Sim/Community/fenix-a320"),
    };

    QCOMPARE(GroupOperations(records).size(), std::size_t{2});
}

void JournalEntriesTest::ASwapThatFailedHalfwaySaysWhereItStopped()
{
    const std::vector<OperationRecord> records{
        LinkAt(OperationKind::DisableAddon, "pmdg-aircraft-77w", kPlace, LinkFailure::CouldNotRemoveLink),
        LinkAt(OperationKind::EnableAddon, "fenix-a320", kPlace),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QVERIFY(!entries.front().Succeeded());
    QCOMPARE(entries.front().WhereItStopped().addonId.folderName, std::string{"pmdg-aircraft-77w"});
    QCOMPARE(std::get<LinkFailure>(entries.front().WhereItStopped().outcome), LinkFailure::CouldNotRemoveLink);
}

void JournalEntriesTest::AQuarantineFollowedByARestoreOverThatPlaceStaysTwoEntries()
{
    const std::filesystem::path place = "D:/Library/Utils/simbridge";
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";

    const std::vector<OperationRecord> records{
        OperationRecord::OfImport(Moment(), OperationKind::QuarantineFromLibrary,
                                  AddonId{.libraryId = "lib-1", .folderName = "simbridge"}, place, held,
                                  FileResult::Completed),
        OperationRecord::OfImport(Moment(), OperationKind::RestoreFromQuarantine,
                                  AddonId{.libraryId = "lib-1", .folderName = "simbridge"}, held, place,
                                  FileResult::Completed, OriginSource::Sidecar),
    };

    QCOMPARE(GroupOperations(records).size(), std::size_t{2});
}

void JournalEntriesTest::AQuarantineFollowedByARestoreOverTheOccupantIsOneSwap()
{
    const std::filesystem::path place = "D:/Library/Utils/simbridge";
    const std::filesystem::path held = "D:/Library/_fsorganizer-quarantine/simbridge";

    const std::vector<OperationRecord> records{
        OperationRecord::OfImport(Moment(), OperationKind::QuarantineFromLibrary,
                                  AddonId{.libraryId = "lib-1", .folderName = "simbridge"}, place, held,
                                  FileResult::Completed),
        OperationRecord::OfImport(Moment(), OperationKind::RestoreOverTheOccupant,
                                  AddonId{.libraryId = "lib-1", .folderName = "simbridge"}, held, place,
                                  FileResult::Completed, OriginSource::Sidecar),
    };

    const std::vector<JournalEntry> entries = GroupOperations(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().kind, JournalEntryKind::Swap);
    QCOMPARE(entries.front().steps.size(), std::size_t{2});
}

QTEST_APPLESS_MAIN(JournalEntriesTest)

#include "tst_journal_entries.moc"
