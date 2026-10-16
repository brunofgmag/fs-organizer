#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <string>

#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ProtonPathMapper.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/ProtonTree.h"

namespace
{
    class ProtonPathMapperTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheSameLetterLeadsToADifferentPlaceInEachPrefix();
        static void EveryComponentIsFoundWhateverItsCase();
        static void AComponentThatIsNotThereKeepsItsSpellingAndTheRestFollowsIt();
        static void TheExactSpellingWinsOverAnotherCaseOfTheSameName();
        static void ForwardSlashesAreReadLikeBackslashes();
        static void DotsAreResolvedBeforeTheyReachTheDisk();
        static void ADriveLetterIsReadInEitherCase();
        static void ADriveWithoutALinkAndAPathThatIsNotOfWindowsStayAsTheyAre();
        static void ThePrefixOfAnotherSimulatorIsNeverUsed();
        static void TheDriveWhoseTargetIsTheClosestToThePathIsTheOneThatIsWritten();
        static void TheTargetItselfIsTheRootOfItsDrive();
        static void TheRootOfTheHostIsTheZDrive();
        static void ADriveLetterIsWrittenInUpperCaseWithTheNamesOfTheDisk();
        static void AFolderReachedThroughALinkToTheTargetIsStillOnThatDrive();
        static void WhenTwoLettersShareATargetTheFirstLetterIsWritten();
        static void APathThatIsAlreadyOfWindowsOrNotAbsoluteIsLeftAlone();
        static void WhatIsWrittenIsReadBackToTheSameFile();
        static void ASimulatorWithoutAPrefixLeavesEveryPathAlone();
    };

    struct Machine
    {
        Machine()
        {
            prefix2020 = tree.AddPrefix(tree.SteamRoot(), ProtonTree::kMsfs2020);
            prefix2024 = tree.AddPrefix(library2024, ProtonTree::kMsfs2024);
            tree.Drive(prefix2020, 's', tree.SteamRoot() / "steamapps");
            tree.Drive(prefix2024, 's', library2024);
        }

        [[nodiscard]] ProtonPathMapper Mapper() const
        {
            return ProtonPathMapper({{.variant = SimulatorVariant::MSFS2020, .path = prefix2020},
                                     {.variant = SimulatorVariant::MSFS2024, .path = prefix2024}});
        }

        ProtonTree tree;
        std::filesystem::path library2024 = tree.Home() / "Jogos" / "SteamLibrary";
        std::filesystem::path prefix2020;
        std::filesystem::path prefix2024;
    };

    std::filesystem::path Windows(const std::string& text)
    {
        return PathFromUtf8(text);
    }

    std::filesystem::path OnTheZDrive(const std::filesystem::path& host)
    {
        std::string text = AsUtf8(host);
        std::ranges::replace(text, '/', '\\');

        return Windows("Z:" + text);
    }
}

void ProtonPathMapperTest::TheSameLetterLeadsToADifferentPlaceInEachPrefix()
{
    const Machine machine;
    std::filesystem::create_directories(machine.tree.SteamRoot() / "steamapps" / "common" / "Tool");
    std::filesystem::create_directories(machine.library2024 / "steamapps" / "common" / "Tool");
    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2020, Windows(R"(S:\common\Tool)")),
             machine.tree.SteamRoot() / "steamapps" / "common" / "Tool");
    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(S:\steamapps\common\Tool)")),
             machine.library2024 / "steamapps" / "common" / "Tool");
}

void ProtonPathMapperTest::EveryComponentIsFoundWhateverItsCase()
{
    const Machine machine;
    const std::filesystem::path packages = machine.prefix2024 / "drive_c" / "users" / "steamuser" / "AppData"
        / "Roaming" / "Microsoft Flight Simulator 2024" / "Packages";
    std::filesystem::create_directories(packages / "Community");

    const std::filesystem::path host = machine.Mapper().ToHost(
        SimulatorVariant::MSFS2024,
        Windows(R"(C:\USERS\SteamUser\appdata\ROAMING\microsoft flight simulator 2024\packages\COMMUNITY)"));

    QCOMPARE(host, packages / "Community");
}

