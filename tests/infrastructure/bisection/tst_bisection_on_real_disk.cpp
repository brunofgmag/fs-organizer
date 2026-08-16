#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "application/BisectionService.h"
#include "domain/model/Manifest.h"
#include "infrastructure/bisection/JsonBisectionStore.h"
#include "domain/ports/ImportedFolders.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/fileops/WindowsSidecarStore.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const NothingWasImported nothingWasImported;

    class BisectionOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheStateIsOnDiskBeforeTheReferenceRoundTakesTheLinksDown();
        static void AJudgementSurvivesTheRoundTripThroughTheFile();
        static void KilledBetweenTheStateAndTheLinksCarryingOnPutsTheDiskWhereTheFileSaysItIs();
        static void KilledBetweenTheStateAndTheLinksPuttingBackRestoresWhatWasOn();
        static void KilledBetweenTheStateAndTheLinksForgettingLeavesTheDiskAsItIs();
        static void TheFileStopsExistingWhenTheProcedureEnds();
        static void ADriftMadeWhileTheAppWasDownDoesNotStopTheResume();
        static void AFreshProcessAnsweringWithoutABaselineDoesNotCallTheWholeDiskADrift();
        static void TheStorySurvivesTheRoundTripThroughTheFile();
        static void ThePartitionOfAGroupSurvivesTheRoundTripThroughTheFile();
    };
}

namespace
{
    constexpr auto kProfileId = "msfs2024";
    constexpr auto kLibraryId = "library-1";
    const std::string kManifest = R"({"title": "An addon", "package_version": "1.0.0"})";

    const std::vector<std::string> kAddons = {"aerosoft-crj", "fenix-a320", "pmdg-aircraft-77w", "inibuilds-a350"};

    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path Library() const
        {
            return Root() / "Library";
        }

        [[nodiscard]] std::filesystem::path Community() const
        {
            return Root() / "Community";
        }

        [[nodiscard]] std::filesystem::path State() const
        {
            return Root() / "State";
        }

        [[nodiscard]] std::filesystem::path Addon(const std::string& name) const
        {
            return Library() / "Aircrafts" / name;
        }

        [[nodiscard]] std::filesystem::path Link(const std::string& name) const
        {
            return Community() / name;
        }

        Disk()
        {
            std::filesystem::create_directories(Community());
            std::filesystem::create_directories(State());

            for (const std::string& name : kAddons)
            {
                std::filesystem::create_directories(Addon(name));
                std::ofstream(ManifestPathIn(Addon(name)), std::ios::binary) << kManifest;
            }
        }

        [[nodiscard]] std::vector<std::string> WhatIsOn() const
        {
            std::vector<std::string> on;

            for (const std::string& name : kAddons)
            {
                if (std::filesystem::exists(Link(name)))
                {
                    on.push_back(name);
                }
            }

            return on;
        }
    };

    struct World
    {
        explicit World(const Disk& disk) : store(disk.State())
        {
        }

        JsonManifestParser manifestParser;
        WindowsFilesystemProbe filesystemProbe;
        WindowsSidecarStore sidecars;
        WindowsLinkService linkService;
        FilesystemScanner scanner{manifestParser, filesystemProbe, nothingWasImported};
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{scanner, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        CouplingScan coupling{filesystemProbe};
        JsonBisectionStore store;
        BisectionService service{profiles, coupling, filesystemProbe, store, clock};
    };

    SimulatorProfile ProfileOn(const Disk& disk)
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {disk.Community()};
        profile.defaultDestination = disk.Community();
        profile.libraries = {Library{.id = kLibraryId, .path = disk.Library(), .label = "Library"}};

        return profile;
    }

    void TurnEverythingOn(const Disk& disk, World& world, const SimulatorProfile& profile)
    {
        const ProfileSnapshot snapshot = world.profiles.Scan(profile);
        std::vector<const TreeNode*> nodes;

        for (const TreeNode& category : snapshot.libraries.front().children)
        {
            for (const TreeNode& addon : category.children)
            {
                nodes.push_back(&addon);
            }
        }

        QCOMPARE(world.profiles.SetEnabled(profile, snapshot, nodes, true).results.size(), kAddons.size());
        QCOMPARE(disk.WhatIsOn().size(), kAddons.size());
    }

    [[nodiscard]] BisectionRun TheProcessDiesBetweenTheStateAndTheLinks(World& world, const SimulatorProfile& profile)
    {
        const std::optional<BisectionRun> run = world.store.Load(profile.id);

        const BisectionRun next = AfterAnswering(*run, BisectionAnswer::ItRanFine, world.clock.Now());

        return next;
    }
}

