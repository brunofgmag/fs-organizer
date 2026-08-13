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

namespace
{
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
        static void TheUpdateModeAndTheLanguageSurviveTheRoundTrip();
        static void TheOriginOfAnAddonThatCameFromAnotherProgramSurvivesTheRoundTrip();
        static void AProfileWrittenBeforeExternalOriginsExistedReadsWithNone();
        static void AFreshProfileIsWrittenWithTheStartupEntriesManaged();
        static void TurningTheStartupEntriesLooseSurvivesTheRoundTrip();
        static void AFileWrittenBeforeTheStartupKeyExistedStillManagesTheStartupEntries();
        static void AFreshProfileIsWrittenWithThePackageListLeftAlone();
        static void TheAirportsTheUserSaidCanCoexistSurviveTheRoundTrip();
        static void TheBookmarksOfAReadDocumentSurviveTheRoundTrip();
        static void ADocumentWrittenBeforeBookmarksExistedReadsWithNone();
        static void AFileWrittenBeforeTheReadingKeysExistedKeepsBothGesturesOn();
        static void TheGesturesOfTheReaderSurviveTheRoundTrip();
    };
}

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
    std::ofstream(storage.File(), std::ios::binary) << "this is not json";

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
    modern.libraries = {Library{.id = libraryId, .path = "D:/MSFS 2024", .label = "MSFS 2024"}};
    modern.destinationOverrides = {
        DestinationOverride{.libraryId = libraryId,
                            .relativePath = "Aircraft Mods/pmdg-aircraft-77w",
                            .destination = "E:/Flight Simulator 2024/Community"},
        DestinationOverride{.libraryId = libraryId,
                            .relativePath = "Sceneries",
                            .destination = "E:/Flight Simulator 2024/Community2024"},
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
    QCOMPARE(read->updateMode, UpdateMode::Notify);
    QVERIFY(read->language.empty());
}

void JsonSettingsRepositoryTest::TheUpdateModeAndTheLanguageSurviveTheRoundTrip()
{
    const Storage storage;

    AppSettings written;
    written.updateMode = UpdateMode::Automatic;
    written.language = "pt_BR";

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.updateMode, UpdateMode::Automatic);
    QCOMPARE(read.language, std::string("pt_BR"));
}

void JsonSettingsRepositoryTest::TheOriginOfAnAddonThatCameFromAnotherProgramSurvivesTheRoundTrip()
{
    const Storage storage;
    const LibraryId libraryId = "{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}";

    SimulatorProfile profile;
    profile.id = "msfs2024";
    profile.libraries = {Library{.id = libraryId, .path = "D:/MSFS 2024", .label = "MSFS 2024"}};
    profile.externalOrigins = {
        ExternalOrigin{.libraryId = libraryId,
                       .relativePath = "Utilities/gsx-pro",
                       .externalPath = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro"},
    };

    AppSettings written;
    written.profiles = {profile};

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.profiles.size(), std::size_t{1});

    const std::vector<ExternalOrigin>& origins = read.profiles[0].externalOrigins;
    QCOMPARE(origins.size(), std::size_t{1});
    QCOMPARE(origins[0].libraryId, libraryId);
    QCOMPARE(origins[0].relativePath, std::filesystem::path("Utilities/gsx-pro"));
    QCOMPARE(origins[0].externalPath, std::filesystem::path("C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro"));
}

void JsonSettingsRepositoryTest::AProfileWrittenBeforeExternalOriginsExistedReadsWithNone()
{
    const Storage storage;

    std::filesystem::create_directories(storage.File().parent_path());
    std::ofstream(storage.File(), std::ios::binary) << R"({"profiles":[{"id":"msfs2024"}]})";

    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.profiles.size(), std::size_t{1});
    QVERIFY(read.profiles[0].externalOrigins.empty());
}

void JsonSettingsRepositoryTest::AFreshProfileIsWrittenWithTheStartupEntriesManaged()
{
    const Storage storage;

    SimulatorProfile profile;
    profile.id = "msfs2024";

    AppSettings written;
    written.profiles = {profile};
    written.activeProfileId = "msfs2024";

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.manageStartupEntries, true);
}

void JsonSettingsRepositoryTest::TurningTheStartupEntriesLooseSurvivesTheRoundTrip()
{
    const Storage storage;

    AppSettings written;
    written.manageStartupEntries = false;

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.manageStartupEntries, false);
}

void JsonSettingsRepositoryTest::AFileWrittenBeforeTheReadingKeysExistedKeepsBothGesturesOn()
{
    const Storage storage;

    std::filesystem::create_directories(storage.File().parent_path());
    std::ofstream(storage.File(), std::ios::binary) << R"({"activeProfileId":"msfs2024","profiles":[]})";

    const std::optional<AppSettings> read = JsonSettingsRepository(storage.File()).Load();

    QVERIFY(read.has_value());
    QVERIFY2(read->wheelZooms,
             "the wheel zoomed the chart before the key existed, and a file written back then keeps that gesture");
    QVERIFY2(read->dragMovesThePage,
             "the drag is born on, so the reader behaves the same on a settings file that "
             "predates the switch");
}

