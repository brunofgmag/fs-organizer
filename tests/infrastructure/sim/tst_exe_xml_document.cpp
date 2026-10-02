#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/StartupEntries.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ExeXmlDocument.h"
#include "infrastructure/sim/XmlEscaping.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/Utf16Text.h"

namespace
{
    class ExeXmlDocumentTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryFormOfEntryInTheRealFileIsRead();
        static void TheSwitchOfTheDocumentItselfIsNotAnEntry();
        static void ASwitchWrittenInLowerCaseReadsLikeAnyOther();
        static void TwoEntriesWithTheSameLabelStayTwoEntries();
        static void TheEntryIsFoundByPathEvenWhenItsSwitchComesFirst();
        static void ASwitchWrittenInLowerCaseIsReplacedLikeAnyOther();
        static void CaseInThePathDoesNotDecideWhichEntryIsFound();
        static void AnEntryTheDocumentDoesNotCarryChangesNothing();
        static void TheLabelNeverDecidesWhichEntryIsSwitched();
        static void AnEntryWithNoSwitchGetsOneAndKeepsEverythingElse();
        static void APathWrittenWithAnEscapedAmpersandIsStillFound();
        static void AWindows1252DocumentIsReadByTheLettersItSpells();
        static void AWindows1252DocumentIsSwitchedByTheLettersItSpells();
        static void TheCommandLineIsReadInBothFormsItIsWritten();
        static void AnEntryInsideACommentIsNotAnEntry();
        static void ABytePassedOffAsUtf8NeverMatchesAndNeverThrows();

        static void AnAddedEntryLandsBeforeTheCloseOfTheDocumentInTheStyleOfTheFile_data();
        static void AnAddedEntryLandsBeforeTheCloseOfTheDocumentInTheStyleOfTheFile();
        static void AnAddedEntryWithoutACommandLineHasNoCommandLineElement_data();
        static void AnAddedEntryWithoutACommandLineHasNoCommandLineElement();
        static void AnAddedEntryIsWrittenEscapedAndReadBackTheSame();
        static void AnEntryTheDocumentAlreadyHasIsNotAddedAgain();
        static void ADocumentWithNoEntryGivesTheIndentationOfTheDescrLine();
        static void TheDocumentTheAppCreatesIsTheStyleOfTheRealFileToday();
        static void TextBeyondAsciiIsAddedOnlyWhereTheDeclarationSaysUtf8();

        static void ARemovedEntryTakesItsWholeLinesAndHandsBackItsBlock_data();
        static void ARemovedEntryTakesItsWholeLinesAndHandsBackItsBlock();
        static void TheLastEntryRemovedHasNoOneFollowingIt();
        static void ARestoredEntryComesBackByteForByteWhereItWas_data();
        static void ARestoredEntryComesBackByteForByteWhereItWas();
        static void AnEntryWhoseFollowerIsGoneIsRestoredAtTheEnd();
        static void ABlockRemovedFromOneStyleIsRestoredInTheStyleOfTheOtherFile();
        static void AnEntryThatIsBackInTheDocumentIsNotRestoredTwice();
        static void AWindows1252BlockSurvivesBeingRemovedAndRestored();
        static void ABlockTheDeclaredEncodingCannotHoldIsNotRestored();

        static void AnEditedEntryChangesInPlaceAndKeepsTheOtherElements_data();
        static void AnEditedEntryChangesInPlaceAndKeepsTheOtherElements();
        static void EditingOnlyTheLabelLeavesNothingToRestore();
        static void AnEmptiedCommandLineTakesTheElementOutInBothForms();
        static void ACommandLineWhereThereWasNoneComesAfterThePath();
        static void ASelfClosingCommandLineIsFilledInPlace();
        static void AnEntryWithNoNameGetsOneAsItsFirstChild();
        static void EditingToThePathOfAnotherEntryIsRefused();
        static void AnEditIsWrittenEscaped();
        static void EditingToWhatTheEntryAlreadyHoldsChangesNothing();
        static void AWindows1252DocumentTakesNoNewAccentButKeepsTheOnesItHas();
        static void ANamelessEntryWhoseFirstChildIsTheCommandLineIsNamedAndEmptiedInOneEdit();
        static void AFileWithNoDeclarationCountsAsUtf8OnlyWhenItsBytesAre();

        static void APathWrittenWithANumericCharacterReferenceIsStillFound_data();
        static void APathWrittenWithANumericCharacterReferenceIsStillFound();
        static void AUtf16DocumentWithAByteOrderMarkIsRead_data();
        static void AUtf16DocumentWithAByteOrderMarkIsRead();
        static void UnescapingReadsTheNamedAndTheNumericReferencesAndLeavesWhatIsNotOne_data();
        static void UnescapingReadsTheNamedAndTheNumericReferencesAndLeavesWhatIsNotOne();
    };

    struct Layout
    {
        std::string_view eol;
        std::string_view unit;
        std::string_view fixture;
    };

    constexpr Layout kLfFourSpaces{.eol = "\n", .unit = "    ", .fixture = "simulator-exe.xml"};
    constexpr Layout kCrlfTwoSpaces{.eol = "\r\n", .unit = "  ", .fixture = "installer-exe-crlf.xml"};

    struct Line
    {
        int depth = 0;
        std::string_view text{};
    };

    constexpr std::string_view kRootClose = "</SimBase.Document>";

    constexpr std::string_view kIFlyPath =
        R"(E:\Flight Simulator 2024\Community\ifly-aircraft-737max8\Data\Tool\737MAX_Plugin.exe)";
    constexpr std::string_view kDynamicLodPath =
        R"(C:\Users\pilot\AppData\Roaming\DynamicLOD_ResetEdition\bin\DynamicLOD_ResetEdition.exe)";
    constexpr std::string_view kFenixPath = R"(C:\Program Files\FenixSim A320\deps\FenixBootstrapper.exe)";
    constexpr std::string_view kFsRealisticPath =
        R"(E:\Flight Simulator 2024\Community\rkapps-fsrealistic\service\FSRealistic-Plus.exe)";
    constexpr std::string_view kCommandCenterPath =
        R"(C:\Program Files (x86)\FS2Crew Command Center\FS2CrewCommandCenter.exe)";
    constexpr std::string_view kFlowSharePath =
        R"(E:\Flight Simulator 2024\Community\p42-util-flow-pro\Tools\FlowShare.exe)";

    constexpr std::string_view kPortuguesePathInUtf8 = "C:\\Avi\xC3\xB5"
                                                       "es\\x.exe";

    constexpr std::string_view kWindows1252Document = "<?xml version=\"1.0\" encoding=\"windows-1252\"?>\r\n"
                                                      "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                                      "  <Launch.Addon>\r\n"
                                                      "    <Name>Avi\xF5"
                                                      "es</Name>\r\n"
                                                      "    <Disabled>False</Disabled>\r\n"
                                                      "    <Path>C:\\Avi\xF5"
                                                      "es\\x.exe</Path>\r\n"
                                                      "  </Launch.Addon>\r\n"
                                                      "</SimBase.Document>\r\n";

    constexpr std::string_view kTwoAccentedEntriesIn1252 = "<?xml version=\"1.0\" encoding=\"windows-1252\"?>\r\n"
                                                           "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                                           "  <Launch.Addon>\r\n"
                                                           "    <Name>Avi\xF5"
                                                           "es</Name>\r\n"
                                                           "    <Disabled>False</Disabled>\r\n"
                                                           "    <Path>C:\\Avi\xF5"
                                                           "es\\x.exe</Path>\r\n"
                                                           "    <CommandLine>--m\xE3"
                                                           "o</CommandLine>\r\n"
                                                           "  </Launch.Addon>\r\n"
                                                           "  <Launch.Addon>\r\n"
                                                           "    <Name>Cr\xE9"
                                                           "dito</Name>\r\n"
                                                           "    <Disabled>True</Disabled>\r\n"
                                                           "    <Path>C:\\Cr\xE9"
                                                           "dito\\y.exe</Path>\r\n"
                                                           "  </Launch.Addon>\r\n"
                                                           "</SimBase.Document>\r\n";

    constexpr std::string_view kTwoToolsOfTheSameName = R"(<?xml version='1.0' encoding='utf-8'?>
