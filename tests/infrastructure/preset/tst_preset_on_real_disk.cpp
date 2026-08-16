#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>
#include <string>

#include "application/PresetService.h"
#include "application/ProfileService.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/preset/PresetPlan.h"
#include "domain/ports/ImportedFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/preset/FilePresetRepository.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const NothingWasImported nothingWasImported;

    class PresetOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ACapturedPresetNamesExactlyTheAddonsThatAreLinked();
        static void UpdatingACapturedPresetKeepsNamingOnlyWhatIsLinked();
        static void ApplyingAFreshlyCapturedPresetLeavesTheDestinationAsItWas();
        static void TheReturnPresetLandsOnDiskWithWhatWasEnabledAndStaysOutOfTheList();
        static void ApplyingTheReturnPresetPutsTheDestinationBackAsItWas();
    };
}

namespace
{
    constexpr int kAddonsPerCategory = 4;

    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdWString());
        }

        [[nodiscard]] std::filesystem::path Library() const
        {
            return Root() / "Library";
        }

        [[nodiscard]] std::filesystem::path Community() const
        {
            return Root() / "Sim" / "Community";
        }

        [[nodiscard]] std::filesystem::path Streamed() const
        {
            return Root() / "Sim" / "StreamedPackages";
        }

        void AddFile(const std::filesystem::path& relative, const std::string& content) const
        {
            const std::filesystem::path file = Root() / relative;
            std::filesystem::create_directories(file.parent_path());

            std::ofstream stream(file, std::ios::binary);
            stream << content;
        }

        [[nodiscard]] SimulatorProfile Profile() const
        {
            SimulatorProfile profile;
            profile.id = "msfs2024";
            profile.destinations = {Community(), Streamed()};
            profile.defaultDestination = Community();
            profile.libraries = {::Library{.id = "lib-1", .path = Library(), .label = "Biblioteca"}};

            return profile;
        }
    };

    struct Composition
    {
        WindowsLinkService linkService{};
        WindowsFilesystemProbe filesystemProbe{};
        WindowsSidecarStore sidecars{};
        LinkingEngine linking{linkService, filesystemProbe};
        SystemClock clock{};
        std::filesystem::path journalFile{};
        JsonlOperationJournal journal{journalFile};
        OperationLog log{journal, clock};
        JsonManifestParser manifestParser{};
        FilesystemScanner catalog{manifestParser, filesystemProbe, nothingWasImported};
        EntryClassifier classifier{linkService, filesystemProbe};
        FakeLibraryIdGenerator identities{};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        std::filesystem::path presetRoot{};
        FilePresetRepository presets{presetRoot};
        PresetService service{presets, profiles, startup.service};
    };

    std::string AddonNameAt(const std::string& category, const int index)
    {
        return category + "-addon-" + std::to_string(index);
    }

    void PutALibraryOnDisk(const Disk& disk)
    {
        for (const std::string& category : {std::string{"Aircrafts"}, std::string{"Sceneries"}, std::string{"Utils"}})
        {
            for (int index = 0; index < kAddonsPerCategory; ++index)
            {
                disk.AddFile("Library/" + category + "/" + AddonNameAt(category, index) + "/manifest.json",
                             R"({"title": "Addon", "package_version": "1.0.0"})");
            }
        }

        std::filesystem::create_directories(disk.Community());
        std::filesystem::create_directories(disk.Streamed());
    }

    void PutFoldersTheAppDoesNotOwnInTheDestination(const Disk& disk, const int howMany)
    {
        for (int index = 0; index < howMany; ++index)
        {
            disk.AddFile("Sim/Community/stranger-" + std::to_string(index) + "/manifest.json",
                         R"({"title": "Stranger"})");
        }
    }

    std::filesystem::path AddonFolder(const Disk& disk, const std::string& category, const int index)
    {
        return disk.Library() / category / AddonNameAt(category, index);
    }

    void ReallyEnable(const Composition& composed, const Disk& disk, const std::string& category, const int index)
    {
        QVERIFY(composed.linking
                    .Enable(Addon{AddonFolder(disk, category, index), Manifest{}}, disk.Community(), LinkType::Junction)
                    .Succeeded());
    }

    std::size_t ChildrenOf(const std::filesystem::path& folder)
    {
        std::size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            static_cast<void>(entry);
            ++count;
        }

        return count;
    }
}

