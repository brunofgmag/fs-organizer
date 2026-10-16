#include <QtTest/QtTest>

#include <filesystem>
#include <vector>

#include "infrastructure/sim/ProtonUserCfgLocations.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ProtonUserCfgLocationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryPrefixOffersTheConfigOfItsSimulatorInsideItself();
        static void NoPrefixOffersNothing();
    };

    const std::filesystem::path kRoaming = "drive_c/users/steamuser/AppData/Roaming";
}

void ProtonUserCfgLocationsTest::EveryPrefixOffersTheConfigOfItsSimulatorInsideItself()
{
    const std::filesystem::path first = "/games/SteamLibrary/steamapps/compatdata/2537590/pfx";
    const std::filesystem::path second = "/mnt/nvme/Steam/steamapps/compatdata/1250410/pfx";

    const std::vector<UserCfgLocation> locations =
        ProtonUserCfgLocations({{.variant = SimulatorVariant::MSFS2024, .path = first},
                                {.variant = SimulatorVariant::MSFS2020, .path = second}});

    QCOMPARE(locations.size(), std::size_t{2});
    QCOMPARE(locations[0].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(locations[0].configPath, first / kRoaming / "Microsoft Flight Simulator 2024" / "UserCfg.opt");
    QCOMPARE(locations[1].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(locations[1].configPath, second / kRoaming / "Microsoft Flight Simulator" / "UserCfg.opt");
}

void ProtonUserCfgLocationsTest::NoPrefixOffersNothing()
{
    QVERIFY(ProtonUserCfgLocations({}).empty());
}

QTEST_APPLESS_MAIN(ProtonUserCfgLocationsTest)

#include "tst_proton_user_cfg_locations.moc"