<SimBase.Document Type="Launch" version="1,0">
    <Disabled>False</Disabled>
    <Launch.Addon>
        <Name>Tool</Name>
        <Disabled>False</Disabled>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
    <Launch.Addon>
        <Name>Tool</Name>
        <Disabled>False</Disabled>
        <Path>C:\Second\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    [[nodiscard]] std::string Fixture(const std::string& name)
    {
        std::ifstream file(std::filesystem::path(FSORG_FIXTURES_DIR) / name, std::ios::binary);

        return std::string(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
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

    void BothStyles()
    {
        QTest::addColumn<int>("which");
        QTest::newRow("LF, no BOM, four spaces") << 0;
        QTest::newRow("CRLF, BOM, two spaces") << 1;
    }

    [[nodiscard]] const Layout& LayoutNumber(const int which)
    {
        return which == 0 ? kLfFourSpaces : kCrlfTwoSpaces;
    }

    [[nodiscard]] std::string Rendered(const Layout& layout, const std::vector<Line>& lines)
    {
        std::string text;

        for (const Line& line : lines)
        {
            for (int level = 0; level < line.depth; ++level)
            {
                text.append(layout.unit);
            }

            text.append(line.text).append(layout.eol);
        }

        return text;
    }

    [[nodiscard]] std::string Replacing(const std::string& document, const std::string& wanted, const std::string& with)
    {
        const std::size_t at = document.find(wanted);

        return at == std::string::npos ? document : std::string(document).replace(at, wanted.size(), with);
    }

    [[nodiscard]] std::string Without(const std::string& document, const std::string& block)
    {
        return Replacing(document, block, {});
    }

    [[nodiscard]] std::string InsertedAtTheEnd(const std::string& document, const std::string& text)
    {
        const std::size_t close = document.rfind(kRootClose);

        return close == std::string::npos ? document : std::string(document).insert(close, text);
    }

    [[nodiscard]] std::string IFlyBlock(const Layout& layout)
    {
        return Rendered(
            layout,
            {{1, "<Launch.Addon>"},
             {2, "<Disabled>False</Disabled>"},
             {2, "<ManualLoad>False</ManualLoad>"},
             {2, "<Name>iFly Plugin</Name>"},
             {2,
              R"(<Path>E:\Flight Simulator 2024\Community\ifly-aircraft-737max8\Data\Tool\737MAX_Plugin.exe</Path>)"},
             {2, "<CommandLine>auto</CommandLine>"},
             {2, "<NewConsole>False</NewConsole>"},
             {1, "</Launch.Addon>"}});
    }

    [[nodiscard]] std::string DynamicLodBlock(const Layout& layout)
    {
        return Rendered(
            layout,
            {{1, "<Launch.Addon>"},
             {2, "<Disabled>True</Disabled>"},
             {2, "<ManualLoad>False</ManualLoad>"},
             {2, "<Name>DynamicLOD_ResetEdition</Name>"},
             {2,
              R"(<Path>C:\Users\pilot\AppData\Roaming\DynamicLOD_ResetEdition\bin\DynamicLOD_ResetEdition.exe</Path>)"},
             {1, "</Launch.Addon>"}});
    }

    [[nodiscard]] std::string FenixBlock(const Layout& layout)
    {
        return Rendered(layout,
                        {{1, "<Launch.Addon>"},
                         {2, "<Name>FenixA320</Name>"},
                         {2, "<Disabled>False</Disabled>"},
                         {2, R"(<Path>C:\Program Files\FenixSim A320\deps\FenixBootstrapper.exe</Path>)"},
                         {1, "</Launch.Addon>"}});
    }

    [[nodiscard]] std::string FsRealisticBlock(const Layout& layout)
    {
        return Rendered(
            layout,
            {{1, "<Launch.Addon>"},
             {2, "<Name>FSRealistic+</Name>"},
             {2, "<Disabled>false</Disabled>"},
             {2, R"(<Path>E:\Flight Simulator 2024\Community\rkapps-fsrealistic\service\FSRealistic-Plus.exe</Path>)"},
             {1, "</Launch.Addon>"}});
    }

    [[nodiscard]] std::string CommandCenterBlock(const Layout& layout, const std::vector<Line>& commandLine)
    {
        std::vector<Line> lines{
            {1, "<Launch.Addon>"},
            {2, "<Name>FS2Crew Command Center</Name>"},
            {2, "<Disabled>True</Disabled>"},
            {2, R"(<Path>C:\Program Files (x86)\FS2Crew Command Center\FS2CrewCommandCenter.exe</Path>)"}};
        lines.insert(lines.end(), commandLine.begin(), commandLine.end());
        lines.push_back({1, "</Launch.Addon>"});

        return Rendered(layout, lines);
    }

    [[nodiscard]] std::filesystem::path PathOf(const std::string_view utf8)
    {
        return PathFromUtf8(std::string(utf8));
    }

    constexpr std::string_view kUtf16Text = "<?xml version=\"1.0\" encoding=\"utf-16\"?>\r\n"
                                            "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                                            "  <Launch.Addon>\r\n"
                                            "    <Name>Avi\xC3\xB5"
                                            "es</Name>\r\n"
                                            "    <Disabled>True</Disabled>\r\n"
                                            "    <Path>C:\\Avi\xC3\xB5"
                                            "es\\x.exe</Path>\r\n"
                                            "    <CommandLine>-a</CommandLine>\r\n"
                                            "  </Launch.Addon>\r\n"
                                            "</SimBase.Document>\r\n";
}

void ExeXmlDocumentTest::EveryFormOfEntryInTheRealFileIsRead()
{
    const std::vector<StartupEntry> entries = StartupEntriesIn(Fixture("simulator-exe.xml"));

    QCOMPARE(entries.size(), std::size_t{21});
    QCOMPARE(entries.front().label, std::string("FenixA320"));
    QCOMPARE(entries.front().path, PathFromUtf8(R"(C:\Program Files\FenixSim A320\deps\FenixBootstrapper.exe)"));
    QVERIFY(entries.front().enabled);
    QCOMPARE(entries[1].label, std::string("Any2GSX"));
    QVERIFY(!entries[1].enabled);
}

void ExeXmlDocumentTest::TheSwitchOfTheDocumentItselfIsNotAnEntry()
{
    constexpr std::string_view nothingIsLaunched = R"(<?xml version='1.0' encoding='utf-8'?>
<SimBase.Document Type="Launch" version="1,0">
    <Descr>Launch</Descr>
    <Filename>EXE.xml</Filename>
    <Disabled>False</Disabled>
    <Launch.ManualLoad>False</Launch.ManualLoad>
</SimBase.Document>
)";

    QVERIFY(StartupEntriesIn(nothingIsLaunched).empty());

    for (const StartupEntry& entry : StartupEntriesIn(Fixture("simulator-exe.xml")))
    {
        QVERIFY(!entry.label.empty());
        QVERIFY(!entry.path.empty());
    }
}