void BisectionOnRealDiskTest::TheStateIsOnDiskBeforeTheReferenceRoundTakesTheLinksDown()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);

    const std::optional<std::filesystem::path> file = world.store.FileOf(kProfileId);

    QVERIFY(file.has_value());
    QVERIFY2(std::filesystem::exists(*file), "the state file is not on disk after the reference round");
    QVERIFY(disk.WhatIsOn().empty());
}

void BisectionOnRealDiskTest::AJudgementSurvivesTheRoundTripThroughTheFile()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);
    QCOMPARE(world.service.Answer(profile, BisectionAnswer::ItRanFine).refusal, BisectionRefusal::None);

    const std::optional<BisectionRun> written = world.store.Load(kProfileId);

    QVERIFY(written.has_value());
    QCOMPARE(QString::fromStdString(written->profileId), QString{kProfileId});
    QCOMPARE(written->units.size(), kAddons.size());
    QCOMPARE(written->round, std::size_t{1});
    QCOMPARE(written->startingConfiguration.size(), kAddons.size());
    QCOMPARE(written->pass, BisectionPass::OverTheUnits);
    QVERIFY(!written->theReferenceRoundCrashed);
    QVERIFY(written->startedAt.time_since_epoch().count() != 0);
}

void BisectionOnRealDiskTest::TheStorySurvivesTheRoundTripThroughTheFile()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);

    world.clock.now += std::chrono::minutes{11};
    QCOMPARE(world.service.Answer(profile, BisectionAnswer::ItRanFine).refusal, BisectionRefusal::None);

    world.clock.now += std::chrono::minutes{6};
    QCOMPARE(world.service.Answer(profile, BisectionAnswer::ItCrashed).refusal, BisectionRefusal::None);

    const std::optional<BisectionRun> inMemory = world.store.Load(kProfileId);

    QVERIFY2(inMemory->story.front().at != inMemory->story.back().at,
             "the file kept one instant for every round, so it is not the round's own");

    QVERIFY(inMemory.has_value());
    QCOMPARE(inMemory->story.size(), std::size_t{2});

    World reopened(disk);
    const std::optional<BisectionRun> reread = reopened.store.Load(kProfileId);

    QVERIFY(reread.has_value());
    QCOMPARE(reread->story.size(), inMemory->story.size());

    for (std::size_t entry = 0; entry < reread->story.size(); ++entry)
    {
        QCOMPARE(reread->story[entry].number, inMemory->story[entry].number);
        QCOMPARE(reread->story[entry].pass, inMemory->story[entry].pass);
        QCOMPARE(reread->story[entry].unitsOn, inMemory->story[entry].unitsOn);
        QCOMPARE(reread->story[entry].answer, inMemory->story[entry].answer);
        QCOMPARE(reread->story[entry].unitsCleared, inMemory->story[entry].unitsCleared);
        QCOMPARE(reread->story[entry].unitsLeft, inMemory->story[entry].unitsLeft);
        QCOMPARE(reread->story[entry].at, inMemory->story[entry].at);
    }

    QCOMPARE(reread->story.front().number, std::size_t{0});
    QCOMPARE(reread->story.front().answer, BisectionAnswer::ItRanFine);
    QCOMPARE(reread->story.back().answer, BisectionAnswer::ItCrashed);
}

void BisectionOnRealDiskTest::KilledBetweenTheStateAndTheLinksCarryingOnPutsTheDiskWhereTheFileSaysItIs()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);

    const BisectionRun next = TheProcessDiesBetweenTheStateAndTheLinks(world, profile);
    QVERIFY(world.store.Save(kProfileId, next));
    QVERIFY(disk.WhatIsOn().empty());

    World reopened(disk);
    const std::optional<BisectionRun> offered = reopened.service.WhatWasInterrupted(kProfileId);

    QVERIFY(offered.has_value());
    QCOMPARE(offered->round, std::size_t{1});

    const BisectionReport report = reopened.service.Resume(profile, ResumeChoice::CarryOnFromWhereItStopped);

    QCOMPARE(report.refusal, BisectionRefusal::None);
    QCOMPARE(report.addonsTurnedOn.size(), kAddons.size() / 2);
    QCOMPARE(disk.WhatIsOn().size(), kAddons.size() / 2);
}

void BisectionOnRealDiskTest::KilledBetweenTheStateAndTheLinksPuttingBackRestoresWhatWasOn()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);
    QVERIFY(world.store.Save(kProfileId, TheProcessDiesBetweenTheStateAndTheLinks(world, profile)));

    World reopened(disk);

    QCOMPARE(reopened.service.Resume(profile, ResumeChoice::PutBackTheStartingConfiguration).refusal,
             BisectionRefusal::None);
    QCOMPARE(disk.WhatIsOn().size(), kAddons.size());
    QVERIFY(!std::filesystem::exists(*reopened.store.FileOf(kProfileId)));
}

