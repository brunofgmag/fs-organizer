#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeStartupEntries.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class FakeStartupEntriesTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddedEntryIsAppendedEnabledAndAKnownPathIsRefused();
        static void ARemovedEntryIsKeptAndComesBackBeforeTheOneThatFollowedIt();
        static void ARemovedEntryComesBackAtTheEndWhenItsFollowerIsGone();
        static void RestoringWhatIsNotKeptOrIsBackInTheFileIsRefused();
        static void AnEditInPlaceKeepsNothingAndAMoveKeepsTheOldEntry();
        static void AMoveToThePathOfAnotherEntryIsRefused();
        static void AnEntryTheFileHasAgainIsNotListedAsRemoved();
        static void ForgettingOnlyDropsWhatWasKept();
        static void TheRemovedAreListedNewestFirstWithTheInstantTheyWereRemoved();
        static void OnlySwitchesAreCountedByTheBackupTheyFoundAndOnlyRealChangesAreWrites();
        static void ARefusalAndAThrowApplyToEveryKindOfChange();
    };

    const std::filesystem::path kFirst = PathFromUtf8(R"(C:\Tools\first.exe)");
    const std::filesystem::path kSecond = PathFromUtf8(R"(C:\Tools\second.exe)");
    const std::filesystem::path kThird = PathFromUtf8(R"(C:\Tools\third.exe)");

    [[nodiscard]] FakeStartupEntries ThreeEntries()
    {
        FakeStartupEntries fake;
        fake.Carry(StartupEntry{.label = "First", .path = kFirst, .enabled = true});
        fake.Carry(StartupEntry{.label = "Second", .path = kSecond, .commandLine = "-s", .enabled = false});
        fake.Carry(StartupEntry{.label = "Third", .path = kThird, .enabled = true});

        return fake;
    }

    [[nodiscard]] std::vector<std::string> LabelsOf(const std::vector<StartupEntry>& entries)
    {
        std::vector<std::string> labels;

        for (const StartupEntry& entry : entries)
        {
            labels.push_back(entry.label);
        }

        return labels;
    }

    [[nodiscard]] std::vector<std::string> LabelsOf(const std::vector<StartupRemovedEntry>& removed)
    {
        std::vector<std::string> labels;

        for (const StartupRemovedEntry& entry : removed)
        {
            labels.push_back(entry.entry.label);
        }

        return labels;
    }

    [[nodiscard]] std::chrono::system_clock::time_point Instant(const int seconds)
    {
        return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
    }
}

void FakeStartupEntriesTest::TheRemovedAreListedNewestFirstWithTheInstantTheyWereRemoved()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    QCOMPARE(fake.Apply(StartupRemoval{.path = kSecond, .at = Instant(20)}, backup).result, FileResult::Completed);
    QCOMPARE(fake.Apply(StartupRemoval{.path = kFirst, .at = Instant(30)}, backup).result, FileResult::Completed);
    QCOMPARE(fake.Apply(StartupEditing{.path = kThird,
                                       .label = "Third",
                                       .newPath = PathFromUtf8(R"(D:\Third.exe)"),
                                       .at = Instant(10)},
                        backup)
                 .result,
             FileResult::Completed);

    const std::vector<StartupRemovedEntry> removed = fake.Removed();

    QCOMPARE(LabelsOf(removed), (std::vector<std::string>{"First", "Second", "Third"}));
    QCOMPARE(removed[0].removedAt, Instant(30));
    QCOMPARE(removed[1].removedAt, Instant(20));
    QCOMPARE(removed[2].removedAt, Instant(10));

    FakeStartupEntries tied = ThreeEntries();
    tied.CarryRemoved(StartupEntry{.label = "Old", .path = PathFromUtf8(R"(C:\old.exe)")}, {}, Instant(5));
    tied.CarryRemoved(StartupEntry{.label = "Newer one of the tie", .path = PathFromUtf8(R"(C:\tie.exe)")}, {},
                      Instant(5));

    QCOMPARE(LabelsOf(tied.Removed()), (std::vector<std::string>{"Newer one of the tie", "Old"}));
}