void ExeXmlDocumentTest::ASwitchWrittenInLowerCaseReadsLikeAnyOther()
{
    const std::vector<StartupEntry> entries = StartupEntriesIn(Fixture("simulator-exe.xml"));

    QCOMPARE(entries.back().label, std::string("FSRealistic+"));
    QVERIFY(entries.back().enabled);
}

void ExeXmlDocumentTest::TwoEntriesWithTheSameLabelStayTwoEntries()
{
    const std::vector<StartupEntry> entries = StartupEntriesIn(kTwoToolsOfTheSameName);

    QCOMPARE(entries.size(), std::size_t{2});
    QCOMPARE(entries[0].label, entries[1].label);
    QCOMPARE(entries[0].path, PathFromUtf8(R"(C:\First\tool.exe)"));
    QCOMPARE(entries[1].path, PathFromUtf8(R"(C:\Second\tool.exe)"));
}

void ExeXmlDocumentTest::TheEntryIsFoundByPathEvenWhenItsSwitchComesFirst()
{
    const std::optional<std::string> written = WithStartupEntrySwitched(
        Fixture("simulator-exe.xml"), PathFromUtf8(R"(C:\Users\pilot\AppData\Roaming\Any2GSX\bin\Any2GSX.exe)"), true);

    QVERIFY(written.has_value());
    QCOMPARE(FirstDifference(*written, Fixture("simulator-exe-any2gsx-enabled.xml")), std::string::npos);
}

void ExeXmlDocumentTest::ASwitchWrittenInLowerCaseIsReplacedLikeAnyOther()
{
    const std::optional<std::string> written = WithStartupEntrySwitched(
        Fixture("simulator-exe.xml"),
        PathFromUtf8(R"(E:\Flight Simulator 2024\Community\rkapps-fsrealistic\service\FSRealistic-Plus.exe)"), false);

    QVERIFY(written.has_value());
    QCOMPARE(FirstDifference(*written, Fixture("simulator-exe-fsrealistic-disabled.xml")), std::string::npos);
}

void ExeXmlDocumentTest::CaseInThePathDoesNotDecideWhichEntryIsFound()
{
    const std::optional<std::string> written = WithStartupEntrySwitched(
        Fixture("simulator-exe.xml"),
        PathFromUtf8(R"(e:\FLIGHT SIMULATOR 2024\community\RKAPPS-FSREALISTIC\Service\fsrealistic-plus.EXE)"), false);

    QVERIFY(written.has_value());
    QCOMPARE(FirstDifference(*written, Fixture("simulator-exe-fsrealistic-disabled.xml")), std::string::npos);
}

void ExeXmlDocumentTest::AnEntryTheDocumentDoesNotCarryChangesNothing()
{
    const std::optional<std::string> written = WithStartupEntrySwitched(
        Fixture("simulator-exe.xml"), PathFromUtf8(R"(C:\Nothing\Like\This\was-ever-installed.exe)"), false);

    QVERIFY(!written.has_value());
}

void ExeXmlDocumentTest::TheLabelNeverDecidesWhichEntryIsSwitched()
{
    const std::optional<std::string> written =
        WithStartupEntrySwitched(kTwoToolsOfTheSameName, PathFromUtf8(R"(C:\Second\tool.exe)"), false);

    QVERIFY(written.has_value());

    const std::vector<StartupEntry> entries = StartupEntriesIn(*written);

    QCOMPARE(entries.size(), std::size_t{2});
    QVERIFY(entries[0].enabled);
    QVERIFY(!entries[1].enabled);
}

void ExeXmlDocumentTest::AnEntryWithNoSwitchGetsOneAndKeepsEverythingElse()
{
    constexpr std::string_view withoutASwitch = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <Name>Tool</Name>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    constexpr std::string_view withOne = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <Disabled>True</Disabled>
        <Name>Tool</Name>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    QVERIFY(StartupEntriesIn(withoutASwitch).front().enabled);

    const std::optional<std::string> written =
        WithStartupEntrySwitched(withoutASwitch, PathFromUtf8(R"(C:\First\tool.exe)"), false);

    QVERIFY(written.has_value());
    QCOMPARE(FirstDifference(*written, std::string(withOne)), std::string::npos);
}

void ExeXmlDocumentTest::APathWrittenWithAnEscapedAmpersandIsStillFound()
{
    constexpr std::string_view escaped = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <Name>Tool</Name>
        <Disabled>False</Disabled>
        <Path>C:\A &amp; B\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    QCOMPARE(StartupEntriesIn(escaped).front().path, PathFromUtf8(R"(C:\A & B\tool.exe)"));

    const std::optional<std::string> written =
        WithStartupEntrySwitched(escaped, PathFromUtf8(R"(C:\A & B\tool.exe)"), false);

    QVERIFY(written.has_value());
    QVERIFY(!StartupEntriesIn(*written).front().enabled);
}

void ExeXmlDocumentTest::AWindows1252DocumentIsReadByTheLettersItSpells()
{
    const std::vector<StartupEntry> entries = StartupEntriesIn(kWindows1252Document);

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().label,
             std::string("Avi\xC3\xB5"
                         "es"));
    QCOMPARE(entries.front().path, PathOf(kPortuguesePathInUtf8));
    QVERIFY(entries.front().enabled);
}

void ExeXmlDocumentTest::AWindows1252DocumentIsSwitchedByTheLettersItSpells()
{
    const std::optional<std::string> written =
        WithStartupEntrySwitched(kWindows1252Document, PathOf(kPortuguesePathInUtf8), false);

    QVERIFY(written.has_value());

    std::string expected(kWindows1252Document);
    expected.replace(expected.find("False"), 5, "True");

    QCOMPARE(FirstDifference(*written, expected), std::string::npos);
}

void ExeXmlDocumentTest::TheCommandLineIsReadInBothFormsItIsWritten()
{
    const std::vector<StartupEntry> entries = StartupEntriesIn(Fixture("simulator-exe.xml"));

    const auto commandLineOf = [&entries](const std::string_view path)
    {
        const auto found = std::ranges::find_if(entries,
                                                [path](const StartupEntry& entry)
                                                {
                                                    return entry.path == PathOf(path);
                                                });

        return found == entries.end() ? std::string("<not found>") : found->commandLine;
    };

    QCOMPARE(commandLineOf(kIFlyPath), std::string("auto"));
    QCOMPARE(commandLineOf(kFlowSharePath), std::string());
    QCOMPARE(commandLineOf(kFenixPath), std::string());
    QCOMPARE(commandLineOf(R"(C:\FlightSimLabs\ControlCenter\FSLControlCenter.exe)"), std::string("-NOTIFY"));
}

void ExeXmlDocumentTest::AnEntryInsideACommentIsNotAnEntry()
{
    constexpr std::string_view commentedOut = R"(<SimBase.Document Type="Launch" version="1,0">
    <!--
    <Launch.Addon>
        <Name>Old</Name>
        <Disabled>False</Disabled>
        <Path>C:\Old\tool.exe</Path>
    </Launch.Addon>
    -->
    <Launch.Addon>
        <Name>Live</Name>
        <Disabled>False</Disabled>
        <Path>C:\Live\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    const std::vector<StartupEntry> entries = StartupEntriesIn(commentedOut);

    QCOMPARE(entries.size(), std::size_t{1});
    QVERIFY(!WithStartupEntrySwitched(commentedOut, PathOf(R"(C:\Old\tool.exe)"), false).has_value());
    QCOMPARE(WithStartupEntryRemoved(commentedOut, PathOf(R"(C:\Old\tool.exe)")).result,
             FileResult::TheDiskDisagreesWithTheScan);

    const StartupDocumentChange added =
        WithStartupEntryAdded(commentedOut, StartupAddition{.label = "Old", .path = PathOf(R"(C:\Old\tool.exe)")});

    QCOMPARE(added.result, FileResult::Completed);
    QCOMPARE(StartupEntriesIn(added.document).size(), std::size_t{2});
}

