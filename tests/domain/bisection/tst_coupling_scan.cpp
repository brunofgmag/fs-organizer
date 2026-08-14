#include <QtTest/QtTest>

#include "domain/bisection/CouplingScan.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class CouplingScanTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddonWithoutModelFoldersCarriesNoCoupling();
        static void TheModelFoldersUnderSimObjectsAirplanesAreRead();
        static void ALiveryCfgThatNamesABaseContainerMarksTheAddonAsASatellite();
        static void ALiveryCfgWithoutABaseContainerLeavesTheAddonUnmarked();
        static void TheProductPackageOfALiveryJsonIsRead();
        static void ALiveryJsonWithoutAProductPackageNamesNobody();
        static void WhatTheAddonWritesInsideAModelFolderComesOutRelativeToTheAddon();
        static void TheFactsOfEveryAddonComeOutInTheOrderTheyWereAsked();
        static void TheMeasuredMdElevenGroupComesOutAsOneUnit();
    };
}

namespace
{
    constexpr auto kBase = "D:/MSFS 2024/Aircrafts (2024)/tfdidesign-aircraft-md11";
    constexpr auto kLivery = "D:/MSFS 2024/Liveries/tfdidesign-md11f-ces-b2170";
    constexpr auto kAirport = "D:/MSFS 2024/Airports/aerosoft-airport-ebbr-brussels";
    constexpr auto kModel = "TFDi_Design_MD-11";

    constexpr auto kMeasuredLiveryCfg = R"([SELECTION]
required_tags = "F,PW"

[VERSION]
major =1
minor =0

[VARIATION]
base_container = "..\TFDi_Design_MD-11F_PW"


[GENERAL]
name = "China Cargo B-2170"
)";

    constexpr auto kMeasuredLiveryCfgWithoutABase = R"([Version]
major = 1
minor = 0

[General]
name="White Livery"

[Selection]
required_tags = "A343_exterior"
)";

    constexpr auto kMeasuredLiveryJson = R"({
  "rev": 2,
  "productId": 212,
  "title": "Aeromexico",
  "productPackage": "pmdg-aircraft-77er",
  "liveryId": "aeromexico_n745am_or_2006_or_historic",
  "version": 1003
})";

    constexpr auto kMeasuredLiveryJsonWithoutAPackage = R"({
  "rev": 2,
  "productId": 212,
  "title": "9 Air ",
  "liveryId": "9_air_b208k_or_2019_or_gold",
  "version": 1002
})";

    struct Disk
    {
        InMemoryFileSystem fileSystem;
        FakeFilesystemProbe filesystemProbe{fileSystem};
        CouplingScan scan{filesystemProbe};

        void PutAModelFolder(const std::filesystem::path& addon, const std::string& model)
        {
            fileSystem.AddDirectory(addon);
            fileSystem.AddDirectory(PathUnder(addon, PathFromUtf8("SimObjects/Airplanes/" + model)));
        }

        void PutALivery(const std::filesystem::path& addon,
                        const std::string& model,
                        const std::string& name,
                        const std::string& file,
                        const std::string& contents)
        {
            const std::string under = "SimObjects/Airplanes/" + model;

            for (const std::string& level :
                 {under + "/liveries", under + "/liveries/vendor", under + "/liveries/vendor/" + name})
            {
                fileSystem.AddDirectory(PathUnder(addon, PathFromUtf8(level)));
            }

            fileSystem.AddFileWithContents(
                PathUnder(addon, PathFromUtf8(under + "/liveries/vendor/" + name + "/" + file)), contents);
        }
    };
}

void CouplingScanTest::AnAddonWithoutModelFoldersCarriesNoCoupling()
{
    Disk disk;
    disk.fileSystem.AddDirectory(kAirport);

    const std::vector<CouplingFacts> facts = disk.scan.FactsAbout({kAirport});

    QCOMPARE(facts.size(), std::size_t{1});
    QCOMPARE(facts.front().folder, std::filesystem::path{kAirport});
    QVERIFY(facts.front().modelFolders.empty());
    QVERIFY(facts.front().declaredPackages.empty());
    QVERIFY(facts.front().writesInside.empty());
    QVERIFY(!facts.front().declaresABaseContainer);
}

void CouplingScanTest::TheModelFoldersUnderSimObjectsAirplanesAreRead()
{
    Disk disk;
    disk.PutAModelFolder(kBase, kModel);
    disk.PutAModelFolder(kBase, "TFDi_Design_MD-11F");

    const std::vector<CouplingFacts> facts = disk.scan.FactsAbout({kBase});

    QCOMPARE(facts.front().modelFolders.size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(facts.front().modelFolders.front()), QString{kModel});
    QCOMPARE(QString::fromStdString(facts.front().modelFolders.back()), QString{"TFDi_Design_MD-11F"});
}

