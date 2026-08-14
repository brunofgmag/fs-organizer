#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "application/BisectionService.h"
#include "domain/importing/ImportPaths.h"
#include "tests/doubles/FakeBisectionStore.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakePresetRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class BisectionServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheUnitsAreBuiltFromWhatIsEnabledAtThatInstant();
        static void BeginningWithNothingEnabledRefuses();
        static void TheFirstApplicationIsTheReferenceRoundAndTurnsEverythingOff();
        static void TheStateIsWrittenBeforeTheReferenceRoundTouchesADisk();
        static void NothingIsAppliedWhenTheStateCannotBeWritten();
        static void NoRoundEverTurnsOnEverySuspect();
        static void NoRoundOfTheFirstPassTurnsOnPartOfAGroup();
        static void TheRoundGoesOutAsOneBatchBecauseUndoingItPutsBackBothHalves();
        static void AJunctionDeletedBetweenTwoRoundsIsCaughtBeforeAnythingIsWritten();
        static void AFolderThatAppearedInTheDestinationIsCaughtTheSameWay();
        static void AReferenceRoundThatCrashesEndsWithTheCauseOutOfReachAndNamesNoCulprit();
        static void TheWholeProcedureNeverWritesTheReturnPreset();
        static void StoppingPutsBackWhatWasEnabledAndForgetsTheProcedure();
        static void AProcedureLeftHalfwayIsOfferedAgainWithTheThreeWaysOut();
        static void TheProcedureOfOneProfileIsNotOfferedInAnother();
        static void TheSecondPassKeepsTheBaseOnInEveryRound();
        static void WhatCarriesOnOutOfReachCountsManifestsAndNotBareFolders();
    };
}

namespace
{
    constexpr auto kLibrary = "D:/MSFS 2024";
    constexpr auto kCommunity = "E:/Flight Simulator 2024/Community";
    constexpr auto kLibraryId = "library-1";
    constexpr auto kProfileId = "msfs2024";

    constexpr auto kCrj = "D:/MSFS 2024/Aircrafts/aerosoft-crj";
    constexpr auto kFenix = "D:/MSFS 2024/Aircrafts/fenix-a320";
    constexpr auto kMd11 = "D:/MSFS 2024/Aircrafts/md11-base";
    constexpr auto kPmdg = "D:/MSFS 2024/Aircrafts/pmdg-aircraft-77w";
    constexpr auto kLivery = "D:/MSFS 2024/Liveries/md11-livery";
    constexpr auto kLiveryTwo = "D:/MSFS 2024/Liveries/md11-livery-two";