void ProtonPathMapperTest::AComponentThatIsNotThereKeepsItsSpellingAndTheRestFollowsIt()
{
    const Machine machine;
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "users");

    const std::filesystem::path host =
        machine.Mapper().ToHost(SimulatorVariant::MSFS2024, Windows(R"(C:\Users\New Folder\Sub\tool.exe)"));

    QCOMPARE(host, machine.prefix2024 / "drive_c" / "users" / "New Folder" / "Sub" / "tool.exe");
}

void ProtonPathMapperTest::TheExactSpellingWinsOverAnotherCaseOfTheSameName()
{
    const Machine machine;
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "Tools");
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "tools");

    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(C:\tools)")),
             machine.prefix2024 / "drive_c" / "tools");
    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(C:\Tools)")),
             machine.prefix2024 / "drive_c" / "Tools");
}

void ProtonPathMapperTest::ForwardSlashesAreReadLikeBackslashes()
{
    const Machine machine;
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "Program Files" / "Navigraph");

    QCOMPARE(machine.Mapper().ToHost(SimulatorVariant::MSFS2024, Windows("c:/program files/navigraph")),
             machine.prefix2024 / "drive_c" / "Program Files" / "Navigraph");
}

void ProtonPathMapperTest::DotsAreResolvedBeforeTheyReachTheDisk()
{
    const Machine machine;
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "A" / "B");
    std::filesystem::create_directories(machine.prefix2024 / "drive_c" / "C");

    QCOMPARE(machine.Mapper().ToHost(SimulatorVariant::MSFS2024, Windows(R"(C:\a\.\b\..\..\c)")),
             machine.prefix2024 / "drive_c" / "C");
}

void ProtonPathMapperTest::ADriveLetterIsReadInEitherCase()
{
    const Machine machine;
    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(s:\x)")), machine.library2024 / "x");
    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(S:\x)")), machine.library2024 / "x");
}

void ProtonPathMapperTest::ADriveWithoutALinkAndAPathThatIsNotOfWindowsStayAsTheyAre()
{
    const Machine machine;
    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(X:\Games\tool.exe)")),
             Windows(R"(X:\Games\tool.exe)"));
    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows("/home/pilot/tool.exe")),
             Windows("/home/pilot/tool.exe"));
    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(\\nas\share\tool.exe)")),
             Windows(R"(\\nas\share\tool.exe)"));
    QVERIFY(mapper.ToHost(SimulatorVariant::MSFS2024, {}).empty());
}

void ProtonPathMapperTest::ThePrefixOfAnotherSimulatorIsNeverUsed()
{
    const Machine machine;
    std::filesystem::create_directories(machine.library2024 / "only-here");
    std::filesystem::create_directories(machine.tree.SteamRoot() / "steamapps" / "only-there");

    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2020, Windows(R"(S:\only-here)")),
             machine.tree.SteamRoot() / "steamapps" / "only-here");
    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, Windows(R"(S:\only-there)")),
             machine.library2024 / "only-there");
}

void ProtonPathMapperTest::TheDriveWhoseTargetIsTheClosestToThePathIsTheOneThatIsWritten()
{
    const Machine machine;
    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToSimulator(SimulatorVariant::MSFS2024, machine.library2024 / "steamapps" / "common" / "tool.exe"),
             Windows(R"(S:\steamapps\common\tool.exe)"));
    QCOMPARE(mapper.ToSimulator(SimulatorVariant::MSFS2024,
                                machine.prefix2024 / "drive_c" / "users" / "steamuser" / "tool.exe"),
             Windows(R"(C:\users\steamuser\tool.exe)"));
    QCOMPARE(mapper.ToSimulator(SimulatorVariant::MSFS2020, machine.library2024 / "steamapps" / "tool.exe"),
             OnTheZDrive(machine.library2024 / "steamapps" / "tool.exe"));
}

void ProtonPathMapperTest::TheTargetItselfIsTheRootOfItsDrive()
{
    const Machine machine;

    QCOMPARE(machine.Mapper().ToSimulator(SimulatorVariant::MSFS2024, machine.library2024), Windows(R"(S:\)"));
    QCOMPARE(machine.Mapper().ToSimulator(SimulatorVariant::MSFS2024, machine.prefix2024 / "drive_c"),
             Windows(R"(C:\)"));
}

