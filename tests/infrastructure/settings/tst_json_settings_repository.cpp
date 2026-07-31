#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "infrastructure/settings/JsonSettingsRepository.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

class JsonSettingsRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    static void AProfileSurvivesTheRoundTrip();
    static void LibrariesKeepTheirIdentityAcrossTheRoundTrip();
    static void TwoProfilesWithDestinationsAndOverridesSurviveTheRoundTrip();
    static void AWriteTheDiskRefusesSaysSoInsteadOfClaimingItLanded();
    static void SettingsThatCannotBeReadAreNotTheSameAsNoSettingsAtAll();
    static void AnAbsentFileReadsAsSettingsWithNoProfileYet();
    static void TheLinkTypeAndTheHashCheckSurviveTheRoundTrip();
    static void AFileWrittenBeforeTheseKeysExistedReadsAsJunctionWithoutTheHashCheck();
};

namespace
{
    struct Storage
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path File() const
        {
            return std::filesystem::path(directory.path().toStdString()) / "settings.json";
        }
    };
}

void JsonSettingsRepositoryTest::AWriteTheDiskRefusesSaysSoInsteadOfClaimingItLanded()
{
    const Storage storage;

    SimulatorProfile profile;
    profile.id = std::string(300, 'p');

    AppSettings written;
    written.profiles = {profile};

    JsonSettingsRepository unreachable(storage.File().parent_path() / std::string(300, 'd') / "settings.json");

    QVERIFY(!unreachable.Save(written));
}

void JsonSettingsRepositoryTest::SettingsThatCannotBeReadAreNotTheSameAsNoSettingsAtAll()
{
    const Storage storage;

    std::filesystem::create_directories(storage.File().parent_path());
    std::ofstream(storage.File(), std::ios::binary) << "isto nao e json";

    QVERIFY(!JsonSettingsRepository(storage.File()).Load().has_value());
}

void JsonSettingsRepositoryTest::AnAbsentFileReadsAsSettingsWithNoProfileYet()
{
    const Storage storage;

    const std::optional<AppSettings> read = JsonSettingsRepository(storage.File()).Load();

    QVERIFY(read.has_value());
    QVERIFY(read->profiles.empty());
}

void JsonSettingsRepositoryTest::AProfileSurvivesTheRoundTrip()
{
    const Storage storage;

    SimulatorProfile profile;
    profile.id = "msfs2024";
    profile.variant = SimulatorVariant::MSFS2024;
    profile.destinations = {"E:/Flight Simulator 2024/Community", "E:/Flight Simulator 2024/Community2024"};
    profile.defaultDestination = "E:/Flight Simulator 2024/Community2024";

    AppSettings written;
    written.profiles = {profile};
    written.activeProfileId = "msfs2024";

    JsonSettingsRepository repository(storage.File());
    QVERIFY(repository.Save(written));

    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.activeProfileId, std::string("msfs2024"));
    QCOMPARE(read.profiles.size(), std::size_t{1});
    QCOMPARE(read.profiles.front().id, std::string("msfs2024"));
    QCOMPARE(read.profiles.front().variant, SimulatorVariant::MSFS2024);
    QCOMPARE(read.profiles.front().destinations.size(), std::size_t{2});
    QCOMPARE(read.profiles.front().destinations.front(), std::filesystem::path("E:/Flight Simulator 2024/Community"));
    QCOMPARE(read.profiles.front().defaultDestination, std::filesystem::path("E:/Flight Simulator 2024/Community2024"));
}

