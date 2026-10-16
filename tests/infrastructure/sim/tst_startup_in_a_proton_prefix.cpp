#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "application/StartupEditor.h"
#include "application/StartupService.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/fileops/PosixFilesystemProbe.h"
#include "infrastructure/sim/ExeXmlDocument.h"
#include "infrastructure/sim/ExeXmlStartupEntries.h"
#include "infrastructure/sim/ProtonPathMapper.h"
#include "infrastructure/sim/ProtonUserCfgLocations.h"
#include "infrastructure/sim/StartupFileLocations.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ProtonTree.h"

namespace
{
    class StartupInAProtonPrefixTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheStartupFileIsFoundBesideTheUserConfigWhateverItsCase();
        static void AProgramPickedOnTheHostIsWrittenWithTheDriveOfTheSimulator();
        static void TheReportReadsTheEntryBackToTheProgramOnTheHost();
        static void EditingAnEntryOnTheSameProgramKeepsItsPath();
        static void EditingAnEntryKeepsTheLetterItWasWrittenWithWhenAnotherLetterReachesTheSameFolder();
        static void AProgramInsideAnAddonIsWrittenThroughTheLinkInTheCommunityFolder();
        static void AnEntryWhoseProgramIsGoneIsBrokenOnTheHost();
    };

    constexpr auto kRoaming2024 = "drive_c/users/steamuser/AppData/Roaming/Microsoft Flight Simulator 2024";
    constexpr auto kRoaming2020 = "drive_c/users/steamuser/AppData/Roaming/Microsoft Flight Simulator";

    struct Machine
    {
        Machine()
        {
            tree.Drive(prefix2020, 's', tree.SteamRoot() / "steamapps");
            tree.Drive(prefix2024, 's', library2024);
            static_cast<void>(tree.WriteText(prefix2024 / kRoaming2024 / "UserCfg.opt", "{ }"));
            static_cast<void>(tree.WriteText(prefix2024 / kRoaming2024 / "exe.xml", NewStartupDocument()));
            static_cast<void>(tree.WriteText(prefix2020 / kRoaming2020 / "UserCfg.opt", "{ }"));
            static_cast<void>(tree.WriteText(prefix2020 / kRoaming2020 / "EXE.xml", NewStartupDocument()));
            std::filesystem::create_directories(CommunityOf2024());
        }

        [[nodiscard]] std::filesystem::path CommunityOf2024() const
        {
            return prefix2024 / kRoaming2024 / "Packages" / "Community";
        }

        [[nodiscard]] std::filesystem::path Program(const std::string& name) const
        {
            return tree.WriteText(library2024 / "steamapps" / "common" / name / (name + ".exe"), "MZ");
        }

        [[nodiscard]] std::vector<ProtonPrefix> Prefixes() const
        {
            return {{.variant = SimulatorVariant::MSFS2020, .path = prefix2020},
                    {.variant = SimulatorVariant::MSFS2024, .path = prefix2024}};
        }

        [[nodiscard]] SimulatorProfile Profile() const
        {
            SimulatorProfile profile;
            profile.id = "msfs2024";
            profile.variant = SimulatorVariant::MSFS2024;
            profile.destinations = {CommunityOf2024()};
            profile.defaultDestination = CommunityOf2024();

            return profile;
        }

        [[nodiscard]] std::string StartupText() const
        {
            std::ifstream stream(prefix2024 / kRoaming2024 / "exe.xml", std::ios::binary);

            return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }

        ProtonTree tree;
        std::filesystem::path library2024 = tree.Home() / "Jogos" / "SteamLibrary";
        std::filesystem::path prefix2020 = tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2020);
        std::filesystem::path prefix2024 = tree.AddPrefix(library2024, ProtonTree::kMsfs2024);
        PosixFilesystemProbe probe;
        ProtonPathMapper mapper{Prefixes()};
        ExeXmlStartupEntries startupEntries{prefix2024 / kRoaming2024 / "exe.xml"};
        FakeProcessProbe processProbe;
        StartupService service{startupEntries, processProbe, probe, mapper, true};
        FakePresetRepository presets;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        StartupEditor editor{service, presets, probe, mapper, log};
    };

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }
}

