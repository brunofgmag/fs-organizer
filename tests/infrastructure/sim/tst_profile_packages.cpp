#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "infrastructure/sim/ProfilePackages.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"

namespace
{
    class ProfilePackagesTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheListOfTheActiveVariantIsTheOneAnswering();
        static void ChangingTheActiveProfileChangesTheListThatAnswers();
        static void ReloadingReadsTheFileAgainInsteadOfAnsweringFromTheFirstRead();
        static void WithNoListForTheVariantEveryNameIsUnverifiable();
        static void WithTwoAccountsTheChosenOneIsNamedAndWithOneThereIsNothingToName();
    };

    struct Machine
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path WriteList(const std::string& relativePath,
                                                      const std::vector<std::string>& entries) const
        {
            const std::filesystem::path file = Root() / relativePath;
            std::filesystem::create_directories(file.parent_path());

            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream << "<Packages>\n";
            for (const std::string& entry : entries)
            {
                stream << "\t<Package name=\"" << entry << "\" active=\"Activated\"/>\n";
            }
            stream << "</Packages>\n";

            return file;
        }
    };
}

void ProfilePackagesTest::TheListOfTheActiveVariantIsTheOneAnswering()
{
    const Machine machine;
    const std::vector<ContentListLocation> locations = {
        {.variant = SimulatorVariant::MSFS2020,
         .listPath = machine.WriteList("2020/Bruno/Content.xml", {"communityfs20-only-in-2020"})},
        {.variant = SimulatorVariant::MSFS2024,
         .listPath = machine.WriteList("2024/Bruno/Content.xml", {"fs24-only-in-2024"})}};

    const StdFilesystemProbe probe;
    ProfilePackages packages(probe, locations);
    packages.Reload(SimulatorVariant::MSFS2020);

    QCOMPARE(packages.PresenceOf("only-in-2020"), PackagePresence::Present);
    QCOMPARE(packages.PresenceOf("only-in-2024"), PackagePresence::Absent);
}

void ProfilePackagesTest::ChangingTheActiveProfileChangesTheListThatAnswers()
{
    const Machine machine;
    const std::vector<ContentListLocation> locations = {
        {.variant = SimulatorVariant::MSFS2020,
         .listPath = machine.WriteList("2020/Bruno/Content.xml", {"communityfs20-only-in-2020"})},
        {.variant = SimulatorVariant::MSFS2024,
         .listPath = machine.WriteList("2024/Bruno/Content.xml", {"fs24-only-in-2024"})}};

    const StdFilesystemProbe probe;
    ProfilePackages packages(probe, locations);

    packages.Reload(SimulatorVariant::MSFS2024);
    QCOMPARE(packages.PresenceOf("only-in-2024"), PackagePresence::Present);

    packages.Reload(SimulatorVariant::MSFS2020);
    QCOMPARE(packages.PresenceOf("only-in-2024"), PackagePresence::Absent);
    QCOMPARE(packages.PresenceOf("only-in-2020"), PackagePresence::Present);
}

void ProfilePackagesTest::ReloadingReadsTheFileAgainInsteadOfAnsweringFromTheFirstRead()
{
    const Machine machine;
    const std::vector<ContentListLocation> locations = {
        {.variant = SimulatorVariant::MSFS2024,
         .listPath = machine.WriteList("2024/Bruno/Content.xml", {"fs24-flown-before"})}};

    const StdFilesystemProbe probe;
    ProfilePackages packages(probe, locations);
    packages.Reload(SimulatorVariant::MSFS2024);

    QCOMPARE(packages.PresenceOf("flown-since"), PackagePresence::Absent);

    static_cast<void>(machine.WriteList("2024/Bruno/Content.xml", {"fs24-flown-before", "fs24-flown-since"}));
    packages.Reload(SimulatorVariant::MSFS2024);

    QCOMPARE(packages.PresenceOf("flown-since"), PackagePresence::Present);
}

void ProfilePackagesTest::WithNoListForTheVariantEveryNameIsUnverifiable()
{
    const Machine machine;
    const std::vector<ContentListLocation> locations = {
        {.variant = SimulatorVariant::MSFS2020,
         .listPath = machine.WriteList("2020/Bruno/Content.xml", {"communityfs20-only-in-2020"})}};

    const StdFilesystemProbe probe;
    ProfilePackages packages(probe, locations);
    packages.Reload(SimulatorVariant::MSFS2024);

    QCOMPARE(packages.PresenceOf("only-in-2020"), PackagePresence::Unverifiable);
    QVERIFY(!packages.ListTakenAt().has_value());
    QCOMPARE(packages.ListAccountFolder(), std::string());
}

void ProfilePackagesTest::WithTwoAccountsTheChosenOneIsNamedAndWithOneThereIsNothingToName()
{
    const Machine machine;
    const std::filesystem::path onlyOne = machine.WriteList("2020/Bruno/Content.xml", {"communityfs20-lonely"});
    const std::vector<ContentListLocation> locations = {
        {.variant = SimulatorVariant::MSFS2020, .listPath = onlyOne},
        {.variant = SimulatorVariant::MSFS2024,
         .listPath = machine.WriteList("2024/Bruno/Content.xml", {"fs24-shared"})},
        {.variant = SimulatorVariant::MSFS2024,
         .listPath = machine.WriteList("2024/NathosT/Content.xml", {"fs24-shared"})}};

    const StdFilesystemProbe probe;
    ProfilePackages packages(probe, locations);

    packages.Reload(SimulatorVariant::MSFS2024);
    QCOMPARE(packages.ListAccountFolder(), std::string("Bruno"));

    packages.Reload(SimulatorVariant::MSFS2020);
    QCOMPARE(packages.ListAccountFolder(), std::string());
    QVERIFY(packages.ListTakenAt().has_value());
}

QTEST_APPLESS_MAIN(ProfilePackagesTest)

#include "tst_profile_packages.moc"
