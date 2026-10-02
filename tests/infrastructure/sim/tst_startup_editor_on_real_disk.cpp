#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "application/StartupEditor.h"
#include "application/StartupService.h"
#include "domain/model/Preset.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ExeXmlDocument.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/RemovedStartupEntriesFile.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"
#include "tests/support/TempFiles.h"

namespace
{
    class StartupEditorOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AddingToAProfileWithoutTheFileCreatesItBesideUserCfg();
        static void AddingToAProfileWithoutUserCfgCreatesNothing();
        static void RemovingAndUndoingGivesTheBytesBack();
        static void EditingThePathAndUndoingGivesTheBytesAndThePresetsBack();
        static void UndoingAnEditThatMovedThePathLeavesNothingInTheRemovedFile();
        static void TheRemovedListAndTheUndoSurviveOnlyWhereTheyShould();
        static void AProgramPickedWithForwardSlashesIsAddedWithTheSeparatorOfWindows();
        static void AProgramPickedWithForwardSlashesIsEditedInWithTheSeparatorOfWindows();
        static void UndoingAnEditToThePathOfARemovedEntryLeavesThatEntryInTheRemovedList();
        static void UndoingAnAddingOfARemovedPathLeavesTheOriginalBlockKept();
        static void UndoingAnEditThatKeptThePathLeavesAnOlderRemovedRecordAlone();
        static void AnEditThatChangesNothingTakesNeitherTheUndoNorALineOfTheJournal();
        static void RemovingOneOfTwoBlocksWithTheSamePathOffersNoUndo();
    };

    constexpr auto kIFly = R"(E:\Flight Simulator 2024\Community\ifly-aircraft-737max8\Data\Tool\737MAX_Plugin.exe)";
    const std::string kProfileId = "msfs2024";

    [[nodiscard]] std::optional<FileResult> UndoResultOf(StartupEditor& editor)
    {
        const std::optional<StartupGestureOutcome> undone = editor.Undo(kProfileId);

        return undone.has_value() ? std::optional<FileResult>(undone->result) : std::nullopt;
    }

    [[nodiscard]] std::string BytesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        return BytesOf(std::filesystem::path(FSORG_FIXTURES_DIR) / name);
    }

    [[nodiscard]] std::size_t FirstDifference(const std::string& left, const std::string& right)
    {
        const std::size_t shared = std::min(left.size(), right.size());

        for (std::size_t at = 0; at < shared; ++at)
        {
            if (left[at] != right[at])
            {
                return at;
            }
        }

        return left.size() == right.size() ? std::string::npos : shared;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = kProfileId;

        return profile;
    }

    struct Setup
    {
        explicit Setup(const bool withUserCfg = true)
        {
            std::filesystem::create_directories(Folder());

            if (withUserCfg)
            {
                static_cast<void>(files.WriteText("profile/UserCfg.opt", "{ }"));
            }

            startupEntries.KeepRemovedEntriesIn(
                RemovedStartupEntriesFileOf(files.Root() / "removed", SimulatorVariant::MSFS2024));
        }

        [[nodiscard]] std::filesystem::path Folder() const
        {
            return files.Root() / "profile";
        }

        [[nodiscard]] std::filesystem::path StartupFile() const
        {
            return Folder() / "EXE.xml";
        }

        [[nodiscard]] std::filesystem::path Program(const std::string& name) const
        {
            return files.WriteText("programs-" + name, "MZ");
        }

        void PutTheFixtureInPlace() const
        {
            std::ofstream(StartupFile(), std::ios::binary) << Fixture("simulator-exe.xml");
        }

        TempFiles files;
        StdFilesystemProbe probe;
        ExeXmlStartupEntries startupEntries{files.Root() / "profile" / "EXE.xml"};
        FakeProcessProbe processProbe;
        StartupService service{startupEntries, processProbe, probe, true};
        FakePresetRepository presets;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        StartupEditor editor{service, presets, probe, log};
    };

    [[nodiscard]] StartupDraft
    Draft(const std::string& label, const std::filesystem::path& file, const std::string& commandLine = {})
    {
        return StartupDraft{.label = label, .file = file, .commandLine = commandLine};
    }

    [[nodiscard]] std::filesystem::path ForwardSlashed(const std::filesystem::path& path)
    {
        return PathFromUtf8(WithGenericSeparators(AsUtf8(path)));
    }

    [[nodiscard]] std::vector<std::string> PathTextsIn(const std::string& document)
    {
        std::vector<std::string> texts;

        for (std::size_t at = document.find("<Path>"); at != std::string::npos; at = document.find("<Path>", at))
        {
            const std::size_t from = at + std::string("<Path>").size();
            const std::size_t to = document.find("</Path>", from);

            texts.push_back(document.substr(from, to - from));
            at = to;
        }

        return texts;
    }

    [[nodiscard]] std::string
    BlockOf(const std::string& label, const std::filesystem::path& program, const std::string& extra = {})
    {
        std::filesystem::path native = program;
        native.make_preferred();

        return "    <Launch.Addon>\n        <Name>" + label + "</Name>\n" + extra + "        <Path>" + AsUtf8(native)
            + "</Path>\n    </Launch.Addon>\n";
    }

    [[nodiscard]] std::string DocumentOf(const std::string& blocks)
    {
        return "<SimBase.Document Type=\"Launch\" version=\"1,0\">\n" + blocks + "</SimBase.Document>\n";
    }

    void PutInPlace(const Setup& s, const std::string& document)
    {
        std::ofstream(s.StartupFile(), std::ios::binary | std::ios::trunc) << document;
    }
}