    constexpr auto kModel = "TFDi_Design_MD-11";
    constexpr auto kASatellite = R"([VARIATION]
base_container = "..\TFDi_Design_MD-11F_PW"
)";

    TreeNode AddonNode(const std::filesystem::path& path)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = Manifest{}};

        return node;
    }

    TreeNode CategoryNode(const std::filesystem::path& path, std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Category;
        node.path = path;
        node.children = std::move(children);

        return node;
    }

    TreeNode LibraryTree()
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = kLibrary;
        library.children = {CategoryNode("D:/MSFS 2024/Aircrafts",
                                         {AddonNode(kCrj), AddonNode(kFenix), AddonNode(kMd11), AddonNode(kPmdg)}),
                            CategoryNode("D:/MSFS 2024/Liveries", {AddonNode(kLivery), AddonNode(kLiveryTwo)})};

        return library;
    }

    SimulatorProfile Profile(const std::string& id = kProfileId)
    {
        SimulatorProfile profile;
        profile.id = id;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = kLibraryId, .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    [[nodiscard]] std::filesystem::path LinkFor(const std::filesystem::path& addon)
    {
        return PathUnder(kCommunity, addon.filename());
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);

            for (const std::filesystem::path& addon : {kCrj, kFenix, kMd11, kPmdg, kLivery, kLiveryTwo})
            {
                fileSystem.AddDirectory(addon);
                fileSystem.AddFileWithContents(ManifestPathIn(addon), "{}");
            }

            PutTheModelFolderIn(kMd11, false);
            PutTheModelFolderIn(kLivery, true);
            PutTheModelFolderIn(kLiveryTwo, true);

            catalog.SetTree(kLibrary, LibraryTree());
        }

        void PutTheModelFolderIn(const std::filesystem::path& addon, const bool satellite)
        {
            const std::string model = "SimObjects/Airplanes/" + std::string(kModel);

            for (const std::string& level : {std::string("SimObjects"), std::string("SimObjects/Airplanes"), model})
            {
                fileSystem.AddDirectory(PathUnder(addon, PathFromUtf8(level)));
            }

            if (!satellite)
            {
                return;
            }

            for (const std::string& level :
                 {model + "/liveries", model + "/liveries/vendor", model + "/liveries/vendor/one"})
            {
                fileSystem.AddDirectory(PathUnder(addon, PathFromUtf8(level)));
            }

            fileSystem.AddFileWithContents(PathUnder(addon, PathFromUtf8(model + "/liveries/vendor/one/livery.cfg")),
                                           kASatellite);
        }

        void Enable(const std::vector<std::filesystem::path>& addons)
        {
            for (const std::filesystem::path& addon : addons)
            {
                fileSystem.AddLink(LinkFor(addon), addon);
            }
        }

        [[nodiscard]] ProfileSnapshot Snapshot(const SimulatorProfile& profile) const
        {
            return profiles.Scan(profile);
        }

        [[nodiscard]] std::vector<std::filesystem::path> WhatIsOn() const
        {
            std::vector<std::filesystem::path> on;

            for (const std::filesystem::path& addon : {kCrj, kFenix, kMd11, kPmdg, kLivery, kLiveryTwo})
            {
                if (fileSystem.IsLink(LinkFor(addon)))
                {
                    on.push_back(addon);
                }
            }

            return on;
        }

        InMemoryFileSystem fileSystem;

        FakeSidecarStore sidecars{fileSystem};
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeCatalogScanner catalog;
        FakeOperationJournal journal;
        FakeClock clock;
        OperationLog log{journal, clock};
        FakeLibraryIdGenerator identities;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        StartupOverFakes startup{filesystemProbe};

        ProfileService profiles{catalog, filesystemProbe, sidecars,        classifier,        linking,
                                log,     identities,      startup.service, LinkType::Junction};
        CouplingScan coupling{filesystemProbe};
        FakeBisectionStore store;
        FakePresetRepository presets;
        BisectionService service{profiles, coupling, filesystemProbe, store};
    };

    [[nodiscard]] bool Holds(const std::vector<std::filesystem::path>& where, const std::filesystem::path& what)
    {
        return std::ranges::find(where, what) != where.end();
    }
}

void BisectionServiceTest::TheUnitsAreBuiltFromWhatIsEnabledAtThatInstant()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    const BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));

    QCOMPARE(report.refusal, BisectionRefusal::None);
    QCOMPARE(report.units, std::size_t{3});
    QCOMPARE(report.roundsInTheWorstCase, std::size_t{2});

    const std::optional<BisectionRun> run = f.store.Load(kProfileId);

    QVERIFY(run.has_value());
    QCOMPARE(run->units.size(), std::size_t{3});
    QVERIFY(!Holds(run->units.back().addons, kPmdg));
    QCOMPARE(run->units.back().addons.size(), std::size_t{2});
}

void BisectionServiceTest::BeginningWithNothingEnabledRefuses()
{
    Fixture f;
    const SimulatorProfile profile = Profile();

    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::NothingIsEnabledToSearch);
    QVERIFY(!f.store.Load(kProfileId).has_value());
}

void BisectionServiceTest::TheFirstApplicationIsTheReferenceRoundAndTurnsEverythingOff()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    const BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));

    QCOMPARE(report.round, std::size_t{0});
    QVERIFY(report.addonsTurnedOn.empty());
    QVERIFY(f.WhatIsOn().empty());
}

void BisectionServiceTest::TheStateIsWrittenBeforeTheReferenceRoundTouchesADisk()
{
    Fixture f;
    f.Enable({kCrj, kFenix});

    const SimulatorProfile profile = Profile();
    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);

    const std::optional<BisectionRun> run = f.store.Load(kProfileId);

    QVERIFY(run.has_value());
    QCOMPARE(run->startingConfiguration.size(), std::size_t{2});
    QVERIFY(run->startingConfiguration.front().action == PresetAction::Enable);
}

void BisectionServiceTest::NothingIsAppliedWhenTheStateCannotBeWritten()
{
    Fixture f;
    f.Enable({kCrj, kFenix});
    f.store.RefuseEveryWrite();

    const SimulatorProfile profile = Profile();
    const BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));

    QCOMPARE(report.refusal, BisectionRefusal::TheStateCouldNotBeWritten);
    QCOMPARE(f.WhatIsOn().size(), std::size_t{2});
    QVERIFY(f.journal.appended.empty());
}