void ExeXmlDocumentTest::ABytePassedOffAsUtf8NeverMatchesAndNeverThrows()
{
    const std::string document = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                 "<SimBase.Document>\n"
                                 "  <Launch.Addon>\n"
                                 "    <Name>Broken</Name>\n"
                                 "    <Path>C:\\Avi\xF5"
                                 "es\\x.exe</Path>\n"
                                 "  </Launch.Addon>\n"
                                 "</SimBase.Document>\n";

    QVERIFY(!WithStartupEntrySwitched(document, PathOf(kPortuguesePathInUtf8), false).has_value());
    QCOMPARE(WithStartupEntryRemoved(document, PathOf(kPortuguesePathInUtf8)).result,
             FileResult::TheDiskDisagreesWithTheScan);
    QCOMPARE(WithStartupEntryAdded(document,
                                   StartupAddition{.label = "Avi\xC3\xB5"
                                                            "es",
                                                   .path = PathOf(kPortuguesePathInUtf8)})
                 .result,
             FileResult::Completed);
}

void ExeXmlDocumentTest::AnAddedEntryLandsBeforeTheCloseOfTheDocumentInTheStyleOfTheFile_data()
{
    BothStyles();
}

void ExeXmlDocumentTest::AnAddedEntryLandsBeforeTheCloseOfTheDocumentInTheStyleOfTheFile()
{
    QFETCH(const int, which);
    const Layout& layout = LayoutNumber(which);
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange change =
        WithStartupEntryAdded(document,
                              StartupAddition{.label = "Little Navmap",
                                              .path = PathOf(R"(C:\Tools\navmap\littlenavmap.exe)"),
                                              .commandLine = "--minimized"});

    const std::string expected = InsertedAtTheEnd(document,
                                                  Rendered(layout,
                                                           {{1, "<Launch.Addon>"},
                                                            {2, "<Name>Little Navmap</Name>"},
                                                            {2, "<Disabled>False</Disabled>"},
                                                            {2, R"(<Path>C:\Tools\navmap\littlenavmap.exe</Path>)"},
                                                            {2, "<CommandLine>--minimized</CommandLine>"},
                                                            {1, "</Launch.Addon>"}}));

    QCOMPARE(change.result, FileResult::Completed);
    QVERIFY(document != expected);
    QCOMPARE(FirstDifference(change.document, expected), std::string::npos);
    QVERIFY(!change.was.has_value());
    QVERIFY(!change.taken.has_value());
}

void ExeXmlDocumentTest::AnAddedEntryWithoutACommandLineHasNoCommandLineElement_data()
{
    BothStyles();
}

void ExeXmlDocumentTest::AnAddedEntryWithoutACommandLineHasNoCommandLineElement()
{
    QFETCH(const int, which);
    const Layout& layout = LayoutNumber(which);
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange change =
        WithStartupEntryAdded(document, StartupAddition{.label = "Tool", .path = PathOf(R"(C:\Tools\tool.exe)")});

    const std::string expected = InsertedAtTheEnd(document,
                                                  Rendered(layout,
                                                           {{1, "<Launch.Addon>"},
                                                            {2, "<Name>Tool</Name>"},
                                                            {2, "<Disabled>False</Disabled>"},
                                                            {2, R"(<Path>C:\Tools\tool.exe</Path>)"},
                                                            {1, "</Launch.Addon>"}}));

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, expected), std::string::npos);
}

void ExeXmlDocumentTest::AnAddedEntryIsWrittenEscapedAndReadBackTheSame()
{
    const Layout& layout = kLfFourSpaces;
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange change = WithStartupEntryAdded(
        document,
        StartupAddition{.label = "Tom & Jerry <Pro>", .path = PathOf(R"(C:\A & B\tool.exe)"), .commandLine = "--a&b"});

    const std::string expected = InsertedAtTheEnd(document,
                                                  Rendered(layout,
                                                           {{1, "<Launch.Addon>"},
                                                            {2, "<Name>Tom &amp; Jerry &lt;Pro&gt;</Name>"},
                                                            {2, "<Disabled>False</Disabled>"},
                                                            {2, R"(<Path>C:\A &amp; B\tool.exe</Path>)"},
                                                            {2, "<CommandLine>--a&amp;b</CommandLine>"},
                                                            {1, "</Launch.Addon>"}}));

    QCOMPARE(FirstDifference(change.document, expected), std::string::npos);

    const StartupEntry added = StartupEntriesIn(change.document).back();

    QCOMPARE(added.label, std::string("Tom & Jerry <Pro>"));
    QCOMPARE(added.path, PathOf(R"(C:\A & B\tool.exe)"));
    QCOMPARE(added.commandLine, std::string("--a&b"));
    QVERIFY(added.enabled);
}

void ExeXmlDocumentTest::AnEntryTheDocumentAlreadyHasIsNotAddedAgain()
{
    const std::string document = Fixture("simulator-exe.xml");

    const StartupDocumentChange change = WithStartupEntryAdded(
        document,
        StartupAddition{
            .label = "Again",
            .path = PathOf(R"(e:\flight simulator 2024\COMMUNITY\rkapps-fsrealistic\service\fsrealistic-plus.exe)")});

    QCOMPARE(change.result, FileResult::TheStartupEntryIsAlreadyThere);
    QVERIFY(change.document.empty());
}

void ExeXmlDocumentTest::ADocumentWithNoEntryGivesTheIndentationOfTheDescrLine()
{
    const std::string empty =
        "<SimBase.Document Type=\"Launch\" version=\"1,0\">\n\t<Descr>Launch</Descr>\n</SimBase.Document>\n";

    const StartupDocumentChange change =
        WithStartupEntryAdded(empty, StartupAddition{.label = "Tool", .path = PathOf(R"(C:\Tools\tool.exe)")});

    const std::string expected = "<SimBase.Document Type=\"Launch\" version=\"1,0\">\n"
                                 "\t<Descr>Launch</Descr>\n"
                                 "\t<Launch.Addon>\n"
                                 "\t\t<Name>Tool</Name>\n"
                                 "\t\t<Disabled>False</Disabled>\n"
                                 "\t\t<Path>C:\\Tools\\tool.exe</Path>\n"
                                 "\t</Launch.Addon>\n"
                                 "</SimBase.Document>\n";

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, expected), std::string::npos);
}

void ExeXmlDocumentTest::TheDocumentTheAppCreatesIsTheStyleOfTheRealFileToday()
{
    const std::string created = NewStartupDocument();

    const std::string header = "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                               "<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n"
                               "  <Descr>Launch</Descr>\r\n"
                               "  <Filename>EXE.xml</Filename>\r\n"
                               "  <Disabled>False</Disabled>\r\n"
                               "  <Launch.ManualLoad>False</Launch.ManualLoad>\r\n";

    QCOMPARE(FirstDifference(created, header + "</SimBase.Document>\r\n"), std::string::npos);
    QVERIFY(StartupEntriesIn(created).empty());

    const StartupDocumentChange change =
        WithStartupEntryAdded(created, StartupAddition{.label = "Tool", .path = PathOf(R"(C:\Tools\tool.exe)")});

    const std::string expected = header
        + "  <Launch.Addon>\r\n"
          "    <Name>Tool</Name>\r\n"
          "    <Disabled>False</Disabled>\r\n"
          "    <Path>C:\\Tools\\tool.exe</Path>\r\n"
          "  </Launch.Addon>\r\n"
          "</SimBase.Document>\r\n";

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, expected), std::string::npos);
}