void StartupInAProtonPrefixTest::TheStartupFileIsFoundBesideTheUserConfigWhateverItsCase()
{
    const Machine machine;

    const std::vector<StartupFileLocation> found =
        StartupFileLocations(ProtonUserCfgLocations(machine.Prefixes()), machine.probe);

    QCOMPARE(found.size(), std::size_t{2});
    QCOMPARE(found[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(found[0].filePath, machine.prefix2020 / kRoaming2020 / "EXE.xml");
    QCOMPARE(found[1].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(found[1].filePath, machine.prefix2024 / kRoaming2024 / "exe.xml");
}

void StartupInAProtonPrefixTest::AProgramPickedOnTheHostIsWrittenWithTheDriveOfTheSimulator()
{
    Machine machine;
    const std::filesystem::path program = machine.Program("Tool");

    const StartupGestureOutcome outcome =
        machine.editor.Add(machine.Profile(), ProfileSnapshot{}, StartupDraft{.label = "Tool", .file = program});

    QCOMPARE(outcome.result, FileResult::Completed);
    QCOMPARE(AsUtf8(outcome.entryPath), std::string(R"(S:\steamapps\common\Tool\Tool.exe)"));
    QVERIFY(machine.StartupText().find(R"(<Path>S:\steamapps\common\Tool\Tool.exe</Path>)") != std::string::npos);
}

void StartupInAProtonPrefixTest::TheReportReadsTheEntryBackToTheProgramOnTheHost()
{
    Machine machine;
    const std::filesystem::path program = machine.Program("Tool");
    QCOMPARE(
        machine.editor.Add(machine.Profile(), ProfileSnapshot{}, StartupDraft{.label = "Tool", .file = program}).result,
        FileResult::Completed);

    const StartupReport report = machine.service.Report(machine.Profile(), ProfileSnapshot{});

    QCOMPARE(report.lines.size(), std::size_t{1});
    QCOMPARE(AsUtf8(report.lines[0].path), std::string(R"(S:\steamapps\common\Tool\Tool.exe)"));
    QCOMPARE(report.lines[0].hostPath, program);
    QCOMPARE(report.lines[0].condition, StartupCondition::Reachable);
}

void StartupInAProtonPrefixTest::EditingAnEntryOnTheSameProgramKeepsItsPath()
{
    Machine machine;
    const std::filesystem::path program = machine.Program("Tool");
    QCOMPARE(
        machine.editor.Add(machine.Profile(), ProfileSnapshot{}, StartupDraft{.label = "Tool", .file = program}).result,
        FileResult::Completed);
    const std::filesystem::path entry = machine.service.Entries().front().path;

    const StartupDraftCheck check = machine.editor.Check(machine.Profile(), ProfileSnapshot{}, program, entry);

    QVERIFY(!check.changesThePath);
    QCOMPARE(check.pathToWrite, entry);
    QCOMPARE(check.refusal, FileResult::Completed);
}

void StartupInAProtonPrefixTest::EditingAnEntryKeepsTheLetterItWasWrittenWithWhenAnotherLetterReachesTheSameFolder()
{
    Machine machine;
    const std::filesystem::path program = machine.Program("Tool");
    QCOMPARE(
        machine.editor.Add(machine.Profile(), ProfileSnapshot{}, StartupDraft{.label = "Tool", .file = program}).result,
        FileResult::Completed);
    const std::filesystem::path entry = machine.service.Entries().front().path;
    machine.tree.Drive(machine.prefix2024, 'q', machine.library2024);

    QVERIFY(machine.mapper.ToSimulator(SimulatorVariant::MSFS2024, program) != entry);

    const StartupDraftCheck check = machine.editor.Check(machine.Profile(), ProfileSnapshot{}, program, entry);

    QVERIFY(!check.changesThePath);
    QCOMPARE(check.pathToWrite, entry);
}

void StartupInAProtonPrefixTest::AProgramInsideAnAddonIsWrittenThroughTheLinkInTheCommunityFolder()
{
    Machine machine;
    const std::filesystem::path library = machine.tree.Root() / "library";
    const std::filesystem::path addon = library / "Utilities" / "p42-util-flow-pro";
    const std::filesystem::path executable = machine.tree.WriteText(addon / "bin" / "flow.exe", "MZ");

    TreeNode libraryNode;
    libraryNode.kind = TreeNodeKind::Library;
    libraryNode.path = library;
    libraryNode.children.push_back(AddonNode(addon));
    ProfileSnapshot snapshot;
    snapshot.libraries.push_back(libraryNode);

    const StartupDraftCheck check = machine.editor.Check(machine.Profile(), snapshot, executable, std::nullopt);

    QVERIFY(check.insideAnAddon);
    QCOMPARE(AsUtf8(check.pathToWrite),
             std::string(R"(C:\users\steamuser\AppData\Roaming\Microsoft Flight Simulator 2024\Packages\Community)")
                 + R"(\p42-util-flow-pro\bin\flow.exe)");
}

void StartupInAProtonPrefixTest::AnEntryWhoseProgramIsGoneIsBrokenOnTheHost()
{
    Machine machine;
    const std::filesystem::path program = machine.Program("Tool");
    QCOMPARE(
        machine.editor.Add(machine.Profile(), ProfileSnapshot{}, StartupDraft{.label = "Tool", .file = program}).result,
        FileResult::Completed);
    std::filesystem::remove(program);

    const StartupReport report = machine.service.Report(machine.Profile(), ProfileSnapshot{});

    QCOMPARE(report.lines[0].condition, StartupCondition::Broken);
}

QTEST_APPLESS_MAIN(StartupInAProtonPrefixTest)

#include "tst_startup_in_a_proton_prefix.moc"