void StartupEditorOnRealDiskTest::AddingToAProfileWithoutTheFileCreatesItBesideUserCfg()
{
    Setup s;
    const std::filesystem::path first = s.Program("first.exe");
    const std::filesystem::path second = s.Program("second.exe");

    QVERIFY(!std::filesystem::exists(s.StartupFile()));

    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("First", first, "-a")).result, FileResult::Completed);

    const std::string afterTheFirst = BytesOf(s.StartupFile());
    std::filesystem::path firstInWindowsForm = first;
    firstInWindowsForm.make_preferred();

    const StartupDocumentChange expected = WithStartupEntryAdded(
        NewStartupDocument(), StartupAddition{.label = "First", .path = firstInWindowsForm, .commandLine = "-a"});

    QCOMPARE(FirstDifference(afterTheFirst, expected.document), std::string::npos);
    QCOMPARE(s.service.Entries().size(), std::size_t{1});

    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("Second", second)).result, FileResult::Completed);
    QCOMPARE(s.service.Entries().size(), std::size_t{2});
    QVERIFY(std::filesystem::exists(BackupOfStartupFile(s.StartupFile())));

    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));
    QCOMPARE(FirstDifference(BytesOf(s.StartupFile()), afterTheFirst), std::string::npos);
    QCOMPARE(s.editor.Removed().size(), std::size_t{1});
    QCOMPARE(s.editor.Removed().at(0).entry.label, std::string("Second"));
}

void StartupEditorOnRealDiskTest::AddingToAProfileWithoutUserCfgCreatesNothing()
{
    Setup s(false);
    const std::filesystem::path program = s.Program("first.exe");

    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("First", program)).result,
             FileResult::CouldNotReadTheStartupFile);
    QVERIFY(!std::filesystem::exists(s.StartupFile()));
    QCOMPARE(s.journal.appended.size(), std::size_t{1});
}