void ExeXmlDocumentTest::TextBeyondAsciiIsAddedOnlyWhereTheDeclarationSaysUtf8()
{
    const StartupAddition accented{.label = "Avi\xC3\xB5"
                                            "es",
                                   .path = PathOf(kPortuguesePathInUtf8)};

    const StartupDocumentChange into1252 = WithStartupEntryAdded(kWindows1252Document, accented);

    QCOMPARE(into1252.result, FileResult::TheStartupFileIsNotUtf8);
    QVERIFY(into1252.document.empty());

    const std::string plain = "<?xml version=\"1.0\" encoding=\"windows-1252\"?>\n<SimBase.Document>\n  "
                              "<Descr>Launch</Descr>\n</SimBase.Document>\n";

    QCOMPARE(
        WithStartupEntryAdded(plain, StartupAddition{.label = "Tool", .path = PathOf(R"(C:\Tools\tool.exe)")}).result,
        FileResult::Completed);

    for (const char* declaration :
         {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", "<?xml version='1.0' encoding='utf-8'?>\n",
          "\xEF\xBB\xBF<?xml version=\"1.0\"?>\n", ""})
    {
        const std::string document =
            std::string(declaration) + "<SimBase.Document>\n  <Descr>Launch</Descr>\n</SimBase.Document>\n";

        const StartupDocumentChange change = WithStartupEntryAdded(document, accented);

        QCOMPARE(change.result, FileResult::Completed);

        const std::vector<StartupEntry> entries = StartupEntriesIn(change.document);

        QCOMPARE(entries.size(), std::size_t{1});
        QCOMPARE(entries.front().label, accented.label);
        QVERIFY(change.document.find(accented.label) != std::string::npos);
    }
}

void ExeXmlDocumentTest::ARemovedEntryTakesItsWholeLinesAndHandsBackItsBlock_data()
{
    BothStyles();
}

void ExeXmlDocumentTest::ARemovedEntryTakesItsWholeLinesAndHandsBackItsBlock()
{
    QFETCH(const int, which);
    const Layout& layout = LayoutNumber(which);
    const std::string document = Fixture(std::string(layout.fixture));
    const std::string block = IFlyBlock(layout);

    QVERIFY(document.find(block) != std::string::npos);

    const StartupDocumentChange change = WithStartupEntryRemoved(document, PathOf(kIFlyPath));

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, Without(document, block)), std::string::npos);

    QVERIFY(change.taken.has_value());
    QCOMPARE(change.taken->block, block);
    QCOMPARE(change.taken->path, PathOf(kIFlyPath));
    QCOMPARE(change.taken->label, std::string("iFly Plugin"));
    QCOMPARE(change.taken->commandLine, std::string("auto"));
    QVERIFY(change.taken->enabled);
    QCOMPARE(change.taken->followedBy, PathOf(kDynamicLodPath));

    QVERIFY(change.was.has_value());
    QCOMPARE(change.was->label, std::string("iFly Plugin"));
    QCOMPARE(change.was->path, PathOf(kIFlyPath));
    QCOMPARE(change.was->commandLine, std::string("auto"));
}

void ExeXmlDocumentTest::TheLastEntryRemovedHasNoOneFollowingIt()
{
    const StartupDocumentChange change =
        WithStartupEntryRemoved(Fixture("simulator-exe.xml"), PathOf(kFsRealisticPath));

    QCOMPARE(change.result, FileResult::Completed);
    QVERIFY(change.taken.has_value());
    QVERIFY(change.taken->followedBy.empty());
    QCOMPARE(change.taken->block, FsRealisticBlock(kLfFourSpaces));
    QVERIFY(change.taken->enabled);

    QCOMPARE(WithStartupEntryRemoved(Fixture("simulator-exe.xml"), PathOf(R"(C:\Nothing\here.exe)")).result,
             FileResult::TheDiskDisagreesWithTheScan);
}

void ExeXmlDocumentTest::ARestoredEntryComesBackByteForByteWhereItWas_data()
{
    BothStyles();
}

void ExeXmlDocumentTest::ARestoredEntryComesBackByteForByteWhereItWas()
{
    QFETCH(const int, which);
    const std::string document = Fixture(std::string(LayoutNumber(which).fixture));

    for (const std::string_view path : {kFenixPath, kIFlyPath, kFsRealisticPath})
    {
        const StartupDocumentChange removed = WithStartupEntryRemoved(document, PathOf(path));

        QVERIFY(removed.taken.has_value());
        QVERIFY(removed.document != document);

        const StartupDocumentChange restored = WithStartupEntryRestored(removed.document, *removed.taken);

        QCOMPARE(restored.result, FileResult::Completed);
        QCOMPARE(FirstDifference(restored.document, document), std::string::npos);
    }
}

void ExeXmlDocumentTest::AnEntryWhoseFollowerIsGoneIsRestoredAtTheEnd()
{
    const Layout& layout = kCrlfTwoSpaces;
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange removed = WithStartupEntryRemoved(document, PathOf(kIFlyPath));
    const StartupDocumentChange followerGone = WithStartupEntryRemoved(removed.document, PathOf(kDynamicLodPath));

    QVERIFY(removed.taken.has_value());
    QCOMPARE(followerGone.result, FileResult::Completed);

    const StartupDocumentChange restored = WithStartupEntryRestored(followerGone.document, *removed.taken);

    const std::string expected =
        InsertedAtTheEnd(Without(Without(document, IFlyBlock(layout)), DynamicLodBlock(layout)), IFlyBlock(layout));

    QCOMPARE(restored.result, FileResult::Completed);
    QCOMPARE(FirstDifference(restored.document, expected), std::string::npos);
}

void ExeXmlDocumentTest::ABlockRemovedFromOneStyleIsRestoredInTheStyleOfTheOtherFile()
{
    const std::string lf = Fixture(std::string(kLfFourSpaces.fixture));
    const std::string crlf = Fixture(std::string(kCrlfTwoSpaces.fixture));

    const StartupDocumentChange takenFromLf = WithStartupEntryRemoved(lf, PathOf(kIFlyPath));
    const StartupDocumentChange takenFromCrlf = WithStartupEntryRemoved(crlf, PathOf(kIFlyPath));

    QVERIFY(takenFromLf.taken.has_value());
    QVERIFY(takenFromCrlf.taken.has_value());

    const StartupDocumentChange intoCrlf = WithStartupEntryRestored(takenFromCrlf.document, *takenFromLf.taken);
    const StartupDocumentChange intoLf = WithStartupEntryRestored(takenFromLf.document, *takenFromCrlf.taken);

    QCOMPARE(intoCrlf.result, FileResult::Completed);
    QCOMPARE(FirstDifference(intoCrlf.document, crlf), std::string::npos);
    QCOMPARE(intoLf.result, FileResult::Completed);
    QCOMPARE(FirstDifference(intoLf.document, lf), std::string::npos);
}