void ProtonPathMapperTest::TheRootOfTheHostIsTheZDrive()
{
    const Machine machine;

    QCOMPARE(machine.Mapper().ToSimulator(SimulatorVariant::MSFS2024, "/usr/bin/env"), Windows(R"(Z:\usr\bin\env)"));
    QCOMPARE(machine.Mapper().ToSimulator(SimulatorVariant::MSFS2024, "/"), Windows(R"(Z:\)"));
}

void ProtonPathMapperTest::ADriveLetterIsWrittenInUpperCaseWithTheNamesOfTheDisk()
{
    const Machine machine;
    std::filesystem::create_directories(machine.library2024 / "Mixed Case");

    const std::string written = AsUtf8(
        machine.Mapper().ToSimulator(SimulatorVariant::MSFS2024, machine.library2024 / "Mixed Case" / "Tool.EXE"));

    QCOMPARE(written, std::string(R"(S:\Mixed Case\Tool.EXE)"));
}

void ProtonPathMapperTest::AFolderReachedThroughALinkToTheTargetIsStillOnThatDrive()
{
    const Machine machine;
    std::filesystem::create_directories(machine.tree.Home() / ".steam");
    std::filesystem::create_directory_symlink(machine.tree.SteamRoot(), machine.tree.Home() / ".steam" / "steam");

    const std::filesystem::path throughTheLink = machine.tree.Home() / ".steam" / "steam" / "steamapps" / "tool.exe";

    QCOMPARE(machine.Mapper().ToSimulator(SimulatorVariant::MSFS2020, throughTheLink), Windows(R"(S:\tool.exe)"));
}

void ProtonPathMapperTest::WhenTwoLettersShareATargetTheFirstLetterIsWritten()
{
    const Machine machine;
    machine.tree.Drive(machine.prefix2024, 'q', machine.library2024);
    machine.tree.Drive(machine.prefix2024, 'e', machine.library2024);

    QCOMPARE(machine.Mapper().ToSimulator(SimulatorVariant::MSFS2024, machine.library2024 / "tool.exe"),
             Windows(R"(E:\tool.exe)"));
}

void ProtonPathMapperTest::APathThatIsAlreadyOfWindowsOrNotAbsoluteIsLeftAlone()
{
    const Machine machine;
    const ProtonPathMapper mapper = machine.Mapper();

    QCOMPARE(mapper.ToSimulator(SimulatorVariant::MSFS2024, Windows(R"(S:\steamapps\tool.exe)")),
             Windows(R"(S:\steamapps\tool.exe)"));
    QCOMPARE(mapper.ToSimulator(SimulatorVariant::MSFS2024, "relative/tool.exe"),
             std::filesystem::path("relative/tool.exe"));
    QVERIFY(mapper.ToSimulator(SimulatorVariant::MSFS2024, {}).empty());
}

void ProtonPathMapperTest::WhatIsWrittenIsReadBackToTheSameFile()
{
    const Machine machine;
    const std::filesystem::path tool = machine.library2024 / "steamapps" / "common" / "Tool" / "tool.exe";
    std::filesystem::create_directories(tool.parent_path());
    const ProtonPathMapper mapper = machine.Mapper();

    const std::filesystem::path written = mapper.ToSimulator(SimulatorVariant::MSFS2024, tool);

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2024, written), tool);
}

void ProtonPathMapperTest::ASimulatorWithoutAPrefixLeavesEveryPathAlone()
{
    const Machine machine;
    const ProtonPathMapper mapper({{.variant = SimulatorVariant::MSFS2024, .path = machine.prefix2024}});

    QCOMPARE(mapper.ToHost(SimulatorVariant::MSFS2020, Windows(R"(S:\x)")), Windows(R"(S:\x)"));
    QCOMPARE(mapper.ToSimulator(SimulatorVariant::MSFS2020, "/home/pilot/x"), std::filesystem::path("/home/pilot/x"));
}

QTEST_APPLESS_MAIN(ProtonPathMapperTest)

#include "tst_proton_path_mapper.moc"