void JsonSettingsRepositoryTest::LibrariesKeepTheirIdentityAcrossTheRoundTrip()
{
    const Storage storage;

    Library main;
    main.id = "{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}";
    main.path = "D:/MSFS 2024";
    main.label = "MSFS 2024";

    Library portable;
    portable.id = "{c9bf9e57-1685-4c89-bafb-ff5af830be8a}";
    portable.path = "Z:/Portable Library";
    portable.label = "Portátil";

    SimulatorProfile profile;
    profile.id = "msfs2024";
    profile.libraries = {main, portable};

    AppSettings written;
    written.profiles = {profile};

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.profiles.size(), std::size_t{1});

    const std::vector<Library>& libraries = read.profiles.front().libraries;
    QCOMPARE(libraries.size(), std::size_t{2});
    QCOMPARE(libraries[0].id, main.id);
    QCOMPARE(libraries[0].path, main.path);
    QCOMPARE(libraries[0].label, main.label);
    QCOMPARE(libraries[1].id, portable.id);
    QCOMPARE(libraries[1].label, std::string("Portátil"));
}

void JsonSettingsRepositoryTest::TwoProfilesWithDestinationsAndOverridesSurviveTheRoundTrip()
{
    const Storage storage;
    const LibraryId libraryId = "{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}";

    SimulatorProfile modern;
    modern.id = "msfs2024";
    modern.variant = SimulatorVariant::MSFS2024;
    modern.destinations = {"E:/Flight Simulator 2024/Community", "E:/Flight Simulator 2024/Community2024"};
    modern.defaultDestination = "E:/Flight Simulator 2024/Community2024";
    modern.libraries = {Library{libraryId, "D:/MSFS 2024", "MSFS 2024"}};
    modern.destinationOverrides = {
        DestinationOverride{libraryId, "Aircraft Mods/pmdg-aircraft-77w", "E:/Flight Simulator 2024/Community"},
        DestinationOverride{libraryId, "Sceneries", "E:/Flight Simulator 2024/Community2024"},
    };

    SimulatorProfile legacy;
    legacy.id = "msfs2020";
    legacy.variant = SimulatorVariant::MSFS2020;
    legacy.destinations = {"C:/Packages/Community"};
    legacy.defaultDestination = "C:/Packages/Community";

    AppSettings written;
    written.profiles = {modern, legacy};
    written.activeProfileId = "msfs2024";

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.profiles.size(), std::size_t{2});
    QCOMPARE(read.profiles[1].variant, SimulatorVariant::MSFS2020);
    QCOMPARE(read.profiles[1].destinations.size(), std::size_t{1});

    const std::vector<DestinationOverride>& overrides = read.profiles[0].destinationOverrides;
    QCOMPARE(overrides.size(), std::size_t{2});
    QCOMPARE(overrides[0].libraryId, libraryId);
    QCOMPARE(overrides[0].relativePath, std::filesystem::path("Aircraft Mods/pmdg-aircraft-77w"));
    QCOMPARE(overrides[0].destination, std::filesystem::path("E:/Flight Simulator 2024/Community"));
    QCOMPARE(overrides[1].relativePath, std::filesystem::path("Sceneries"));
}

void JsonSettingsRepositoryTest::TheLinkTypeAndTheHashCheckSurviveTheRoundTrip()
{
    const Storage storage;

    AppSettings written;
    written.linkType = LinkType::Symbolic;
    written.verifyWithHash = true;

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.linkType, LinkType::Symbolic);
    QCOMPARE(read.verifyWithHash, true);
}

void JsonSettingsRepositoryTest::AFileWrittenBeforeTheseKeysExistedReadsAsJunctionWithoutTheHashCheck()
{
    const Storage storage;

    std::filesystem::create_directories(storage.File().parent_path());
    std::ofstream(storage.File(), std::ios::binary) << R"({"activeProfileId":"msfs2024","profiles":[]})";

    const std::optional<AppSettings> read = JsonSettingsRepository(storage.File()).Load();

    QVERIFY(read.has_value());
    QCOMPARE(read->activeProfileId, std::string("msfs2024"));
    QCOMPARE(read->linkType, LinkType::Junction);
    QCOMPARE(read->verifyWithHash, false);
}

QTEST_APPLESS_MAIN(JsonSettingsRepositoryTest)

#include "tst_json_settings_repository.moc"
