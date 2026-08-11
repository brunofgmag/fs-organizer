#include <QtTest/QtTest>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "infrastructure/sim/ContentListLocations.h"
#include "infrastructure/sim/ContentXmlPackages.h"
#include "support/MomentText.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/StdFilesystemProbe.h"
#include "tests/support/TempFiles.h"

namespace
{
    class ContentXmlPackagesTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFourGenerationPrefixesAnswerToTheBareName();
        static void AListThatIsNotThereIsUnverifiableAndNeverAbsent();
        static void AListThatCannotBeParsedEndsWhereAMissingOneEnds();
        static void AListWithNoEntryAtAllIsNoEvidenceOfAbsence();
        static void TheDayTheListWasWrittenTravelsWithIt();
        static void AListNobodyCouldReadCarriesNoDay();
        static void AnEntryWithoutAPrefixAnswersByTheSamePathAndAStrangerIsAbsent();
        static void CaseOnEitherSideDoesNotDecidePresence();
        static void ANameCarryingAGenerationPrefixDoesNotResolve();
        static void TheSameNameUnderTwoGenerationsAnswersOnceByTheBareName();
        static void TheListTheDiscoveryFindsOnRealDiskIsTheListTheAdapterReads();
        static void TheDoubleAnswersUnverifiableUntilItIsGivenAList();
        static void APackageInstalledAfterTheFirstReadShowsUpOnTheNextOne();
        static void ReadingAgainForgetsWhatThePreviousListSaid();
    };

    [[nodiscard]] std::filesystem::path FixtureList()
    {
        return std::filesystem::path(FSORG_FIXTURES_DIR) / "simulator-content.xml";
    }

    [[nodiscard]] std::filesystem::file_time_type FileTimeOf(const std::chrono::system_clock::time_point moment)
    {
        return std::filesystem::file_time_type::clock::now()
            + std::chrono::duration_cast<std::filesystem::file_time_type::duration>(moment
                                                                                    - std::chrono::system_clock::now());
    }
}

void ContentXmlPackagesTest::APackageInstalledAfterTheFirstReadShowsUpOnTheNextOne()
{
    const TempFiles files;
    const std::filesystem::path listPath = files.WriteText(
        "Content.xml", "<Packages>\n\t<Package name=\"fs24-asobo-activities\" active=\"Activated\"/>\n</Packages>\n");

    const StdFilesystemProbe probe;
    ContentXmlPackages packages(probe, listPath);

    QCOMPARE(packages.PresenceOf("aaa-simaddons-animals"), PackagePresence::Absent);

    static_cast<void>(files.WriteText("Content.xml",
                                      "<Packages>\n\t<Package name=\"fs24-asobo-activities\" active=\"Activated\"/>\n"
                                      "\t<Package name=\"fs24-aaa-simaddons-animals\" active=\"Activated\"/>\n"
                                      "</Packages>\n"));

    packages.ReadAgain(listPath);

    QCOMPARE(packages.PresenceOf("aaa-simaddons-animals"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("asobo-activities"), PackagePresence::Present);
}

void ContentXmlPackagesTest::ReadingAgainForgetsWhatThePreviousListSaid()
{
    const TempFiles files;
    const std::filesystem::path listPath = files.WriteText(
        "Content.xml", "<Packages>\n\t<Package name=\"fs24-asobo-activities\" active=\"Activated\"/>\n</Packages>\n");

    const StdFilesystemProbe probe;
    ContentXmlPackages packages(probe, listPath);

    QCOMPARE(packages.PresenceOf("asobo-activities"), PackagePresence::Present);
    QVERIFY(packages.ListTakenAt().has_value());

    packages.ReadAgain(files.Root() / "no-such-content.xml");

    QCOMPARE(packages.PresenceOf("asobo-activities"), PackagePresence::Unverifiable);
    QVERIFY(!packages.ListTakenAt().has_value());
}

void ContentXmlPackagesTest::TheFourGenerationPrefixesAnswerToTheBareName()
{
    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, FixtureList());

    QCOMPARE(packages.PresenceOf("asobo-vcockpits-core"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("asobo-activities"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("aaa-simaddons-animals"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("xmd11_light_mod_fs24"), PackagePresence::Present);
}

void ContentXmlPackagesTest::AListThatIsNotThereIsUnverifiableAndNeverAbsent()
{
    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, std::filesystem::path(FSORG_FIXTURES_DIR) / "no-such-content.xml");

    QCOMPARE(packages.PresenceOf("asobo-vcockpits-core"), PackagePresence::Unverifiable);
    QCOMPARE(packages.PresenceOf("nothing-like-this-was-ever-installed"), PackagePresence::Unverifiable);
}

void ContentXmlPackagesTest::AListThatCannotBeParsedEndsWhereAMissingOneEnds()
{
    const TempFiles files;
    const std::filesystem::path listPath = files.WriteText(
        "Content.xml", "<Packages>\n\t<Package name=\"fs24-asobo-vcockpits-core\" active=\"Activated\"\n");

    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, listPath);

    QCOMPARE(packages.PresenceOf("asobo-vcockpits-core"), PackagePresence::Unverifiable);
}

void ContentXmlPackagesTest::AListWithNoEntryAtAllIsNoEvidenceOfAbsence()
{
    const TempFiles files;
    const std::filesystem::path listPath = files.WriteText("Content.xml", "<Packages>\n</Packages>\n");

    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, listPath);

    QCOMPARE(packages.PresenceOf("asobo-vcockpits-core"), PackagePresence::Unverifiable);
    QVERIFY(!packages.ListTakenAt().has_value());
}