void CouplingScanTest::ALiveryCfgThatNamesABaseContainerMarksTheAddonAsASatellite()
{
    Disk disk;
    disk.PutAModelFolder(kLivery, kModel);
    disk.PutALivery(kLivery, kModel, "ces-b2170-f", "livery.cfg", kMeasuredLiveryCfg);

    QVERIFY(disk.scan.FactsAbout({kLivery}).front().declaresABaseContainer);
}

void CouplingScanTest::ALiveryCfgWithoutABaseContainerLeavesTheAddonUnmarked()
{
    Disk disk;
    disk.PutAModelFolder(kBase, kModel);
    disk.PutALivery(kBase, kModel, "White", "livery.cfg", kMeasuredLiveryCfgWithoutABase);

    QVERIFY(!disk.scan.FactsAbout({kBase}).front().declaresABaseContainer);
}

void CouplingScanTest::TheProductPackageOfALiveryJsonIsRead()
{
    Disk disk;
    disk.PutAModelFolder(kLivery, kModel);
    disk.PutALivery(kLivery, kModel, "aeromexico", "livery.json", kMeasuredLiveryJson);

    const std::vector<std::string> declared = disk.scan.FactsAbout({kLivery}).front().declaredPackages;

    QCOMPARE(declared.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(declared.front()), QString{"pmdg-aircraft-77er"});
}

void CouplingScanTest::ALiveryJsonWithoutAProductPackageNamesNobody()
{
    Disk disk;
    disk.PutAModelFolder(kLivery, kModel);
    disk.PutALivery(kLivery, kModel, "nine-air", "livery.json", kMeasuredLiveryJsonWithoutAPackage);

    QVERIFY(disk.scan.FactsAbout({kLivery}).front().declaredPackages.empty());
}

void CouplingScanTest::WhatTheAddonWritesInsideAModelFolderComesOutRelativeToTheAddon()
{
    Disk disk;
    disk.PutAModelFolder(kBase, kModel);
    disk.fileSystem.AddDirectory(
        PathUnder(kBase, PathFromUtf8("SimObjects/Airplanes/" + std::string(kModel) + "/model")));
    disk.fileSystem.AddDirectory(
        PathUnder(kBase, PathFromUtf8("SimObjects/Airplanes/" + std::string(kModel) + "/panel")));

    const std::vector<std::filesystem::path> written = disk.scan.FactsAbout({kBase}).front().writesInside;

    QCOMPARE(written.size(), std::size_t{2});
    QCOMPARE(written.front(), PathFromUtf8("SimObjects/Airplanes/" + std::string(kModel) + "/model"));
    QCOMPARE(written.back(), PathFromUtf8("SimObjects/Airplanes/" + std::string(kModel) + "/panel"));
}

void CouplingScanTest::TheFactsOfEveryAddonComeOutInTheOrderTheyWereAsked()
{
    Disk disk;
    disk.fileSystem.AddDirectory(kAirport);
    disk.PutAModelFolder(kBase, kModel);

    const std::vector<CouplingFacts> facts = disk.scan.FactsAbout({kAirport, kBase});

    QCOMPARE(facts.size(), std::size_t{2});
    QCOMPARE(facts.front().folder, std::filesystem::path{kAirport});
    QCOMPARE(facts.back().folder, std::filesystem::path{kBase});
}

void CouplingScanTest::TheMeasuredMdElevenGroupComesOutAsOneUnit()
{
    Disk disk;
    disk.PutAModelFolder(kBase, kModel);
    disk.PutALivery(kBase, kModel, "White", "livery.cfg", kMeasuredLiveryCfgWithoutABase);
    disk.PutAModelFolder(kLivery, kModel);
    disk.PutALivery(kLivery, kModel, "ces-b2170-f", "livery.cfg", kMeasuredLiveryCfg);
    disk.fileSystem.AddDirectory(kAirport);

    const std::vector<SearchUnit> units = UnitsFrom(disk.scan.FactsAbout({kBase, kLivery, kAirport}));

    QCOMPARE(units.size(), std::size_t{2});
    QCOMPARE(units.front().addons.size(), std::size_t{2});
    QCOMPARE(units.front().base, std::optional<std::filesystem::path>{kBase});
    QCOMPARE(units.back().addons, std::vector<std::filesystem::path>{kAirport});
}

QTEST_APPLESS_MAIN(CouplingScanTest)

#include "tst_coupling_scan.moc"