void ExeXmlDocumentTest::AnEntryThatIsBackInTheDocumentIsNotRestoredTwice()
{
    const std::string document = Fixture("simulator-exe.xml");

    const StartupDocumentChange removed = WithStartupEntryRemoved(document, PathOf(kIFlyPath));

    QVERIFY(removed.taken.has_value());

    const StartupDocumentChange back = WithStartupEntryRestored(removed.document, *removed.taken);
    const StartupDocumentChange twice = WithStartupEntryRestored(back.document, *removed.taken);

    QCOMPARE(twice.result, FileResult::TheStartupEntryIsAlreadyThere);
    QVERIFY(twice.document.empty());
}

void ExeXmlDocumentTest::AWindows1252BlockSurvivesBeingRemovedAndRestored()
{
    const std::string document(kTwoAccentedEntriesIn1252);

    const StartupDocumentChange removed = WithStartupEntryRemoved(document, PathOf(kPortuguesePathInUtf8));

    QCOMPARE(removed.result, FileResult::Completed);
    QVERIFY(removed.taken.has_value());
    QCOMPARE(removed.taken->label,
             std::string("Avi\xC3\xB5"
                         "es"));
    QCOMPARE(removed.taken->commandLine,
             std::string("--m\xC3\xA3"
                         "o"));
    QVERIFY(removed.taken->block.find("Avi\xC3\xB5"
                                      "es")
            != std::string::npos);
    QVERIFY(removed.taken->block.find('\xF5') == std::string::npos);
    QCOMPARE(StartupEntriesIn(removed.document).size(), std::size_t{1});

    const StartupDocumentChange restored = WithStartupEntryRestored(removed.document, *removed.taken);

    QCOMPARE(restored.result, FileResult::Completed);
    QCOMPARE(FirstDifference(restored.document, document), std::string::npos);
}

void ExeXmlDocumentTest::ABlockTheDeclaredEncodingCannotHoldIsNotRestored()
{
    const RemovedStartupEntry greek{.label = "Omega",
                                    .path = PathOf(R"(C:\Omega\tool.exe)"),
                                    .block = "  <Launch.Addon>\r\n    <Name>\xCE\xA9</Name>\r\n    "
                                             "<Path>C:\\Omega\\tool.exe</Path>\r\n  </Launch.Addon>\r\n"};

    const StartupDocumentChange change = WithStartupEntryRestored(kWindows1252Document, greek);

    QCOMPARE(change.result, FileResult::TheStartupFileIsNotUtf8);
    QVERIFY(change.document.empty());
}

void ExeXmlDocumentTest::AnEditedEntryChangesInPlaceAndKeepsTheOtherElements_data()
{
    BothStyles();
}

void ExeXmlDocumentTest::AnEditedEntryChangesInPlaceAndKeepsTheOtherElements()
{
    QFETCH(const int, which);
    const Layout& layout = LayoutNumber(which);
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange change =
        WithStartupEntryEdited(document,
                               StartupEditing{.path = PathOf(kIFlyPath),
                                              .label = "iFly 737 Plugin",
                                              .newPath = PathOf(R"(E:\Tools\iFly\737.exe)"),
                                              .commandLine = "auto --fast"});

    const std::string edited = Rendered(layout,
                                        {{1, "<Launch.Addon>"},
                                         {2, "<Disabled>False</Disabled>"},
                                         {2, "<ManualLoad>False</ManualLoad>"},
                                         {2, "<Name>iFly 737 Plugin</Name>"},
                                         {2, R"(<Path>E:\Tools\iFly\737.exe</Path>)"},
                                         {2, "<CommandLine>auto --fast</CommandLine>"},
                                         {2, "<NewConsole>False</NewConsole>"},
                                         {1, "</Launch.Addon>"}});

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, Replacing(document, IFlyBlock(layout), edited)), std::string::npos);

    QVERIFY(change.was.has_value());
    QCOMPARE(change.was->label, std::string("iFly Plugin"));
    QCOMPARE(change.was->path, PathOf(kIFlyPath));
    QCOMPARE(change.was->commandLine, std::string("auto"));

    QVERIFY(change.taken.has_value());
    QCOMPARE(change.taken->block, IFlyBlock(layout));
    QCOMPARE(change.taken->path, PathOf(kIFlyPath));
    QCOMPARE(change.taken->followedBy, PathOf(kDynamicLodPath));

    const StartupDocumentChange back = WithStartupEntryRestored(change.document, *change.taken);

    QCOMPARE(back.result, FileResult::Completed);
    QCOMPARE(StartupEntriesIn(back.document).size(), std::size_t{22});
}

void ExeXmlDocumentTest::EditingOnlyTheLabelLeavesNothingToRestore()
{
    const Layout& layout = kLfFourSpaces;
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange change = WithStartupEntryEdited(
        document,
        StartupEditing{.path = PathOf(kIFlyPath),
                       .label = "iFly",
                       .newPath = PathOf(
                           R"(e:\flight simulator 2024\community\ifly-aircraft-737max8\data\tool\737max_plugin.exe)"),
                       .commandLine = "auto"});

    QCOMPARE(change.result, FileResult::Completed);
    QVERIFY(!change.taken.has_value());
    QVERIFY(change.document.find("<Name>iFly</Name>") != std::string::npos);
    QVERIFY(change.document.find(
                R"(<Path>e:\flight simulator 2024\community\ifly-aircraft-737max8\data\tool\737max_plugin.exe</Path>)")
            != std::string::npos);
    QCOMPARE(change.document.size(), document.size() - std::string("iFly Plugin").size() + std::string("iFly").size());
}

void ExeXmlDocumentTest::AnEmptiedCommandLineTakesTheElementOutInBothForms()
{
    for (const Layout& layout : {kLfFourSpaces, kCrlfTwoSpaces})
    {
        const std::string document = Fixture(std::string(layout.fixture));

        const StartupDocumentChange full = WithStartupEntryEdited(
            document,
            StartupEditing{
                .path = PathOf(kIFlyPath), .label = "iFly Plugin", .newPath = PathOf(kIFlyPath), .commandLine = ""});

        const std::string withoutTheLine = Rendered(
            layout,
            {{1, "<Launch.Addon>"},
             {2, "<Disabled>False</Disabled>"},
             {2, "<ManualLoad>False</ManualLoad>"},
             {2, "<Name>iFly Plugin</Name>"},
             {2,
              R"(<Path>E:\Flight Simulator 2024\Community\ifly-aircraft-737max8\Data\Tool\737MAX_Plugin.exe</Path>)"},
             {2, "<NewConsole>False</NewConsole>"},
             {1, "</Launch.Addon>"}});

        QCOMPARE(full.result, FileResult::Completed);
        QCOMPARE(FirstDifference(full.document, Replacing(document, IFlyBlock(layout), withoutTheLine)),
                 std::string::npos);
        QVERIFY(!full.taken.has_value());

        const StartupDocumentChange selfClosing =
            WithStartupEntryEdited(document,
                                   StartupEditing{.path = PathOf(kCommandCenterPath),
                                                  .label = "FS2Crew Command Center",
                                                  .newPath = PathOf(kCommandCenterPath),
                                                  .commandLine = ""});

        QCOMPARE(selfClosing.result, FileResult::Completed);
        QCOMPARE(FirstDifference(selfClosing.document,
                                 Replacing(document, CommandCenterBlock(layout, {{2, "<CommandLine/>"}}),
                                           CommandCenterBlock(layout, {}))),
                 std::string::npos);
    }
}