void FakeStartupEntriesTest::AnAddedEntryIsAppendedEnabledAndAKnownPathIsRefused()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    const StartupApplied added = fake.Apply(
        StartupAddition{.label = "Fourth", .path = PathFromUtf8(R"(C:\Tools\fourth.exe)"), .commandLine = "-f"},
        backup);

    QCOMPARE(added.result, FileResult::Completed);
    QVERIFY(!added.was.has_value());

    const std::vector<StartupEntry> entries = fake.Entries();

    QCOMPARE(entries.size(), std::size_t{4});
    QCOMPARE(entries.back().label, std::string("Fourth"));
    QCOMPARE(entries.back().commandLine, std::string("-f"));
    QVERIFY(entries.back().enabled);

    QCOMPARE(
        fake.Apply(StartupAddition{.label = "Again", .path = PathFromUtf8(R"(c:\tools\FIRST.exe)")}, backup).result,
        FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(fake.Entries().size(), std::size_t{4});
}

void FakeStartupEntriesTest::ARemovedEntryIsKeptAndComesBackBeforeTheOneThatFollowedIt()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    const StartupApplied removed = fake.Apply(StartupRemoval{.path = kSecond}, backup);

    QCOMPARE(removed.result, FileResult::Completed);
    QVERIFY(removed.was.has_value());
    QCOMPARE(removed.was->label, std::string("Second"));
    QCOMPARE(removed.was->commandLine, std::string("-s"));
    QVERIFY(!removed.was->enabled);
    QCOMPARE(LabelsOf(fake.Entries()), (std::vector<std::string>{"First", "Third"}));
    QCOMPARE(LabelsOf(fake.Removed()), (std::vector<std::string>{"Second"}));

    QCOMPARE(fake.Apply(StartupRestoring{.path = kSecond}, backup).result, FileResult::Completed);
    QCOMPARE(LabelsOf(fake.Entries()), (std::vector<std::string>{"First", "Second", "Third"}));
    QVERIFY(fake.Removed().empty());
}

void FakeStartupEntriesTest::ARemovedEntryComesBackAtTheEndWhenItsFollowerIsGone()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    QCOMPARE(fake.Apply(StartupRemoval{.path = kSecond}, backup).result, FileResult::Completed);
    QCOMPARE(fake.Apply(StartupRemoval{.path = kThird}, backup).result, FileResult::Completed);
    QCOMPARE(fake.Apply(StartupRestoring{.path = kSecond}, backup).result, FileResult::Completed);

    QCOMPARE(LabelsOf(fake.Entries()), (std::vector<std::string>{"First", "Second"}));
}

void FakeStartupEntriesTest::RestoringWhatIsNotKeptOrIsBackInTheFileIsRefused()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    QCOMPARE(fake.Apply(StartupRestoring{.path = kFirst}, backup).result, FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(fake.Apply(StartupRemoval{.path = PathFromUtf8(R"(C:\Nothing.exe)")}, backup).result,
             FileResult::TheDiskDisagreesWithTheScan);

    QCOMPARE(fake.Apply(StartupRemoval{.path = kFirst}, backup).result, FileResult::Completed);
    QCOMPARE(fake.Apply(StartupAddition{.label = "First again", .path = kFirst}, backup).result, FileResult::Completed);
    QCOMPARE(fake.Apply(StartupRestoring{.path = kFirst}, backup).result, FileResult::TheStartupEntryIsAlreadyThere);
}

void FakeStartupEntriesTest::AnEditInPlaceKeepsNothingAndAMoveKeepsTheOldEntry()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    const StartupApplied relabelled =
        fake.Apply(StartupEditing{.path = kSecond, .label = "Second!", .newPath = kSecond, .commandLine = ""}, backup);

    QCOMPARE(relabelled.result, FileResult::Completed);
    QVERIFY(relabelled.was.has_value());
    QCOMPARE(relabelled.was->label, std::string("Second"));
    QCOMPARE(fake.Entries()[1].label, std::string("Second!"));
    QCOMPARE(fake.Entries()[1].commandLine, std::string());
    QVERIFY(!fake.Entries()[1].enabled);
    QVERIFY(fake.Removed().empty());

    const std::filesystem::path elsewhere = PathFromUtf8(R"(D:\Elsewhere\second.exe)");
    const StartupApplied moved =
        fake.Apply(StartupEditing{.path = kSecond, .label = "Second!", .newPath = elsewhere}, backup);

    QCOMPARE(moved.result, FileResult::Completed);
    QCOMPARE(fake.Entries()[1].path, elsewhere);
    QCOMPARE(LabelsOf(fake.Removed()), (std::vector<std::string>{"Second!"}));
    QCOMPARE(fake.Removed().front().entry.path, kSecond);
}

