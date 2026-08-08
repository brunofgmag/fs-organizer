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
#include "tests/support/PathPrinting.h"

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
    };

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

QTEST_APPLESS_MAIN(ExeXmlDocumentTest)

#include "tst_exe_xml_document.moc"