void ExeXmlDocumentTest::ACommandLineWhereThereWasNoneComesAfterThePath()
{
    for (const Layout& layout : {kLfFourSpaces, kCrlfTwoSpaces})
    {
        const std::string document = Fixture(std::string(layout.fixture));

        const StartupDocumentChange change = WithStartupEntryEdited(document,
                                                                    StartupEditing{.path = PathOf(kFenixPath),
                                                                                   .label = "FenixA320",
                                                                                   .newPath = PathOf(kFenixPath),
                                                                                   .commandLine = "-x & -y"});

        const std::string withIt =
            Rendered(layout,
                     {{1, "<Launch.Addon>"},
                      {2, "<Name>FenixA320</Name>"},
                      {2, "<Disabled>False</Disabled>"},
                      {2, R"(<Path>C:\Program Files\FenixSim A320\deps\FenixBootstrapper.exe</Path>)"},
                      {2, "<CommandLine>-x &amp; -y</CommandLine>"},
                      {1, "</Launch.Addon>"}});

        QCOMPARE(change.result, FileResult::Completed);
        QCOMPARE(FirstDifference(change.document, Replacing(document, FenixBlock(layout), withIt)), std::string::npos);
    }
}

void ExeXmlDocumentTest::ASelfClosingCommandLineIsFilledInPlace()
{
    const Layout& layout = kLfFourSpaces;
    const std::string document = Fixture(std::string(layout.fixture));

    const StartupDocumentChange change = WithStartupEntryEdited(document,
                                                                StartupEditing{.path = PathOf(kCommandCenterPath),
                                                                               .label = "FS2Crew Command Center",
                                                                               .newPath = PathOf(kCommandCenterPath),
                                                                               .commandLine = "--quiet"});

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document,
                             Replacing(document, CommandCenterBlock(layout, {{2, "<CommandLine/>"}}),
                                       CommandCenterBlock(layout, {{2, "<CommandLine>--quiet</CommandLine>"}}))),
             std::string::npos);
}

void ExeXmlDocumentTest::AnEntryWithNoNameGetsOneAsItsFirstChild()
{
    constexpr std::string_view nameless = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <Disabled>True</Disabled>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    constexpr std::string_view named = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <Name>Tool</Name>
        <Disabled>True</Disabled>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    const StartupDocumentChange change = WithStartupEntryEdited(
        nameless,
        StartupEditing{
            .path = PathOf(R"(C:\First\tool.exe)"), .label = "Tool", .newPath = PathOf(R"(C:\First\tool.exe)")});

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, std::string(named)), std::string::npos);
}

void ExeXmlDocumentTest::EditingToThePathOfAnotherEntryIsRefused()
{
    const std::string document = Fixture("simulator-exe.xml");

    const StartupDocumentChange change = WithStartupEntryEdited(document,
                                                                StartupEditing{.path = PathOf(kIFlyPath),
                                                                               .label = "iFly Plugin",
                                                                               .newPath = PathOf(kFsRealisticPath),
                                                                               .commandLine = "auto"});

    QCOMPARE(change.result, FileResult::TheStartupEntryIsAlreadyThere);

    const StartupDocumentChange missing = WithStartupEntryEdited(
        document,
        StartupEditing{
            .path = PathOf(R"(C:\Nothing\here.exe)"), .label = "x", .newPath = PathOf(R"(C:\Nothing\there.exe)")});

    QCOMPARE(missing.result, FileResult::TheDiskDisagreesWithTheScan);
}

void ExeXmlDocumentTest::AnEditIsWrittenEscaped()
{
    const std::string document = Fixture("simulator-exe.xml");

    const StartupDocumentChange change =
        WithStartupEntryEdited(document,
                               StartupEditing{.path = PathOf(kFenixPath),
                                              .label = "A & B <x>",
                                              .newPath = PathOf(R"(C:\A & B\fenix.exe)"),
                                              .commandLine = "--a&b"});

    QCOMPARE(change.result, FileResult::Completed);
    QVERIFY(change.document.find("<Name>A &amp; B &lt;x&gt;</Name>") != std::string::npos);
    QVERIFY(change.document.find(R"(<Path>C:\A &amp; B\fenix.exe</Path>)") != std::string::npos);
    QVERIFY(change.document.find("<CommandLine>--a&amp;b</CommandLine>") != std::string::npos);

    const StartupEntry first = StartupEntriesIn(change.document).front();

    QCOMPARE(first.label, std::string("A & B <x>"));
    QCOMPARE(first.path, PathOf(R"(C:\A & B\fenix.exe)"));
    QCOMPARE(first.commandLine, std::string("--a&b"));
}

void ExeXmlDocumentTest::EditingToWhatTheEntryAlreadyHoldsChangesNothing()
{
    const std::string document = Fixture("simulator-exe.xml");

    const StartupDocumentChange change = WithStartupEntryEdited(
        document,
        StartupEditing{
            .path = PathOf(kIFlyPath), .label = "iFly Plugin", .newPath = PathOf(kIFlyPath), .commandLine = "auto"});

    QCOMPARE(change.result, FileResult::Completed);
    QCOMPARE(FirstDifference(change.document, document), std::string::npos);
    QVERIFY(!change.taken.has_value());
}

void ExeXmlDocumentTest::AWindows1252DocumentTakesNoNewAccentButKeepsTheOnesItHas()
{
    const std::string document(kTwoAccentedEntriesIn1252);

    const StartupDocumentChange refused =
        WithStartupEntryEdited(document,
                               StartupEditing{.path = PathOf(kPortuguesePathInUtf8),
                                              .label = "Avi\xC3\xB5"
                                                       "es",
                                              .newPath = PathOf(kPortuguesePathInUtf8),
                                              .commandLine = "--n\xC3\xA3"
                                                             "o"});

    QCOMPARE(refused.result, FileResult::TheStartupFileIsNotUtf8);

    const StartupDocumentChange kept = WithStartupEntryEdited(document,
                                                              StartupEditing{.path = PathOf(kPortuguesePathInUtf8),
                                                                             .label = "Avi\xC3\xB5"
                                                                                      "es",
                                                                             .newPath = PathOf(kPortuguesePathInUtf8),
                                                                             .commandLine = "--plain"});

    std::string expected = document;
    expected.replace(expected.find("--m\xE3"
                                   "o"),
                     5, "--plain");

    QCOMPARE(kept.result, FileResult::Completed);
    QCOMPARE(FirstDifference(kept.document, expected), std::string::npos);
}

void ExeXmlDocumentTest::ANamelessEntryWhoseFirstChildIsTheCommandLineIsNamedAndEmptiedInOneEdit()
{
    constexpr std::string_view onSeparateLines = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <CommandLine>-x</CommandLine>
        <Disabled>True</Disabled>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    constexpr std::string_view namedAndEmptied = R"(<SimBase.Document Type="Launch" version="1,0">
    <Launch.Addon>
        <Name>Tool</Name>
        <Disabled>True</Disabled>
        <Path>C:\First\tool.exe</Path>
    </Launch.Addon>
</SimBase.Document>
)";

    constexpr std::string_view onOneLine =
        R"(<SimBase.Document><Launch.Addon><CommandLine>-x</CommandLine><Path>C:\First\tool.exe</Path></Launch.Addon></SimBase.Document>)";

    constexpr std::string_view namedOnOneLine =
        R"(<SimBase.Document><Launch.Addon><Name>Tool</Name><Path>C:\First\tool.exe</Path></Launch.Addon></SimBase.Document>)";

    const std::filesystem::path path = PathOf(R"(C:\First\tool.exe)");

    const StartupDocumentChange apart = WithStartupEntryEdited(
        onSeparateLines, StartupEditing{.path = path, .label = "Tool", .newPath = path, .commandLine = ""});

    QCOMPARE(apart.result, FileResult::Completed);
    QCOMPARE(apart.document, std::string(namedAndEmptied));

    const StartupDocumentChange together = WithStartupEntryEdited(
        onOneLine, StartupEditing{.path = path, .label = "Tool", .newPath = path, .commandLine = ""});

    QCOMPARE(together.result, FileResult::Completed);
    QCOMPARE(together.document, std::string(namedOnOneLine));

    for (const StartupDocumentChange* change : {&apart, &together})
    {
        const std::vector<StartupEntry> entries = StartupEntriesIn(change->document);

        QCOMPARE(entries.size(), std::size_t{1});
        QCOMPARE(entries.front().label, std::string("Tool"));
        QCOMPARE(entries.front().path, path);
        QVERIFY(entries.front().commandLine.empty());
    }
}