void JsonSettingsRepositoryTest::TheGesturesOfTheReaderSurviveTheRoundTrip()
{
    const Storage storage;

    AppSettings written;
    written.activeProfileId = "msfs2024";
    written.wheelZooms = false;
    written.dragMovesThePage = false;

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));

    const std::optional<AppSettings> read = JsonSettingsRepository(storage.File()).Load();

    QVERIFY(read.has_value());
    QCOMPARE(read->wheelZooms, false);
    QCOMPARE(read->dragMovesThePage, false);
}

void JsonSettingsRepositoryTest::AFileWrittenBeforeTheStartupKeyExistedStillManagesTheStartupEntries()
{
    const Storage storage;

    std::filesystem::create_directories(storage.File().parent_path());
    std::ofstream(storage.File(), std::ios::binary) << R"({"activeProfileId":"msfs2024","verifyWithHash":true})";

    const std::optional<AppSettings> read = JsonSettingsRepository(storage.File()).Load();

    QVERIFY(read.has_value());
    QCOMPARE(read->verifyWithHash, true);
    QCOMPARE(read->manageStartupEntries, true);
    QVERIFY2(!read->managePackageList,
             "this one is born off, and a file written before the key existed adopts the default instead of being "
             "refused");
    QVERIFY(read->coexistingAirports.empty());
}

void JsonSettingsRepositoryTest::AFreshProfileIsWrittenWithThePackageListLeftAlone()
{
    const Storage storage;

    SimulatorProfile profile;
    profile.id = "msfs2024";

    AppSettings written;
    written.profiles = {profile};
    written.activeProfileId = "msfs2024";

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QVERIFY2(!read.managePackageList,
             "unlike the startup file, this one prevents no defect and writes into the file the simulator wipes when "
             "it dislikes the XML");
    QCOMPARE(read.manageStartupEntries, true);
}

void JsonSettingsRepositoryTest::TheAirportsTheUserSaidCanCoexistSurviveTheRoundTrip()
{
    const Storage storage;

    AppSettings written;
    written.managePackageList = true;
    written.coexistingAirports = {{.one = {.libraryId = "library-1", .folderName = "one-eham"},
                                   .other = {.libraryId = "library-1", .folderName = "another-eham"}}};

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QVERIFY(read.managePackageList);
    QCOMPARE(read.coexistingAirports.size(), std::size_t{1});
    QVERIFY2(read.coexistingAirports.front().one == written.coexistingAirports.front().one,
             "the key is the pair of addon identities, which survives renaming a category and moving an addon");
    QVERIFY(read.coexistingAirports.front().other == written.coexistingAirports.front().other);
}

void JsonSettingsRepositoryTest::TheBookmarksOfAReadDocumentSurviveTheRoundTrip()
{
    const Storage storage;

    AppSettings written;
    written.documents = {{.addon = "aerosoft-crj",
                          .document = "Documentation/Vol1_Aircraft Systems.pdf",
                          .page = 57,
                          .favourite = true,
                          .bookmarks = {{.page = 12, .name = {}}, {.page = 57, .name = "Where I stopped"}}}};

    QVERIFY(JsonSettingsRepository(storage.File()).Save(written));
    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.documents.size(), std::size_t{1});
    QCOMPARE(read.documents.front().page, 57);
    QVERIFY(read.documents.front().favourite);
    QCOMPARE(read.documents.front().bookmarks.size(), std::size_t{2});
    QCOMPARE(read.documents.front().bookmarks.front().page, 12);
    QVERIFY2(read.documents.front().bookmarks.front().name.empty(),
             "the derived name is computed from the outline every time, so writing it would freeze a name that has to "
             "follow the other marks of its section");
    QCOMPARE(read.documents.front().bookmarks.back().name, std::string{"Where I stopped"});
}

void JsonSettingsRepositoryTest::ADocumentWrittenBeforeBookmarksExistedReadsWithNone()
{
    const Storage storage;

    std::ofstream(storage.File(), std::ios::binary)
        << R"({"documents":[{"addon":"aerosoft-crj","document":"a.pdf","page":3,"favourite":false}]})";

    const AppSettings read = JsonSettingsRepository(storage.File()).Load().value_or(AppSettings{});

    QCOMPARE(read.documents.size(), std::size_t{1});
    QCOMPARE(read.documents.front().page, 3);
    QVERIFY(read.documents.front().bookmarks.empty());
}

QTEST_APPLESS_MAIN(JsonSettingsRepositoryTest)

#include "tst_json_settings_repository.moc"