void BisectionOnRealDiskTest::KilledBetweenTheStateAndTheLinksForgettingLeavesTheDiskAsItIs()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);
    QVERIFY(world.store.Save(kProfileId, TheProcessDiesBetweenTheStateAndTheLinks(world, profile)));

    World reopened(disk);

    QCOMPARE(reopened.service.Resume(profile, ResumeChoice::ForgetItAndLeaveTheDiskAsItIs).refusal,
             BisectionRefusal::None);
    QVERIFY(disk.WhatIsOn().empty());
    QVERIFY(!std::filesystem::exists(*reopened.store.FileOf(kProfileId)));
    QVERIFY(!reopened.service.WhatWasInterrupted(kProfileId).has_value());
}

void BisectionOnRealDiskTest::TheFileStopsExistingWhenTheProcedureEnds()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);
    QVERIFY(std::filesystem::exists(*world.store.FileOf(kProfileId)));

    QCOMPARE(world.service.Stop(profile).refusal, BisectionRefusal::None);

    QVERIFY(!std::filesystem::exists(*world.store.FileOf(kProfileId)));
    QCOMPARE(disk.WhatIsOn().size(), kAddons.size());
}

void BisectionOnRealDiskTest::ADriftMadeWhileTheAppWasDownDoesNotStopTheResume()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);
    QVERIFY(world.store.Save(kProfileId, TheProcessDiesBetweenTheStateAndTheLinks(world, profile)));

    std::filesystem::create_directories(disk.Link("somebody-elses-folder"));

    World reopened(disk);

    QCOMPARE(reopened.service.Resume(profile, ResumeChoice::CarryOnFromWhereItStopped).refusal, BisectionRefusal::None);
    QCOMPARE(disk.WhatIsOn().size(), kAddons.size() / 2);
    QVERIFY(std::filesystem::exists(disk.Link("somebody-elses-folder")));

    QCOMPARE(reopened.service.Answer(profile, BisectionAnswer::ItRanFine).refusal, BisectionRefusal::None);
}

void BisectionOnRealDiskTest::AFreshProcessAnsweringWithoutABaselineDoesNotCallTheWholeDiskADrift()
{
    const Disk disk;
    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);

    World reopened(disk);
    const BisectionReport report = reopened.service.Answer(profile, BisectionAnswer::ItRanFine);

    QVERIFY2(report.drift.empty(), "a process that never saw a round called the disk it found a drift");
    QCOMPARE(report.refusal, BisectionRefusal::None);
    QCOMPARE(disk.WhatIsOn().size(), kAddons.size() / 2);
}

void BisectionOnRealDiskTest::ThePartitionOfAGroupSurvivesTheRoundTripThroughTheFile()
{
    const Disk disk;
    const std::filesystem::path model = std::filesystem::path("SimObjects") / "Airplanes" / "Shared_Model";

    for (const std::string& name : {std::string("aerosoft-crj"), std::string("fenix-a320")})
    {
        std::filesystem::create_directories(disk.Addon(name) / model / "liveries");
    }

    std::filesystem::create_directories(disk.Addon("pmdg-aircraft-77w") / model / "common");

    World world(disk);
    const SimulatorProfile profile = ProfileOn(disk);
    TurnEverythingOn(disk, world, profile);

    QCOMPARE(world.service.Begin(profile, world.profiles.Scan(profile)).refusal, BisectionRefusal::None);

    const std::optional<BisectionRun> written = world.store.Load(kProfileId);

    QVERIFY(written.has_value());

    const auto group = std::ranges::find_if(written->units,
                                            [](const SearchUnit& unit)
                                            {
                                                return unit.addons.size() > 1;
                                            });

    QVERIFY2(group != written->units.end(), "the three addons under one model folder did not come out as a group");
    QCOMPARE(group->addons.size(), std::size_t{3});
    QCOMPARE(group->coupling, Coupling::OnlyTheSharedModelFolder);
    QCOMPARE(group->writingTogether.size(), std::size_t{1});
    QCOMPARE(group->writingTogether.front().size(), std::size_t{2});
    QCOMPARE(group->writingApart.size(), std::size_t{1});
    QCOMPARE(group->writingApart.front().filename(), std::filesystem::path{"pmdg-aircraft-77w"});
}

QTEST_APPLESS_MAIN(BisectionOnRealDiskTest)

#include "tst_bisection_on_real_disk.moc"