void ExeXmlDocumentTest::AFileWithNoDeclarationCountsAsUtf8OnlyWhenItsBytesAre()
{
    const std::string legacyBytes = "<SimBase.Document>\n  <Launch.Addon>\n    <Name>Avi\xF5"
                                    "es</Name>\n    <Path>C:\\Avi\xF5"
                                    "es\\x.exe</Path>\n  </Launch.Addon>\n</SimBase.Document>\n";
    const std::string withADeclarationThatNamesNoEncoding = "<?xml version=\"1.0\"?>\n" + legacyBytes;

    const StartupAddition accented{.label = "Avi\xC3\xB5"
                                            "es",
                                   .path = PathOf(kPortuguesePathInUtf8)};
    const StartupAddition plain{.label = "Tool", .path = PathOf(R"(C:\Tools\tool.exe)")};

    for (const std::string& document : {legacyBytes, withADeclarationThatNamesNoEncoding})
    {
        QCOMPARE(StartupEntriesIn(document).size(), std::size_t{1});
        QCOMPARE(WithStartupEntryAdded(document, accented).result, FileResult::TheStartupFileIsNotUtf8);
        QCOMPARE(WithStartupEntryAdded(document, plain).result, FileResult::Completed);
    }
}

void ExeXmlDocumentTest::APathWrittenWithANumericCharacterReferenceIsStillFound_data()
{
    QTest::addColumn<QString>("spelling");

    QTest::newRow("decimal") << QStringLiteral("&#233;");
    QTest::newRow("hexadecimal") << QStringLiteral("&#xE9;");
    QTest::newRow("leading zeros") << QStringLiteral("&#0233;");
}

void ExeXmlDocumentTest::APathWrittenWithANumericCharacterReferenceIsStillFound()
{
    QFETCH(const QString, spelling);

    const std::string document = std::string("<SimBase.Document Type=\"Launch\" version=\"1,0\">\n"
                                             "    <Launch.Addon>\n"
                                             "        <Name>Tool</Name>\n"
                                             "        <Disabled>False</Disabled>\n"
                                             "        <Path>C:\\Caf")
        + spelling.toStdString()
        + "\\tool.exe</Path>\n"
          "    </Launch.Addon>\n"
          "</SimBase.Document>\n";
    const std::filesystem::path read = StartupEntriesIn(document).front().path;

    QCOMPARE(read, PathOf("C:\\Caf\xC3\xA9\\tool.exe"));

    const std::optional<std::string> switched = WithStartupEntrySwitched(document, read, false);

    QVERIFY(switched.has_value());
    QVERIFY(!StartupEntriesIn(*switched).front().enabled);
    QCOMPARE(WithStartupEntryRemoved(document, read).result, FileResult::Completed);
    QCOMPARE(WithStartupEntryAdded(document, StartupAddition{.label = "Again", .path = read}).result,
             FileResult::TheStartupEntryIsAlreadyThere);
}

void ExeXmlDocumentTest::AUtf16DocumentWithAByteOrderMarkIsRead_data()
{
    QTest::addColumn<bool>("bigEndian");

    QTest::newRow("little endian") << false;
    QTest::newRow("big endian") << true;
}

void ExeXmlDocumentTest::AUtf16DocumentWithAByteOrderMarkIsRead()
{
    QFETCH(const bool, bigEndian);

    const std::string document = Utf16WithByteOrderMarkOf(kUtf16Text, bigEndian);
    const std::vector<StartupEntry> entries = StartupEntriesIn(document);

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().label,
             std::string("Avi\xC3\xB5"
                         "es"));
    QCOMPARE(entries.front().path, PathOf(kPortuguesePathInUtf8));
    QCOMPARE(entries.front().commandLine, std::string("-a"));
    QVERIFY(!entries.front().enabled);
    QVERIFY(StartupDocumentIsUtf16(document));
    QVERIFY(!StartupDocumentIsUtf16(std::string(kUtf16Text)));
    QVERIFY(!StartupDocumentIsUtf16(std::string(kWindows1252Document)));
}

void ExeXmlDocumentTest::UnescapingReadsTheNamedAndTheNumericReferencesAndLeavesWhatIsNotOne_data()
{
    QTest::addColumn<QByteArray>("written");
    QTest::addColumn<QByteArray>("plain");

    QTest::newRow("named") << QByteArray("a &amp; b &lt;&gt; &quot;&apos;") << QByteArray("a & b <> \"'");
    QTest::newRow("decimal") << QByteArray("&#65;&#233;") << QByteArray("A\xC3\xA9");
    QTest::newRow("hexadecimal") << QByteArray("&#x41;&#xe9;&#xE9;") << QByteArray("A\xC3\xA9\xC3\xA9");
    QTest::newRow("three byte") << QByteArray("&#x20AC;") << QByteArray("\xE2\x82\xAC");
    QTest::newRow("four byte") << QByteArray("&#x1F600;") << QByteArray("\xF0\x9F\x98\x80");
    QTest::newRow("last code point") << QByteArray("&#x10FFFF;") << QByteArray("\xF4\x8F\xBF\xBF");
    QTest::newRow("no double unescaping") << QByteArray("&amp;#233;") << QByteArray("&#233;");
    QTest::newRow("zero") << QByteArray("&#0;") << QByteArray("&#0;");
    QTest::newRow("surrogate") << QByteArray("&#xD800;&#57343;") << QByteArray("&#xD800;&#57343;");
    QTest::newRow("beyond unicode") << QByteArray("&#x110000;&#1114112;") << QByteArray("&#x110000;&#1114112;");
    QTest::newRow("too many digits") << QByteArray("&#000000065;") << QByteArray("&#000000065;");
    QTest::newRow("empty") << QByteArray("&#;&#x;") << QByteArray("&#;&#x;");
    QTest::newRow("not digits") << QByteArray("&#xZZ;&#12a;&#X41;") << QByteArray("&#xZZ;&#12a;&#X41;");
    QTest::newRow("never closed") << QByteArray("&#65") << QByteArray("&#65");
    QTest::newRow("a bare ampersand") << QByteArray("A & B") << QByteArray("A & B");
}

void ExeXmlDocumentTest::UnescapingReadsTheNamedAndTheNumericReferencesAndLeavesWhatIsNotOne()
{
    QFETCH(const QByteArray, written);
    QFETCH(const QByteArray, plain);

    const std::string unescaped = UnescapedXmlText(std::string_view(written.constData(), written.size()));

    QCOMPARE(QByteArray(unescaped.data(), static_cast<qsizetype>(unescaped.size())), plain);
}

QTEST_APPLESS_MAIN(ExeXmlDocumentTest)

#include "tst_exe_xml_document.moc"
