#include <QtTest/QtTest>

#include <chrono>
#include <map>
#include <ranges>
#include <string>

#include "domain/importing/WhatTheImporterBrought.h"
#include "domain/journal/LinksTheAppMade.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/journal/JournalImportedFolders.h"
#include "infrastructure/journal/JournalLinkedFolders.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class JournalFoldsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheLinkedFoldersFoldOnlyWhatWasAppendedSinceTheLastAnswer();
        static void TheImportedFoldersFoldOnlyWhatWasAppendedSinceTheLastAnswer();
        static void TheImporterFoldedOneRecordAtATimeAnswersLikeTheWholeHistoryAfterEveryPrefix();
    };

    const std::filesystem::path kLibraryCopy = "D:/Library/Utilities/navigraph-nav-base";
    const std::filesystem::path kPlace = "E:/Sim/Community/navigraph-nav-base";

    [[nodiscard]] OperationRecord
    Link(const OperationKind kind, const std::filesystem::path& source, const std::filesystem::path& target)
    {
        return OperationRecord::OfLink(std::chrono::system_clock::time_point{}, kind, AddonId{}, source, target,
                                       LinkFailure::None);
    }

    [[nodiscard]] OperationRecord Imported(const OperationKind kind,
                                           const std::filesystem::path& source,
                                           const std::filesystem::path& target,
                                           const FileResult result = FileResult::Completed)
    {
        return OperationRecord::OfImport(std::chrono::system_clock::time_point{}, kind, AddonId{}, source, target,
                                         result);
    }

    [[nodiscard]] std::vector<std::filesystem::path>
    TheWholeHistoryFoldedFromScratch(const std::vector<OperationRecord>& history)
    {
        std::map<std::string, std::filesystem::path> brought;

        for (const OperationRecord& record : history)
        {
            if (!Succeeded(record.outcome))
            {
                continue;
            }

            const bool arrivedNow = record.kind == OperationKind::ImportMoveIntoPlace;
            const bool movedOn =
                record.kind == OperationKind::MoveAddon && brought.erase(ComparablePath(record.source)) == 1;

            if (arrivedNow || movedOn)
            {
                brought.insert_or_assign(ComparablePath(record.target), record.target);
            }
        }

        std::vector<std::filesystem::path> folders;
        for (const std::filesystem::path& folder : brought | std::views::values)
        {
            folders.push_back(folder);
        }

        return folders;
    }
}

void JournalFoldsTest::TheLinkedFoldersFoldOnlyWhatWasAppendedSinceTheLastAnswer()
{
    FakeOperationJournal journal;
    journal.Append(Link(OperationKind::EnableAddon, kLibraryCopy, kPlace));
    journal.Append(Link(OperationKind::EnableAddon, "D:/Library/Utilities/other", "E:/Sim/Community/other"));
    journal.Append(Link(OperationKind::DisableAddon, "D:/Library/Utilities/other", "E:/Sim/Community/other"));

    const JournalLinkedFolders linked(journal);

    QCOMPARE(linked.WhatTheAppLinked().size(), std::size_t{1});
    QCOMPARE(journal.recordsHandedOut, std::size_t{3});

    journal.Append(Link(OperationKind::EnableAddon, "D:/Library/Utilities/third", "E:/Sim/Community/third"));

    const std::vector<LinkTheAppMade> answer = linked.WhatTheAppLinked();
    const std::vector<LinkTheAppMade> whole = WhereTheAppMadeLinks(journal.appended);

    QCOMPARE(journal.recordsHandedOut, std::size_t{4});
    QCOMPARE(answer.size(), std::size_t{2});
    QCOMPARE(answer.size(), whole.size());

    for (std::size_t index = 0; index < whole.size(); ++index)
    {
        QCOMPARE(answer[index].place, whole[index].place);
        QCOMPARE(answer[index].libraryCopy, whole[index].libraryCopy);
    }

    QCOMPARE(linked.WhatTheAppLinked().size(), std::size_t{2});
    QCOMPARE(journal.recordsHandedOut, std::size_t{4});
}

void JournalFoldsTest::TheImportedFoldersFoldOnlyWhatWasAppendedSinceTheLastAnswer()
{
    FakeOperationJournal journal;
    journal.Append(Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/one", "D:/Library/one"));
    journal.Append(Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/two", "D:/Library/two"));
    journal.Append(Imported(OperationKind::MoveAddon, "D:/Library/two", "D:/Library/Navdata/two"));

    const JournalImportedFolders imported(journal);

    QCOMPARE(imported.WhatTheImporterBrought().size(), std::size_t{2});
    QCOMPARE(journal.recordsHandedOut, std::size_t{3});

    journal.Append(Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/three", "D:/Library/three"));

    const std::vector<std::filesystem::path> answer = imported.WhatTheImporterBrought();
    const std::vector<std::filesystem::path> whole = FoldersTheImporterBrought(journal.appended);

    QCOMPARE(journal.recordsHandedOut, std::size_t{4});
    QCOMPARE(answer.size(), std::size_t{3});
    QCOMPARE(answer.size(), whole.size());

    for (std::size_t index = 0; index < whole.size(); ++index)
    {
        QCOMPARE(answer[index], whole[index]);
    }

    QCOMPARE(imported.WhatTheImporterBrought().size(), std::size_t{3});
    QCOMPARE(journal.recordsHandedOut, std::size_t{4});
}

void JournalFoldsTest::TheImporterFoldedOneRecordAtATimeAnswersLikeTheWholeHistoryAfterEveryPrefix()
{
    const std::vector<OperationRecord> history{
        Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/one", "D:/Library/one"),
        Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/two", "D:/Library/Two",
                 FileResult::VerificationFailed),
        Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/two", "D:/Library/two"),
        Imported(OperationKind::MoveAddon, "D:/Library/ONE", "D:/Library/Navdata/one"),
        Imported(OperationKind::MoveAddon, "D:/Library/never-imported", "D:/Library/Navdata/never-imported"),
        Imported(OperationKind::ImportCopyToStaging, "D:/Origin/four", "D:/Staging/four"),
        Imported(OperationKind::MoveAddon, "D:/Library/Navdata/one", "D:/Library/Archive/one"),
        Imported(OperationKind::ImportMoveIntoPlace, "D:/Staging/one", "d:/library/archive/ONE"),
    };

    FoldersTheImporterBroughtSoFar incremental;
    std::vector<OperationRecord> prefix;

    for (const OperationRecord& record : history)
    {
        incremental.Fold(record);
        prefix.push_back(record);

        const std::vector<std::filesystem::path> expected = TheWholeHistoryFoldedFromScratch(prefix);
        const std::vector<std::filesystem::path> folded = incremental.Folders();
        const std::vector<std::filesystem::path> overTheWhole = FoldersTheImporterBrought(prefix);

        QCOMPARE(folded.size(), expected.size());
        QCOMPARE(overTheWhole.size(), expected.size());

        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            QCOMPARE(folded[index], expected[index]);
            QCOMPARE(overTheWhole[index], expected[index]);
        }
    }
}

QTEST_APPLESS_MAIN(JournalFoldsTest)

#include "tst_journal_folds.moc"