void StartupEditorOnRealDiskTest::RemovingAndUndoingGivesTheBytesBack()
{
    Setup s;
    s.PutTheFixtureInPlace();
    const std::string original = Fixture("simulator-exe.xml");
    const std::size_t before = s.service.Entries().size();

    QCOMPARE(s.editor.Remove(kProfileId, PathFromUtf8(kIFly)).result, FileResult::Completed);
    QVERIFY(FirstDifference(BytesOf(s.StartupFile()), original) != std::string::npos);
    QCOMPARE(s.service.Entries().size(), before - 1);
    QCOMPARE(s.editor.Removed().size(), std::size_t{1});

    const std::optional<StartupUndoPlan> plan = s.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RestoresTheRemovedEntry);
    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));

    QCOMPARE(FirstDifference(BytesOf(s.StartupFile()), original), std::string::npos);
    QVERIFY(s.editor.Removed().empty());
    QCOMPARE(s.service.Entries().size(), before);
}

void StartupEditorOnRealDiskTest::EditingThePathAndUndoingGivesTheBytesAndThePresetsBack()
{
    Setup s;
    s.PutTheFixtureInPlace();
    const std::string original = Fixture("simulator-exe.xml");
    const std::filesystem::path moved = s.Program("moved.exe");

    Preset preset;
    preset.name = "Short";
    preset.governsStartup = true;
    preset.startupEntries = {PresetStartupEntry{.path = PathFromUtf8(kIFly), .action = PresetAction::Disable}};
    QVERIFY(s.presets.Save(kProfileId, preset));

    const StartupGestureOutcome edited =
        s.editor.Edit(Profile(), ProfileSnapshot{}, PathFromUtf8(kIFly), Draft("iFly moved", moved, "--new"));

    QCOMPARE(edited.result, FileResult::Completed);
    QCOMPARE(edited.presetsThatFollowed, (std::vector<std::string>{"Short"}));
    QVERIFY(FirstDifference(BytesOf(s.StartupFile()), original) != std::string::npos);
    QCOMPARE(s.presets.Load(kProfileId, "Short")->startupEntries.front().path, moved);

    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));

    QCOMPARE(FirstDifference(BytesOf(s.StartupFile()), original), std::string::npos);
    QCOMPARE(s.presets.Load(kProfileId, "Short")->startupEntries.front().path, PathFromUtf8(kIFly));
    QVERIFY(s.presets.Load(kProfileId, "Short")->startupEntries.front().action == PresetAction::Disable);
}

void StartupEditorOnRealDiskTest::UndoingAnEditThatMovedThePathLeavesNothingInTheRemovedFile()
{
    Setup s;
    s.PutTheFixtureInPlace();
    const RemovedStartupEntriesFile keeping(
        RemovedStartupEntriesFileOf(s.files.Root() / "removed", SimulatorVariant::MSFS2024));

    const StartupGestureOutcome edited =
        s.editor.Edit(Profile(), ProfileSnapshot{}, PathFromUtf8(kIFly), Draft("iFly moved", s.Program("moved.exe")));

    QCOMPARE(edited.result, FileResult::Completed);
    QCOMPARE(keeping.Entries().size(), std::size_t{1});

    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));

    QVERIFY2(keeping.Entries().empty(), "the removed file still keeps what nobody removed");
}

void StartupEditorOnRealDiskTest::TheRemovedListAndTheUndoSurviveOnlyWhereTheyShould()
{
    Setup s;
    s.PutTheFixtureInPlace();

    QCOMPARE(s.editor.Remove(kProfileId, PathFromUtf8(kIFly)).result, FileResult::Completed);

    StartupService otherService{s.startupEntries, s.processProbe, s.probe, true};
    FakePresetRepository otherPresets;
    StartupEditor afterARestart{otherService, otherPresets, s.probe, s.log};

    QCOMPARE(afterARestart.Removed().size(), std::size_t{1});
    QVERIFY2(!afterARestart.WhatUndoWouldDo(kProfileId).has_value(), "the undo is in memory and does not outlive it");
    QCOMPARE(afterARestart.Restore(kProfileId, PathFromUtf8(kIFly)).result, FileResult::Completed);
    QCOMPARE(afterARestart.Removed().size(), std::size_t{0});
}

