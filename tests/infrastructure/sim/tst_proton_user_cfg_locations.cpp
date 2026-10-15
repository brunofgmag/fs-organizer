#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "infrastructure/sim/ProtonUserCfgLocations.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class ProtonUserCfgLocationsTest : public QObject
    {
        Q_OBJECT

    private slots:
        void init();
        void cleanup();
        void EveryLibraryOffersTheConfigOfBothSimulatorsInsideTheirSteamPrefix();
        void NoLibraryOffersNothing();
        void TheDefaultLibrariesAreTheSteamFolderUnderTheDataHomeAndTheSteamLinkAtHome();
        void TheSteamLinkThatPointsAtTheDataHomeFolderIsNotOfferedTwice();
        void TheLibrariesDoNotNeedToExistToBeOffered();

    private:
        std::optional<QTemporaryDir> root_{};
        QByteArray home_{};
        QByteArray data_{};
    };

    const std::filesystem::path kPrefix2020 = "steamapps/compatdata/1250410/pfx/drive_c/users/steamuser/AppData/"
                                              "Roaming/Microsoft Flight Simulator/UserCfg.opt";
    const std::filesystem::path kPrefix2024 =
        "steamapps/compatdata/2537590/pfx/drive_c/users/steamuser/AppData/Roaming/Microsoft Flight Simulator "
        "2024/UserCfg.opt";
}

void ProtonUserCfgLocationsTest::init()
{
    home_ = qgetenv("HOME");
    data_ = qgetenv("XDG_DATA_HOME");

    root_.emplace();
    QVERIFY(root_->isValid());

    qputenv("HOME", root_->filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_DATA_HOME", root_->filePath(QStringLiteral("home/.local/share")).toUtf8());
}

void ProtonUserCfgLocationsTest::cleanup()
{
    const auto restore = [](const char* name, const QByteArray& value)
    {
        if (value.isNull())
        {
            qunsetenv(name);
            return;
        }

        qputenv(name, value);
    };

    restore("HOME", home_);
    restore("XDG_DATA_HOME", data_);
}

void ProtonUserCfgLocationsTest::EveryLibraryOffersTheConfigOfBothSimulatorsInsideTheirSteamPrefix()
{
    const std::filesystem::path first = "/games/SteamLibrary";
    const std::filesystem::path second = "/mnt/nvme/Steam";

    const std::vector<UserCfgLocation> locations = ProtonUserCfgLocations({first, second});

    QCOMPARE(locations.size(), std::size_t{4});
    QCOMPARE(locations[0].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(locations[0].configPath, first / kPrefix2020);
    QCOMPARE(locations[1].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(locations[1].configPath, first / kPrefix2024);
    QCOMPARE(locations[2].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(locations[2].configPath, second / kPrefix2020);
    QCOMPARE(locations[3].variant, SimulatorVariant::MSFS2024);
    QCOMPARE(locations[3].configPath, second / kPrefix2024);
}

void ProtonUserCfgLocationsTest::NoLibraryOffersNothing()
{
    QVERIFY(ProtonUserCfgLocations({}).empty());
}

void ProtonUserCfgLocationsTest::TheDefaultLibrariesAreTheSteamFolderUnderTheDataHomeAndTheSteamLinkAtHome()
{
    const std::filesystem::path home = AsPath(root_->filePath(QStringLiteral("home")));

    const std::vector<std::filesystem::path> libraries = SteamLibraries();

    QCOMPARE(libraries.size(), std::size_t{2});
    QCOMPARE(libraries[0], home / ".local" / "share" / "Steam");
    QCOMPARE(libraries[1], home / ".steam" / "steam");
}

void ProtonUserCfgLocationsTest::TheSteamLinkThatPointsAtTheDataHomeFolderIsNotOfferedTwice()
{
    const std::filesystem::path home = AsPath(root_->filePath(QStringLiteral("home")));
    std::filesystem::create_directories(home / ".local" / "share" / "Steam");
    std::filesystem::create_directories(home / ".steam");
    std::filesystem::create_directory_symlink(home / ".local" / "share" / "Steam", home / ".steam" / "steam");

    const std::vector<std::filesystem::path> libraries = SteamLibraries();

    QCOMPARE(libraries.size(), std::size_t{1});
    QCOMPARE(libraries[0], home / ".local" / "share" / "Steam");
}

void ProtonUserCfgLocationsTest::TheLibrariesDoNotNeedToExistToBeOffered()
{
    QVERIFY(!std::filesystem::exists(AsPath(root_->filePath(QStringLiteral("home")))));

    QCOMPARE(ProtonUserCfgLocations(SteamLibraries()).size(), std::size_t{4});
}

QTEST_APPLESS_MAIN(ProtonUserCfgLocationsTest)

#include "tst_proton_user_cfg_locations.moc"