void BisectionServiceTest::NoRoundEverTurnsOnEverySuspect()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery, kPmdg});

    const SimulatorProfile profile = Profile();
    BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));
    report = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    while (report.outcome == BisectionOutcome::StillSearching)
    {
        const std::optional<BisectionRun> run = f.store.Load(kProfileId);

        QVERIFY(run.has_value());
        QVERIFY(!report.addonsTurnedOn.empty());
        QVERIFY2(report.addonsTurnedOn.size() < f.WhatIsOn().size() + report.addonsTurnedOn.size(),
                 "the round turned on everything that was under suspicion");

        std::size_t suspectAddons = 0;

        for (const std::size_t suspect : run->suspects)
        {
            suspectAddons += run->units[suspect].addons.size();
        }

        QVERIFY2(report.addonsTurnedOn.size() < suspectAddons, "the round turned on every suspect");

        report = f.service.Answer(profile, BisectionAnswer::ItCrashed);
    }

    QCOMPARE(report.outcome, BisectionOutcome::OneAddonLeft);
}

void BisectionServiceTest::NoRoundOfTheFirstPassTurnsOnPartOfAGroup()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery, kPmdg});

    const SimulatorProfile profile = Profile();
    BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));
    report = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    while (report.outcome == BisectionOutcome::StillSearching)
    {
        QCOMPARE(Holds(report.addonsTurnedOn, kMd11), Holds(report.addonsTurnedOn, kLivery));
        QCOMPARE(Holds(f.WhatIsOn(), kMd11), Holds(f.WhatIsOn(), kLivery));

        report = f.service.Answer(profile, BisectionAnswer::ItRanFine);
    }
}

void BisectionServiceTest::TheRoundGoesOutAsOneBatchBecauseUndoingItPutsBackBothHalves()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);

    const BisectionReport first = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    QCOMPARE(first.addonsTurnedOn.size(), std::size_t{1});
    QCOMPARE(f.WhatIsOn().size(), std::size_t{1});

    const BisectionReport second = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    QVERIFY(!second.results.empty());
    QCOMPARE(f.profiles.UndoLastBatch().size(), second.results.size());
    QCOMPARE(f.WhatIsOn(), std::vector<std::filesystem::path>{kCrj});
}

void BisectionServiceTest::AJunctionDeletedBetweenTwoRoundsIsCaughtBeforeAnythingIsWritten()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);
    QCOMPARE(f.service.Answer(profile, BisectionAnswer::ItRanFine).addonsTurnedOn.size(), std::size_t{1});

    QVERIFY(f.fileSystem.RemoveTree(LinkFor(kCrj)));

    const std::size_t saves = f.store.saves;
    const std::size_t records = f.journal.appended.size();
    const BisectionReport report = f.service.Answer(profile, BisectionAnswer::ItCrashed);

    QCOMPARE(report.refusal, BisectionRefusal::TheDiskMovedSinceTheLastRound);
    QCOMPARE(report.drift.size(), std::size_t{1});
    QCOMPARE(report.drift.front().kind, DriftKind::ALinkWeLeftIsGone);
    QCOMPARE(f.store.saves, saves);
    QCOMPARE(f.journal.appended.size(), records);
}

void BisectionServiceTest::AFolderThatAppearedInTheDestinationIsCaughtTheSameWay()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);

    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/somebody-elses-folder");

    const BisectionReport report = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    QCOMPARE(report.refusal, BisectionRefusal::TheDiskMovedSinceTheLastRound);
    QCOMPARE(report.drift.size(), std::size_t{1});
    QCOMPARE(report.drift.front().kind, DriftKind::AnEntryWeDidNotLeaveIsThere);
}

void BisectionServiceTest::AReferenceRoundThatCrashesEndsWithTheCauseOutOfReachAndNamesNoCulprit()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/somebody-elses-addon");
    f.fileSystem.AddFileWithContents(ManifestPathIn("E:/Flight Simulator 2024/Community/somebody-elses-addon"), "{}");

    const SimulatorProfile profile = Profile();
    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);

    const BisectionReport report = f.service.Answer(profile, BisectionAnswer::ItCrashed);

    QCOMPARE(report.outcome, BisectionOutcome::NotAmongTheManagedOnes);
    QVERIFY(report.whatIsLeft.empty());
    QCOMPARE(report.outOfReach, std::size_t{1});
    QVERIFY(!report.aSecondPassIsPossible);
}

void BisectionServiceTest::TheWholeProcedureNeverWritesTheReturnPreset()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    Preset theWayBack;
    theWayBack.name = "return";
    theWayBack.entries = {PresetEntry{.addonId = AddonId{.libraryId = kLibraryId, .folderName = "fenix-a320"},
                                      .action = PresetAction::Enable}};
    QVERIFY(f.presets.SaveReturnPreset(kProfileId, theWayBack));

    const SimulatorProfile profile = Profile();
    BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));
    report = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    while (report.outcome == BisectionOutcome::StillSearching)
    {
        report = f.service.Answer(profile, BisectionAnswer::ItCrashed);
    }

    QCOMPARE(f.service.Stop(profile).refusal, BisectionRefusal::None);

    const std::optional<Preset> after = f.presets.LoadReturnPreset(kProfileId);

    QVERIFY(after.has_value());
    QCOMPARE(after->entries.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(after->entries.front().addonId.folderName), QString{"fenix-a320"});
    QCOMPARE(QString::fromStdString(after->name), QString{"return"});
}

