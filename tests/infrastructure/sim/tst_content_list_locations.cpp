#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "infrastructure/sim/ContentListLocations.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"

namespace
{
    class ContentListLocationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheListIsFoundInTheAccountFolderBelowTheUserCfg();
        static void TheOlderLayoutWithTheListBesideTheUserCfgIsFoundToo();
        static void EveryAccountFolderIsReportedAndTheOrderIsTheSameOnEveryMachine();
        static void TheBackupsTheSimulatorLeavesBesideItAreNeverTheList();
        static void TheOnlyListOfTheVariantIsUsedAndThereIsNothingMoreToShow();
        static void AListOfAnotherVariantIsNeverTheOneChosen();
        static void WithTwoAccountsTheFirstIsUsedAndTheChosenAccountFolderIsNamed();
        static void WithNoListForTheVariantNothingIsChosen();
    };

    struct Machine
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path AddUserCfg(const std::string& folder) const
        {
            const std::filesystem::path configPath = Root() / folder / "UserCfg.opt";
            std::filesystem::create_directories(configPath.parent_path());

            std::ofstream file(configPath, std::ios::binary);
            file << "Version 66\n";

            return configPath;
        }

        std::filesystem::path AddFile(const std::string& relativePath) const
        {
            const std::filesystem::path file = Root() / relativePath;
            std::filesystem::create_directories(file.parent_path());

            std::ofstream stream(file, std::ios::binary);
            stream << "<Packages>\n</Packages>\n";

            return file;
        }
    };
}

void ContentListLocationsTest::TheListIsFoundInTheAccountFolderBelowTheUserCfg()
{
    const Machine machine;
    const std::filesystem::path userCfg = machine.AddUserCfg("Microsoft Flight Simulator 2024");
    const std::filesystem::path list = machine.AddFile("Microsoft Flight Simulator 2024/NathosT/Content.xml");

    const StdFilesystemProbe probe;
    const std::vector<ContentListLocation> found =
        ContentListLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = userCfg}}, probe);

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(found[0].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(found[0].listPath, list);
}

void ContentListLocationsTest::TheOlderLayoutWithTheListBesideTheUserCfgIsFoundToo()
{
    const Machine machine;
    const std::filesystem::path userCfg = machine.AddUserCfg("Microsoft Flight Simulator");
    const std::filesystem::path list = machine.AddFile("Microsoft Flight Simulator/Content.xml");
    const std::filesystem::path neverFlown = machine.AddUserCfg("Microsoft Flight Simulator 2024");

    const StdFilesystemProbe probe;
    const std::vector<ContentListLocation> found =
        ContentListLocations({{.variant = SimulatorVariant::MSFS2020, .configPath = userCfg},
                              {.variant = SimulatorVariant::MSFS2024, .configPath = neverFlown}},
                             probe);

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(found[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(found[0].listPath, list);
}

void ContentListLocationsTest::EveryAccountFolderIsReportedAndTheOrderIsTheSameOnEveryMachine()
{
    const Machine machine;
    const std::filesystem::path userCfg = machine.AddUserCfg("Microsoft Flight Simulator 2024");
    const std::filesystem::path later = machine.AddFile("Microsoft Flight Simulator 2024/NathosT/Content.xml");
    const std::filesystem::path earlier = machine.AddFile("Microsoft Flight Simulator 2024/Bruno/Content.xml");

    const StdFilesystemProbe probe;
    const std::vector<ContentListLocation> found =
        ContentListLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = userCfg}}, probe);

    QCOMPARE(found.size(), std::size_t{2});
    QCOMPARE(found[0].listPath, earlier);
    QCOMPARE(found[1].listPath, later);
}

void ContentListLocationsTest::TheBackupsTheSimulatorLeavesBesideItAreNeverTheList()
{
    const Machine machine;
    const std::filesystem::path userCfg = machine.AddUserCfg("Microsoft Flight Simulator 2024");
    const std::filesystem::path list = machine.AddFile("Microsoft Flight Simulator 2024/NathosT/Content.xml");
    machine.AddFile("Microsoft Flight Simulator 2024/NathosT/Content.xml_backup_20250914154450");
    machine.AddFile("Microsoft Flight Simulator 2024/NathosT/Content.xml_backup_20250914154815");
    machine.AddFile("Microsoft Flight Simulator 2024/Layouts/Content.xml_backup_20250914190942");

    const StdFilesystemProbe probe;
    const std::vector<ContentListLocation> found =
        ContentListLocations({{.variant = SimulatorVariant::MSFS2024, .configPath = userCfg}}, probe);

    QCOMPARE(found.size(), std::size_t{1});
    QCOMPARE(found[0].listPath, list);
}

void ContentListLocationsTest::TheOnlyListOfTheVariantIsUsedAndThereIsNothingMoreToShow()
{
    const std::vector<ContentListLocation> found = {
        {.variant = SimulatorVariant::MSFS2024, .listPath = "D:/AppData/Flight Simulator 2024/Bruno/Content.xml"}};

    const std::optional<ChosenContentList> chosen = ChooseContentList(found, SimulatorVariant::MSFS2024);

    QVERIFY(chosen.has_value());
    QCOMPARE(chosen->listPath, std::filesystem::path("D:/AppData/Flight Simulator 2024/Bruno/Content.xml"));
    QCOMPARE(chosen->accountFolder, std::string());
}

void ContentListLocationsTest::AListOfAnotherVariantIsNeverTheOneChosen()
{
    const std::vector<ContentListLocation> found = {
        {.variant = SimulatorVariant::MSFS2020, .listPath = "D:/AppData/Flight Simulator/Bruno/Content.xml"},
        {.variant = SimulatorVariant::MSFS2024, .listPath = "D:/AppData/Flight Simulator 2024/Bruno/Content.xml"}};

    const std::optional<ChosenContentList> chosen = ChooseContentList(found, SimulatorVariant::MSFS2020);

    QVERIFY(chosen.has_value());
    QCOMPARE(chosen->listPath, std::filesystem::path("D:/AppData/Flight Simulator/Bruno/Content.xml"));
    QCOMPARE(chosen->accountFolder, std::string());
}

void ContentListLocationsTest::WithTwoAccountsTheFirstIsUsedAndTheChosenAccountFolderIsNamed()
{
    const std::vector<ContentListLocation> found = {
        {.variant = SimulatorVariant::MSFS2024, .listPath = "D:/AppData/Flight Simulator 2024/Bruno/Content.xml"},
        {.variant = SimulatorVariant::MSFS2024, .listPath = "D:/AppData/Flight Simulator 2024/NathosT/Content.xml"}};

    const std::optional<ChosenContentList> chosen = ChooseContentList(found, SimulatorVariant::MSFS2024);

    QVERIFY(chosen.has_value());
    QCOMPARE(chosen->listPath, std::filesystem::path("D:/AppData/Flight Simulator 2024/Bruno/Content.xml"));
    QCOMPARE(chosen->accountFolder, std::string("Bruno"));
}

void ContentListLocationsTest::WithNoListForTheVariantNothingIsChosen()
{
    const std::vector<ContentListLocation> found = {
        {.variant = SimulatorVariant::MSFS2020, .listPath = "D:/AppData/Flight Simulator/Bruno/Content.xml"}};

    QVERIFY(!ChooseContentList(found, SimulatorVariant::MSFS2024).has_value());
    QVERIFY(!ChooseContentList({}, SimulatorVariant::MSFS2024).has_value());
}

QTEST_APPLESS_MAIN(ContentListLocationsTest)

#include "tst_content_list_locations.moc"