void StartupEditorOnRealDiskTest::AProgramPickedWithForwardSlashesIsAddedWithTheSeparatorOfWindows()
{
    Setup s;
    const std::filesystem::path picked = ForwardSlashed(s.Program("first.exe"));

    QVERIFY(AsUtf8(picked).find('/') != std::string::npos);
    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("First", picked)).result, FileResult::Completed);

    const std::vector<std::string> written = PathTextsIn(BytesOf(s.StartupFile()));

    QCOMPARE(written.size(), std::size_t{1});
    QVERIFY2(written.front().find('/') == std::string::npos, qPrintable(QString::fromStdString(written.front())));
    QVERIFY(written.front().find('\\') != std::string::npos);
    QCOMPARE(s.service.Entries().front().path, picked);
}

void StartupEditorOnRealDiskTest::AProgramPickedWithForwardSlashesIsEditedInWithTheSeparatorOfWindows()
{
    Setup s;
    const std::filesystem::path first = s.Program("first.exe");
    const std::filesystem::path second = s.Program("second.exe");

    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("First", first)).result, FileResult::Completed);
    QCOMPARE(s.editor.Edit(Profile(), ProfileSnapshot{}, first, Draft("First", ForwardSlashed(second))).result,
             FileResult::Completed);

    const std::vector<std::string> written = PathTextsIn(BytesOf(s.StartupFile()));

    QCOMPARE(written.size(), std::size_t{1});
    QVERIFY2(written.front().find('/') == std::string::npos, qPrintable(QString::fromStdString(written.front())));
}

void StartupEditorOnRealDiskTest::UndoingAnEditToThePathOfARemovedEntryLeavesThatEntryInTheRemovedList()
{
    Setup s;
    const std::filesystem::path a = s.Program("a.exe");
    const std::filesystem::path b = s.Program("b.exe");
    PutInPlace(s, DocumentOf(BlockOf("A", a) + BlockOf("B", b)));

    QCOMPARE(s.editor.Remove(kProfileId, b).result, FileResult::Completed);

    const std::string beforeTheEdit = BytesOf(s.StartupFile());

    QCOMPARE(s.editor.Edit(Profile(), ProfileSnapshot{}, a, Draft("A", b)).result, FileResult::Completed);
    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));

    QCOMPARE(FirstDifference(BytesOf(s.StartupFile()), beforeTheEdit), std::string::npos);

    const std::vector<StartupRemovedEntry> removed = s.editor.Removed();

    QCOMPARE(removed.size(), std::size_t{1});
    QCOMPARE(removed.front().entry.label, std::string("B"));
}

void StartupEditorOnRealDiskTest::UndoingAnAddingOfARemovedPathLeavesTheOriginalBlockKept()
{
    Setup s;
    const std::filesystem::path program = s.Program("p.exe");
    PutInPlace(s, DocumentOf(BlockOf("Original", program, "        <ManualLoad>True</ManualLoad>\n")));
    const RemovedStartupEntriesFile keeping(
        RemovedStartupEntriesFileOf(s.files.Root() / "removed", SimulatorVariant::MSFS2024));

    QCOMPARE(s.editor.Remove(kProfileId, program).result, FileResult::Completed);
    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("Again", program)).result, FileResult::Completed);
    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));

    const std::vector<RemovedStartupEntry> kept = keeping.Entries();

    QCOMPARE(kept.size(), std::size_t{2});
    QCOMPARE(kept.front().label, std::string("Original"));
    QVERIFY(kept.front().block.find("<ManualLoad>True</ManualLoad>") != std::string::npos);
    QCOMPARE(s.editor.Removed().size(), std::size_t{1});
    QCOMPARE(s.editor.Removed().front().entry.label, std::string("Again"));

    QCOMPARE(s.editor.Forget(kProfileId, program).result, FileResult::Completed);
    QCOMPARE(s.editor.Removed().size(), std::size_t{1});
    QCOMPARE(s.editor.Removed().front().entry.label, std::string("Original"));
}

