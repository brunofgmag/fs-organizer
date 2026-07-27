#include <QtTest/QtTest>

#include "domain/journal/JournalEntries.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

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
};

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
        return OperationRecord::OfImport(Moment(), kind, AddonId{"lib-1", folder}, kSource, kTarget, result);
    }

    OperationRecord
    Link(const OperationKind kind, const std::string& folder, const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(Moment(), kind, AddonId{"lib-1", folder}, kTarget, kSource, failure);
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
    const std::vector<JournalEntry> entries = GroupImportRuns(AFinishedImportOf("simbridge"));

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

    const std::vector<JournalEntry> entries = GroupImportRuns(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().steps.size(), std::size_t{2});
    QVERIFY(!entries.front().Succeeded());
}

void JournalEntriesTest::TwoImportsInARowNeverMergeIntoOne()
{
    std::vector<OperationRecord> records = AFinishedImportOf("simbridge");
    const std::vector<OperationRecord> second = AFinishedImportOf("fss-aircraft-727");
    records.insert(records.end(), second.begin(), second.end());

    const std::vector<JournalEntry> entries = GroupImportRuns(records);

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

    const std::vector<JournalEntry> entries = GroupImportRuns(records);

    QCOMPARE(entries.size(), std::size_t{3});
    QVERIFY(!entries.front().IsAnImportRun());
}

void JournalEntriesTest::AnEnableThatFollowsNoCopyIsNotPartOfARun()
{
    const std::vector<OperationRecord> records{
        Step(OperationKind::QuarantineFromDestination, "simbridge"),
        Link(OperationKind::EnableAddon, "simbridge"),
    };

    const std::vector<JournalEntry> entries = GroupImportRuns(records);

    QCOMPARE(entries.size(), std::size_t{2});
}

void JournalEntriesTest::AnEntryFailsWhenAnyOfItsStepsFailed()
{
    std::vector<OperationRecord> records = AFinishedImportOf("simbridge");
    records.back() = Link(OperationKind::EnableAddon, "simbridge", LinkFailure::CouldNotCreateLink);

    const std::vector<JournalEntry> entries = GroupImportRuns(records);

    QCOMPARE(entries.size(), std::size_t{1});
    QVERIFY(!entries.front().Succeeded());
    QVERIFY(!StepSucceeded(entries.front().Last()));
    QVERIFY(StepSucceeded(entries.front().First()));
}

QTEST_APPLESS_MAIN(JournalEntriesTest)

#include "tst_journal_entries.moc"