void FakeStartupEntriesTest::AMoveToThePathOfAnotherEntryIsRefused()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    QCOMPARE(fake.Apply(StartupEditing{.path = kSecond, .label = "Second", .newPath = kThird}, backup).result,
             FileResult::TheStartupEntryIsAlreadyThere);
    QCOMPARE(
        fake.Apply(StartupEditing{.path = PathFromUtf8(R"(C:\Nothing.exe)"), .label = "x", .newPath = kThird}, backup)
            .result,
        FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(fake.Entries()[1].path, kSecond);
    QCOMPARE(fake.writes, std::size_t{0});
}

void FakeStartupEntriesTest::AnEntryTheFileHasAgainIsNotListedAsRemoved()
{
    FakeStartupEntries fake = ThreeEntries();
    fake.CarryRemoved(StartupEntry{.label = "Gone", .path = PathFromUtf8(R"(C:\Tools\gone.exe)"), .enabled = true});
    fake.CarryRemoved(StartupEntry{.label = "Back", .path = kFirst, .enabled = true});

    QCOMPARE(LabelsOf(fake.Removed()), (std::vector<std::string>{"Gone"}));
}

void FakeStartupEntriesTest::ForgettingOnlyDropsWhatWasKept()
{
    FakeStartupEntries fake = ThreeEntries();
    StartupBackup backup;

    QCOMPARE(fake.Apply(StartupRemoval{.path = kSecond}, backup).result, FileResult::Completed);

    const std::size_t writes = fake.writes;

    QCOMPARE(fake.Apply(StartupForgetting{.path = kSecond}, backup).result, FileResult::Completed);
    QVERIFY(fake.Removed().empty());
    QCOMPARE(fake.writes, writes);
    QCOMPARE(fake.Entries().size(), std::size_t{2});
    QCOMPARE(fake.Apply(StartupForgetting{.path = kSecond}, backup).result, FileResult::Completed);
}

void FakeStartupEntriesTest::OnlySwitchesAreCountedByTheBackupTheyFoundAndOnlyRealChangesAreWrites()
{
    FakeStartupEntries fake = ThreeEntries();

    StartupBackup batch;

    QCOMPARE(fake.Switch(kFirst, true, batch), FileResult::Completed);
    QCOMPARE(fake.writes, std::size_t{0});
    QVERIFY(!batch.taken);

    QCOMPARE(fake.Switch(kFirst, false, batch), FileResult::Completed);
    QCOMPARE(fake.writes, std::size_t{1});
    QVERIFY(batch.taken);

    QCOMPARE(
        fake.Apply(StartupAddition{.label = "Fourth", .path = PathFromUtf8(R"(C:\Tools\fourth.exe)")}, batch).result,
        FileResult::Completed);
    QCOMPARE(fake.writes, std::size_t{2});

    QCOMPARE(fake.Switch(kSecond, true, batch), FileResult::Completed);

    QCOMPARE(fake.switchesThatFoundNoBackup, std::size_t{2});
    QCOMPARE(fake.switchesThatFoundTheBackupTaken, std::size_t{1});
    QCOMPARE(fake.writes, std::size_t{3});
    QCOMPARE(fake.reads, std::size_t{0});

    static_cast<void>(fake.Entries());

    QCOMPARE(fake.reads, std::size_t{1});
}

void FakeStartupEntriesTest::ARefusalAndAThrowApplyToEveryKindOfChange()
{
    FakeStartupEntries refusing = ThreeEntries();
    refusing.MakeSwitchingFailWith(FileResult::CouldNotWriteTheStartupFile);

    StartupBackup backup;

    QCOMPARE(refusing.Switch(kFirst, false, backup), FileResult::CouldNotWriteTheStartupFile);
    QCOMPARE(refusing.Apply(StartupRemoval{.path = kFirst}, backup).result, FileResult::CouldNotWriteTheStartupFile);
    QCOMPARE(refusing.Entries().size(), std::size_t{3});
    QCOMPARE(refusing.writes, std::size_t{0});

    FakeStartupEntries throwing = ThreeEntries();
    throwing.MakeSwitchingThrow();

    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                             static_cast<void>(throwing.Apply(StartupRemoval{.path = kFirst}, backup)));
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, static_cast<void>(throwing.Switch(kFirst, false)));
}

QTEST_APPLESS_MAIN(FakeStartupEntriesTest)

#include "tst_fake_startup_entries.moc"