void BisectionServiceTest::StoppingPutsBackWhatWasEnabledAndForgetsTheProcedure()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    const std::vector<std::filesystem::path> before = f.WhatIsOn();

    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);
    QVERIFY(f.WhatIsOn().empty());
    QCOMPARE(f.service.Answer(profile, BisectionAnswer::ItRanFine).refusal, BisectionRefusal::None);

    QCOMPARE(f.service.Stop(profile).refusal, BisectionRefusal::None);
    QCOMPARE(f.WhatIsOn(), before);
    QVERIFY(!f.store.Load(kProfileId).has_value());
}

void BisectionServiceTest::AProcedureLeftHalfwayIsOfferedAgainWithTheThreeWaysOut()
{
    Fixture f;
    f.Enable({kCrj, kFenix, kMd11, kLivery});

    const SimulatorProfile profile = Profile();
    const std::vector<std::filesystem::path> before = f.WhatIsOn();

    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).refusal, BisectionRefusal::None);
    QCOMPARE(f.service.Answer(profile, BisectionAnswer::ItRanFine).refusal, BisectionRefusal::None);

    BisectionService reopened{f.profiles, f.coupling, f.filesystemProbe, f.store};

    QVERIFY(reopened.WhatWasInterrupted(kProfileId).has_value());
    QCOMPARE(reopened.Resume(profile, ResumeChoice::PutBackTheStartingConfiguration).refusal, BisectionRefusal::None);
    QCOMPARE(f.WhatIsOn(), before);
    QVERIFY(!f.store.Load(kProfileId).has_value());
}

void BisectionServiceTest::TheProcedureOfOneProfileIsNotOfferedInAnother()
{
    Fixture f;
    f.Enable({kCrj, kFenix});

    QCOMPARE(f.service.Begin(Profile(), f.Snapshot(Profile())).refusal, BisectionRefusal::None);

    QVERIFY(f.service.WhatWasInterrupted(kProfileId).has_value());
    QVERIFY(!f.service.WhatWasInterrupted("msfs2020").has_value());
}

void BisectionServiceTest::TheSecondPassKeepsTheBaseOnInEveryRound()
{
    Fixture f;
    f.Enable({kMd11, kLivery, kLiveryTwo, kCrj});

    const SimulatorProfile profile = Profile();
    BisectionReport report = f.service.Begin(profile, f.Snapshot(profile));
    report = f.service.Answer(profile, BisectionAnswer::ItRanFine);

    while (report.outcome == BisectionOutcome::StillSearching)
    {
        report = f.service.Answer(profile, BisectionAnswer::ItRanFine);
    }

    QCOMPARE(report.outcome, BisectionOutcome::AnIrreducibleSet);
    QCOMPARE(report.whatIsLeft.size(), std::size_t{3});
    QVERIFY(report.aSecondPassIsPossible);

    report = f.service.Refine(profile);

    QCOMPARE(report.refusal, BisectionRefusal::None);
    QVERIFY(Holds(report.addonsTurnedOn, kMd11));
    QVERIFY(Holds(f.WhatIsOn(), kMd11));

    while (report.outcome == BisectionOutcome::StillSearching)
    {
        QVERIFY2(Holds(f.WhatIsOn(), kMd11), "a round of the second pass ran without the base");

        report = f.service.Answer(profile, BisectionAnswer::ItCrashed);
    }

    QCOMPARE(report.outcome, BisectionOutcome::OneAddonLeft);
    QCOMPARE(report.whatIsLeft, std::vector<std::filesystem::path>{kLivery});
}

void BisectionServiceTest::WhatCarriesOnOutOfReachCountsManifestsAndNotBareFolders()
{
    Fixture f;
    f.Enable({kCrj});
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/carries");
    f.fileSystem.AddFileWithContents(ManifestPathIn("E:/Flight Simulator 2024/Community/carries"), "{}");
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community/will-not-load");

    const SimulatorProfile profile = Profile();

    QCOMPARE(f.service.Begin(profile, f.Snapshot(profile)).outOfReach, std::size_t{1});
}

QTEST_APPLESS_MAIN(BisectionServiceTest)

#include "tst_bisection_service.moc"