void PresetOnRealDiskTest::ACapturedPresetNamesExactlyTheAddonsThatAreLinked()
{
    const Disk disk;
    PutALibraryOnDisk(disk);
    PutFoldersTheAppDoesNotOwnInTheDestination(disk, 7);

    Composition composed{.journalFile = disk.Root() / "journal" / "operations.jsonl",
                         .presetRoot = disk.Root() / "presets"};
    const SimulatorProfile profile = disk.Profile();

    ReallyEnable(composed, disk, "Aircrafts", 0);
    ReallyEnable(composed, disk, "Sceneries", 2);
    ReallyEnable(composed, disk, "Utils", 1);

    const ProfileSnapshot snapshot = composed.profiles.Scan(profile);

    QCOMPARE(EnabledAddonFolders(snapshot.entries).size(), std::size_t{3});

    QVERIFY(composed.service.Create(profile, snapshot, "Voo curto"));

    const std::optional<Preset> stored = composed.service.Load(profile.id, "Voo curto");

    QVERIFY(stored.has_value());
    QCOMPARE(stored->entries.size(), std::size_t{3});
}

void PresetOnRealDiskTest::UpdatingACapturedPresetKeepsNamingOnlyWhatIsLinked()
{
    const Disk disk;
    PutALibraryOnDisk(disk);
    PutFoldersTheAppDoesNotOwnInTheDestination(disk, 7);

    Composition composed{.journalFile = disk.Root() / "journal" / "operations.jsonl",
                         .presetRoot = disk.Root() / "presets"};
    const SimulatorProfile profile = disk.Profile();

    ReallyEnable(composed, disk, "Aircrafts", 0);
    ReallyEnable(composed, disk, "Sceneries", 2);

    QVERIFY(composed.service.Create(profile, composed.profiles.Scan(profile), "Voo curto"));

    ReallyEnable(composed, disk, "Utils", 1);
    ReallyEnable(composed, disk, "Utils", 3);

    const ProfileSnapshot snapshot = composed.profiles.Scan(profile);

    QCOMPARE(EnabledAddonFolders(snapshot.entries).size(), std::size_t{4});

    QVERIFY(composed.service.Update(profile, snapshot, "Voo curto"));

    const std::optional<Preset> stored = composed.service.Load(profile.id, "Voo curto");

    QVERIFY(stored.has_value());
    QCOMPARE(stored->entries.size(), std::size_t{4});
}

void PresetOnRealDiskTest::ApplyingAFreshlyCapturedPresetLeavesTheDestinationAsItWas()
{
    const Disk disk;
    PutALibraryOnDisk(disk);
    PutFoldersTheAppDoesNotOwnInTheDestination(disk, 7);

    Composition composed{.journalFile = disk.Root() / "journal" / "operations.jsonl",
                         .presetRoot = disk.Root() / "presets"};
    const SimulatorProfile profile = disk.Profile();

    ReallyEnable(composed, disk, "Aircrafts", 0);
    ReallyEnable(composed, disk, "Sceneries", 2);
    ReallyEnable(composed, disk, "Utils", 1);

    const ProfileSnapshot snapshot = composed.profiles.Scan(profile);
    QVERIFY(composed.service.Create(profile, snapshot, "Voo curto"));

    const std::optional<Preset> stored = composed.service.Load(profile.id, "Voo curto");
    QVERIFY(stored.has_value());

    const PresetPlan plan =
        PlanPresetApplication(*stored, ApplyMode::Replace, profile, snapshot.libraries, snapshot.enabled);

    QCOMPARE(plan.toEnable.size(), std::size_t{0});
    QCOMPARE(plan.toDisable.size(), std::size_t{0});
    QCOMPARE(plan.unresolved.size(), std::size_t{0});
    QCOMPARE(plan.alreadyInPlace.size(), std::size_t{3});

    const std::size_t before = ChildrenOf(disk.Community());

    const PresetApplyReport report = composed.service.Apply(profile, snapshot, *stored, ApplyMode::Replace);

    QCOMPARE(report.results.size(), std::size_t{0});
    QCOMPARE(ChildrenOf(disk.Community()), before);
}