void StartupEditorOnRealDiskTest::UndoingAnEditThatKeptThePathLeavesAnOlderRemovedRecordAlone()
{
    Setup s;
    const std::filesystem::path program = s.Program("p.exe");
    PutInPlace(s, DocumentOf(BlockOf("Original", program)));
    const RemovedStartupEntriesFile keeping(
        RemovedStartupEntriesFileOf(s.files.Root() / "removed", SimulatorVariant::MSFS2024));

    QCOMPARE(s.editor.Remove(kProfileId, program).result, FileResult::Completed);
    QCOMPARE(s.editor.Add(Profile(), ProfileSnapshot{}, Draft("Again", program)).result, FileResult::Completed);
    QCOMPARE(s.editor.Edit(Profile(), ProfileSnapshot{}, program, Draft("Renamed", program)).result,
             FileResult::Completed);
    QCOMPARE(UndoResultOf(s.editor), std::optional<FileResult>(FileResult::Completed));

    QCOMPARE(s.service.Entries().front().label, std::string("Again"));

    const std::vector<RemovedStartupEntry> kept = keeping.Entries();

    QCOMPARE(kept.size(), std::size_t{1});
    QCOMPARE(kept.front().label, std::string("Original"));
}

void StartupEditorOnRealDiskTest::AnEditThatChangesNothingTakesNeitherTheUndoNorALineOfTheJournal()
{
    Setup s;
    s.PutTheFixtureInPlace();
    const std::filesystem::path fenix = PathFromUtf8(R"(C:\Program Files\FenixSim A320\deps\FenixBootstrapper.exe)");

    QCOMPARE(s.editor.Remove(kProfileId, PathFromUtf8(kIFly)).result, FileResult::Completed);

    const std::string bytes = BytesOf(s.StartupFile());
    const std::size_t lines = s.journal.appended.size();

    const StartupGestureOutcome same = s.editor.Edit(Profile(), ProfileSnapshot{}, fenix, Draft("FenixA320", fenix));

    QCOMPARE(same.result, FileResult::Completed);
    QVERIFY(same.changedNothing);
    QCOMPARE(s.journal.appended.size(), lines);
    QCOMPARE(FirstDifference(BytesOf(s.StartupFile()), bytes), std::string::npos);

    const std::optional<StartupUndoPlan> plan = s.editor.WhatUndoWouldDo(kProfileId);

    QVERIFY(plan.has_value());
    QCOMPARE(plan->effect, StartupUndoEffect::RestoresTheRemovedEntry);

    const StartupGestureOutcome renamed = s.editor.Edit(Profile(), ProfileSnapshot{}, fenix, Draft("Fenix", fenix));

    QCOMPARE(renamed.result, FileResult::Completed);
    QVERIFY(!renamed.changedNothing);
    QCOMPARE(s.journal.appended.size(), lines + 1);
    QCOMPARE(s.editor.WhatUndoWouldDo(kProfileId)->effect, StartupUndoEffect::EditsTheEntryBack);
}

void StartupEditorOnRealDiskTest::RemovingOneOfTwoBlocksWithTheSamePathOffersNoUndo()
{
    Setup s;
    const std::filesystem::path program = s.Program("p.exe");
    PutInPlace(s, DocumentOf(BlockOf("First", program) + BlockOf("Second", program)));

    QCOMPARE(s.editor.Remove(kProfileId, program).result, FileResult::Completed);

    QCOMPARE(s.service.Entries().size(), std::size_t{1});
    QVERIFY2(!s.editor.WhatUndoWouldDo(kProfileId).has_value(),
             "the path is still in the file, so restoring would be refused");
}

QTEST_APPLESS_MAIN(StartupEditorOnRealDiskTest)

#include "tst_startup_editor_on_real_disk.moc"