void ContentXmlPackagesTest::TheDayTheListWasWrittenTravelsWithIt()
{
    const TempFiles files;
    const std::filesystem::path listPath = files.WriteText(
        "Content.xml", "<Packages>\n\t<Package name=\"fs24-asobo-activities\" active=\"Activated\"/>\n</Packages>\n");

    const std::chrono::system_clock::time_point lastFlight =
        std::chrono::sys_days{std::chrono::September / 14 / 2025} + std::chrono::hours{12};
    std::filesystem::last_write_time(listPath, FileTimeOf(lastFlight));

    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, listPath);

    QVERIFY(packages.ListTakenAt().has_value());
    QCOMPARE(AsDay(*packages.ListTakenAt()), AsDay(lastFlight));
    QVERIFY(AsDay(*packages.ListTakenAt()) != AsDay(std::chrono::system_clock::now()));
}

void ContentXmlPackagesTest::AListNobodyCouldReadCarriesNoDay()
{
    const TempFiles files;
    const std::filesystem::path listPath = files.WriteText("Content.xml", "<Packages>\n\t<Package name=\"fs24-a\"\n");

    const StdFilesystemProbe probe;
    const ContentXmlPackages present(probe, listPath);
    const ContentXmlPackages missing(probe, files.Root() / "no-such-content.xml");

    QVERIFY(!present.ListTakenAt().has_value());
    QVERIFY(!missing.ListTakenAt().has_value());
}

void ContentXmlPackagesTest::AnEntryWithoutAPrefixAnswersByTheSamePathAndAStrangerIsAbsent()
{
    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, FixtureList());

    QCOMPARE(packages.PresenceOf("unprefixed-legacy-package"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("nothing-like-this-was-ever-installed"), PackagePresence::Absent);
}

void ContentXmlPackagesTest::CaseOnEitherSideDoesNotDecidePresence()
{
    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, FixtureList());

    QCOMPARE(packages.PresenceOf("mixed-case-package"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("ASOBO-VCOCKPITS-CORE"), PackagePresence::Present);
}

void ContentXmlPackagesTest::ANameCarryingAGenerationPrefixDoesNotResolve()
{
    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, FixtureList());

    QCOMPARE(packages.PresenceOf("fs24-asobo-vcockpits-core"), PackagePresence::Absent);
    QCOMPARE(packages.PresenceOf("communityfs20-xmd11_light_mod_fs24"), PackagePresence::Absent);
}

void ContentXmlPackagesTest::TheSameNameUnderTwoGenerationsAnswersOnceByTheBareName()
{
    const StdFilesystemProbe probe;
    const ContentXmlPackages packages(probe, FixtureList());

    QCOMPARE(packages.PresenceOf("asobo-activities"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("fs20-asobo-activities"), PackagePresence::Absent);
    QCOMPARE(packages.PresenceOf("fs24-asobo-activities"), PackagePresence::Absent);
}

void ContentXmlPackagesTest::TheListTheDiscoveryFindsOnRealDiskIsTheListTheAdapterReads()
{
    const TempFiles files;
    const std::filesystem::path base = files.Root() / "Microsoft Flight Simulator 2024";
    std::filesystem::create_directories(base / "NathosT");
    std::filesystem::copy_file(FixtureList(), base / "NathosT" / "Content.xml");

    const std::filesystem::path userCfg = base / "UserCfg.opt";
    std::ofstream(userCfg, std::ios::binary) << "Version 66\n";

    const StdFilesystemProbe probe;
    const std::vector<ContentListLocation> found =
        ContentListLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = userCfg}}, probe);

    QCOMPARE(found.size(), std::size_t{1});

    const ContentXmlPackages packages(probe, found[0].listPath);

    QCOMPARE(packages.PresenceOf("asobo-vcockpits-core"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("nothing-like-this-was-ever-installed"), PackagePresence::Absent);
    QVERIFY(packages.ListTakenAt().has_value());
}

void ContentXmlPackagesTest::TheDoubleAnswersUnverifiableUntilItIsGivenAList()
{
    FakeSimulatorPackages packages;
    const SimulatorPackages& asTheDomainSeesIt = packages;

    QCOMPARE(asTheDomainSeesIt.PresenceOf("asobo-vcockpits-core"), PackagePresence::Unverifiable);

    packages.ReportAsInstalled("asobo-vcockpits-core");

    QCOMPARE(asTheDomainSeesIt.PresenceOf("asobo-vcockpits-core"), PackagePresence::Present);
    QCOMPARE(asTheDomainSeesIt.PresenceOf("fs-base-propdefs"), PackagePresence::Absent);
}

QTEST_APPLESS_MAIN(ContentXmlPackagesTest)

#include "tst_content_xml_packages.moc"