namespace
{
    std::vector<std::string> LinkNamesIn(const std::filesystem::path& folder)
    {
        std::vector<std::string> names;

        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            names.push_back(entry.path().filename().string());
        }

        std::ranges::sort(names);

        return names;
    }
}

void PresetOnRealDiskTest::TheReturnPresetLandsOnDiskWithWhatWasEnabledAndStaysOutOfTheList()
{
    const Disk disk;
    PutALibraryOnDisk(disk);

    Composition composed{.journalFile = disk.Root() / "journal" / "operations.jsonl",
                         .presetRoot = disk.Root() / "presets"};
    const SimulatorProfile profile = disk.Profile();

    ReallyEnable(composed, disk, "Aircrafts", 0);
    ReallyEnable(composed, disk, "Sceneries", 2);

    QVERIFY(composed.service.Create(profile, composed.profiles.Scan(profile), "Voo curto"));

    const std::optional<Preset> stored = composed.service.Load(profile.id, "Voo curto");
    QVERIFY(stored.has_value());

    ReallyEnable(composed, disk, "Utils", 1);

    const PresetApplyReport report =
        composed.service.Apply(profile, composed.profiles.Scan(profile), *stored, ApplyMode::Replace);

    QVERIFY(report.refusal == PresetApplyRefusal::None);
    QVERIFY(std::filesystem::exists(disk.Root() / "presets" / "msfs2024.return.json"));

    const std::optional<Preset> back = composed.service.ReturnPreset(profile.id);

    QVERIFY(back.has_value());
    QCOMPARE(back->entries.size(), std::size_t{3});
    QCOMPARE(composed.service.List(profile.id).size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(composed.service.List(profile.id).front().name), QString{"Voo curto"});
}

void PresetOnRealDiskTest::ApplyingTheReturnPresetPutsTheDestinationBackAsItWas()
{
    const Disk disk;
    PutALibraryOnDisk(disk);
    PutFoldersTheAppDoesNotOwnInTheDestination(disk, 3);

    Composition composed{.journalFile = disk.Root() / "journal" / "operations.jsonl",
                         .presetRoot = disk.Root() / "presets"};
    const SimulatorProfile profile = disk.Profile();

    ReallyEnable(composed, disk, "Aircrafts", 0);
    ReallyEnable(composed, disk, "Sceneries", 2);
    ReallyEnable(composed, disk, "Utils", 1);

    const std::vector<std::string> before = LinkNamesIn(disk.Community());

    Preset preset;
    preset.name = "Só um";
    preset.entries = {PresetEntry{.addonId = AddonId{.libraryId = "lib-1", .folderName = AddonNameAt("Utils", 3)},
                                  .action = PresetAction::Enable}};

    static_cast<void>(composed.service.Apply(profile, composed.profiles.Scan(profile), preset, ApplyMode::Replace));

    QVERIFY(LinkNamesIn(disk.Community()) != before);

    const std::optional<Preset> back = composed.service.ReturnPreset(profile.id);
    QVERIFY(back.has_value());

    static_cast<void>(composed.service.Apply(profile, composed.profiles.Scan(profile), *back, ApplyMode::Replace));

    QCOMPARE(LinkNamesIn(disk.Community()), before);
}

QTEST_APPLESS_MAIN(PresetOnRealDiskTest)

#include "tst_preset_on_real_disk.moc"
