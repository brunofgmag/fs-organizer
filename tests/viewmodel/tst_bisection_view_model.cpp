#include <QtTest/QtTest>

#include <filesystem>
#include <string>
#include <vector>

#include "domain/importing/ImportPaths.h"
#include "tests/doubles/FakeBisectionStore.h"
#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/FakeOperationJournal.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSidecarStore.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/doubles/StartupOverFakes.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/BisectionViewModel.h"
#include "viewmodel/SessionNotifier.h"

namespace
{
    class BisectionViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheWorstCaseIsCountedFromTheSetOfTheInstant();
        static void TheScreenSaysWhatItWillSearchBeforeTheFirstRound();
        static void ReopeningTheScreenMidProcedureShowsTheRoundItIsOn();
        static void StoppingGoesBackToTheScreenThatAnnouncesWhatWouldBeSearched();
        static void TurningOneMoreAddonOnBeforeStartingChangesTheAnnouncedNumber();
        static void NothingIsWrittenWhileTheQuestionIsStillOnTheScreen();
        static void TheThirdOutcomeSaysHowManyEntriesCarryOnOutOfReach();
        static void AProcedureLeftHalfwayIsOfferedOnTheNextOpening();
        static void AGroupSaysHowManyAddonsItCarriesAndOfWhatKind();
        static void AGroupCarriesEveryMemberAndSaysWhichOneOnlyBringsTheName();
        static void AnAddonJoiningTheLibraryTakesTheScreenToItsOwnStageAndNotToTheDrift();
        static void CarryingOnAppliesTheAnswerThatWasHeldBackInsteadOfLosingIt();
        static void TheHeldAnswerIsForgottenOnceTheScreenLeavesThatStage();
        static void TheLaunchesAlreadyMadeCountTheReferenceRound();
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
    constexpr auto kOtherLivery = "D:/MSFS 2024/Liveries/md11-livery-two";

    constexpr auto kModel = "TFDi_Design_MD-11";

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
                            CategoryNode("D:/MSFS 2024/Liveries", {AddonNode(kLivery), AddonNode(kOtherLivery)})};

        return library;
    }

    TreeNode ALibraryTreeWithOneMore()
    {
        TreeNode library = LibraryTree();
        library.children.front().children.push_back(AddonNode("D:/MSFS 2024/Aircrafts/pmdg-aircraft-738"));

        return library;
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.id = kProfileId;
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {kCommunity};
        profile.defaultDestination = kCommunity;
        profile.libraries = {Library{.id = kLibraryId, .path = kLibrary, .label = "MSFS 2024"}};

        return profile;
    }

    struct Fixture
    {
        Fixture()
        {
            fileSystem.AddDirectory(kCommunity);

            for (const std::filesystem::path& addon : {kCrj, kFenix, kMd11, kPmdg, kLivery, kOtherLivery})
            {
                fileSystem.AddDirectory(addon);
                fileSystem.AddFileWithContents(ManifestPathIn(addon), "{}");
            }

            PutTheModelFolderIn(kMd11);
            PutTheModelFolderIn(kLivery);
            PutTheModelFolderIn(kOtherLivery);

            catalog.SetTree(kLibrary, LibraryTree());
        }

        void PutTheModelFolderIn(const std::filesystem::path& addon) const
        {
            const std::string model = "SimObjects/Airplanes/" + std::string(kModel);

            for (const std::string& level : {std::string("SimObjects"), std::string("SimObjects/Airplanes"), model})
            {
                fileSystem.AddDirectory(PathUnder(addon, PathFromUtf8(level)));
            }
        }

        void TurnOn(const std::filesystem::path& addon) const
        {
            fileSystem.AddLink(PathUnder(kCommunity, addon.filename()), addon);
        }

        void Seed() const
        {
            const SimulatorProfile profile = Profile();

            static_cast<void>(session.Rewrite(
                [&profile](AppSettings& settings)
                {
                    settings.profiles = {profile};
                    settings.activeProfileId = profile.id;

                    return true;
                }));

            session.ShowActiveProfile();
        }

        mutable InMemoryFileSystem fileSystem;
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
        FakeSidecarStore sidecars{fileSystem};
        ProfileService service{catalog, filesystemProbe, sidecars,        classifier,        linking,
                               log,     identities,      startup.service, LinkType::Junction};
        FakeFileOperations files{fileSystem};
        FakeProcessProbe processProbe;
        LibraryOrganizer organizer{catalog,    filesystemProbe, files, linking,
                                   classifier, processProbe,    log,   LinkType::Junction};
        FakeSettingsRepository settings;
        InlineBackgroundRunner runner;
        SessionNotifier notifier;
        mutable Session session{service, organizer, settings, settings.stored, processProbe, runner, notifier};
        CouplingScan coupling{filesystemProbe};
        FakeBisectionStore store;
        BisectionService bisection{service, coupling, filesystemProbe, store, clock};
        BisectionViewModel viewModel{bisection, session};
    };
}

void BisectionViewModelTest::TheWorstCaseIsCountedFromTheSetOfTheInstant()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();

    QCOMPARE(f.viewModel.Report().units, std::size_t{3});
    QCOMPARE(f.viewModel.Report().roundsInTheWorstCase, RoundsInTheWorstCase(3));
}

void BisectionViewModelTest::TheScreenSaysWhatItWillSearchBeforeTheFirstRound()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);

    const std::filesystem::path stranger = PathUnder(kCommunity, PathFromUtf8("someone-elses-addon"));

    f.fileSystem.AddDirectory(stranger);
    f.fileSystem.AddFileWithContents(ManifestPathIn(stranger), "{}");
    f.Seed();

    f.viewModel.Show();

    QCOMPARE(f.viewModel.Stage(), BisectionStage::NotStarted);
    QCOMPARE(f.viewModel.Report().units, std::size_t{3});
    QCOMPARE(f.viewModel.Report().roundsInTheWorstCase, RoundsInTheWorstCase(3));
    QCOMPARE(f.viewModel.Report().outOfReach, std::size_t{1});
    QCOMPARE(f.viewModel.WhatIsLeft().size(), std::size_t{3});
    QVERIFY(f.journal.appended.empty());
}

void BisectionViewModelTest::ReopeningTheScreenMidProcedureShowsTheRoundItIsOn()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();
    f.viewModel.Answer(BisectionAnswer::ItRanFine);

    const std::size_t round = f.viewModel.Report().round;
    const std::size_t turnedOn = f.viewModel.WhatToTurnOn().size();

    QVERIFY(round > 0);
    QVERIFY(turnedOn > 0);

    BisectionViewModel opened{f.bisection, f.session};
    opened.Show();

    QCOMPARE(opened.Stage(), BisectionStage::Asking);
    QCOMPARE(opened.Report().round, round);
    QCOMPARE(opened.WhatToTurnOn().size(), turnedOn);
    QCOMPARE(opened.Report().units, f.viewModel.Report().units);
}

void BisectionViewModelTest::StoppingGoesBackToTheScreenThatAnnouncesWhatWouldBeSearched()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Show();

    const BisectionReport announced = f.viewModel.Report();

    f.viewModel.Begin();
    f.viewModel.Stop();

    QCOMPARE(f.viewModel.Stage(), BisectionStage::NotStarted);
    QCOMPARE(f.viewModel.Report().units, announced.units);
    QCOMPARE(f.viewModel.Report().roundsInTheWorstCase, announced.roundsInTheWorstCase);
    QCOMPARE(f.viewModel.WhatIsLeft().size(), announced.unitsUnderSuspicion.size());
}

void BisectionViewModelTest::TurningOneMoreAddonOnBeforeStartingChangesTheAnnouncedNumber()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.Seed();

    f.viewModel.Begin();

    const std::size_t withTwo = f.viewModel.Report().roundsInTheWorstCase;

    QCOMPARE(f.viewModel.Report().units, std::size_t{2});

    f.viewModel.Stop();
    f.TurnOn(kPmdg);
    f.session.ShowActiveProfile();

    f.viewModel.Begin();

    QCOMPARE(f.viewModel.Report().units, std::size_t{3});
    QCOMPARE(withTwo, RoundsInTheWorstCase(2));
    QCOMPARE(f.viewModel.Report().roundsInTheWorstCase, RoundsInTheWorstCase(3));
    QVERIFY(f.viewModel.Report().roundsInTheWorstCase > withTwo);
}

void BisectionViewModelTest::NothingIsWrittenWhileTheQuestionIsStillOnTheScreen()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();

    const std::size_t afterTheReferenceRound = f.journal.appended.size();

    f.viewModel.Show();
    static_cast<void>(f.viewModel.WhatToTurnOn());
    static_cast<void>(f.viewModel.WhatIsLeft());
    static_cast<void>(f.viewModel.RoundsLeftInTheWorstCase());
    static_cast<void>(f.viewModel.Stage());

    QCOMPARE(f.journal.appended.size(), afterTheReferenceRound);

    f.viewModel.Answer(BisectionAnswer::ItRanFine);

    QVERIFY(f.journal.appended.size() > afterTheReferenceRound);
}

void BisectionViewModelTest::TheThirdOutcomeSaysHowManyEntriesCarryOnOutOfReach()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.fileSystem.AddDirectory(PathUnder(kCommunity, PathFromUtf8("someone-elses-addon")));
    f.fileSystem.AddFileWithContents(ManifestPathIn(PathUnder(kCommunity, PathFromUtf8("someone-elses-addon"))), "{}");
    f.Seed();

    f.viewModel.Begin();
    f.viewModel.Answer(BisectionAnswer::ItCrashed);

    QCOMPARE(f.viewModel.Report().outcome, BisectionOutcome::NotAmongTheManagedOnes);
    QCOMPARE(f.viewModel.Report().outOfReach, std::size_t{1});
    QCOMPARE(f.viewModel.Stage(), BisectionStage::Finished);
}

void BisectionViewModelTest::AProcedureLeftHalfwayIsOfferedOnTheNextOpening()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();

    QVERIFY(f.viewModel.AProcedureWasInterrupted());

    BisectionViewModel opened{f.bisection, f.session};
    opened.Show();

    QVERIFY(opened.AProcedureWasInterrupted());

    opened.Resume(ResumeChoice::PutBackTheStartingConfiguration);

    QVERIFY(!opened.AProcedureWasInterrupted());
}

void BisectionViewModelTest::AGroupSaysHowManyAddonsItCarriesAndOfWhatKind()
{
    Fixture f;
    f.TurnOn(kMd11);
    f.TurnOn(kLivery);
    f.TurnOn(kCrj);
    f.Seed();

    f.viewModel.Begin();
    f.viewModel.Answer(BisectionAnswer::ItRanFine);

    const std::vector<UnitOnScreen> left = f.viewModel.WhatIsLeft();

    QCOMPARE(left.size(), std::size_t{2});

    const auto group = std::ranges::find_if(left,
                                            [](const UnitOnScreen& unit)
                                            {
                                                return unit.addons > 1;
                                            });

    QVERIFY(group != left.end());
    QCOMPARE(group->addons, std::size_t{2});
    QVERIFY(group->coupling != Coupling::NotYetMeasured);
    QVERIFY(group->coupling != Coupling::Alone);
}

void BisectionViewModelTest::AGroupCarriesEveryMemberAndSaysWhichOneOnlyBringsTheName()
{
    Fixture f;
    const std::string under = "SimObjects/Airplanes/" + std::string(kModel);

    f.fileSystem.AddDirectory(PathUnder(kMd11, PathFromUtf8(under + "/common")));
    f.fileSystem.AddDirectory(PathUnder(kLivery, PathFromUtf8(under + "/liveries")));
    f.fileSystem.AddDirectory(PathUnder(kOtherLivery, PathFromUtf8(under + "/liveries")));

    f.TurnOn(kMd11);
    f.TurnOn(kLivery);
    f.TurnOn(kOtherLivery);
    f.Seed();

    f.viewModel.Show();

    const std::vector<UnitOnScreen> shown = f.viewModel.WhatIsLeft();

    QCOMPARE(shown.size(), std::size_t{1});
    QCOMPARE(shown.front().coupling, Coupling::OnlyTheSharedModelFolder);
    QCOMPARE(shown.front().members.size(), std::size_t{3});

    const auto alone = std::ranges::find_if(shown.front().members,
                                            [](const MemberOnScreen& member)
                                            {
                                                return member.writesWith == 0;
                                            });

    const auto together = std::ranges::count_if(shown.front().members,
                                                [](const MemberOnScreen& member)
                                                {
                                                    return member.writesWith == 1;
                                                });

    QVERIFY(alone != shown.front().members.end());
    QCOMPARE(alone->name, QString("md11-base"));
    QCOMPARE(static_cast<std::size_t>(together), std::size_t{2});
}

void BisectionViewModelTest::AnAddonJoiningTheLibraryTakesTheScreenToItsOwnStageAndNotToTheDrift()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();
    QCOMPARE(f.viewModel.Stage(), BisectionStage::Asking);

    f.catalog.SetTree(kLibrary, ALibraryTreeWithOneMore());
    f.viewModel.Answer(BisectionAnswer::ItRanFine);

    QCOMPARE(f.viewModel.Stage(), BisectionStage::TheLibraryGainedAnAddon);
    QCOMPARE(f.viewModel.Report().refusal, BisectionRefusal::TheLibraryGainedAnAddon);
    QVERIFY(f.viewModel.ItIsRunning());
}

void BisectionViewModelTest::CarryingOnAppliesTheAnswerThatWasHeldBackInsteadOfLosingIt()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();

    f.catalog.SetTree(kLibrary, ALibraryTreeWithOneMore());
    f.viewModel.Answer(BisectionAnswer::ItRanFine);
    QCOMPARE(f.viewModel.Stage(), BisectionStage::TheLibraryGainedAnAddon);

    f.viewModel.CarryOn();

    QCOMPARE(f.viewModel.Stage(), BisectionStage::Asking);
    QCOMPARE(f.viewModel.Report().refusal, BisectionRefusal::None);
    QCOMPARE(f.viewModel.Report().round, std::size_t{1});
    QCOMPARE(f.viewModel.Report().story.size(), std::size_t{1});
    QCOMPARE(f.viewModel.Report().story.front().answer, BisectionAnswer::ItRanFine);
    QCOMPARE(f.viewModel.Report().units, std::size_t{3});
}

void BisectionViewModelTest::TheHeldAnswerIsForgottenOnceTheScreenLeavesThatStage()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();

    f.catalog.SetTree(kLibrary, ALibraryTreeWithOneMore());
    f.viewModel.Answer(BisectionAnswer::ItCrashed);
    QCOMPARE(f.viewModel.Stage(), BisectionStage::TheLibraryGainedAnAddon);

    f.viewModel.CarryOn();
    QCOMPARE(f.viewModel.Stage(), BisectionStage::Finished);

    f.viewModel.CarryOn();

    QCOMPARE(f.viewModel.Stage(), BisectionStage::Finished);
    QCOMPARE(f.viewModel.Report().story.size(), std::size_t{1});
}

void BisectionViewModelTest::TheLaunchesAlreadyMadeCountTheReferenceRound()
{
    Fixture f;
    f.TurnOn(kCrj);
    f.TurnOn(kFenix);
    f.TurnOn(kPmdg);
    f.Seed();

    f.viewModel.Begin();

    QCOMPARE(f.viewModel.Report().round, std::size_t{0});
    QCOMPARE(f.viewModel.LaunchesAlreadyMade(), std::size_t{1});

    f.viewModel.Answer(BisectionAnswer::ItRanFine);

    QCOMPARE(f.viewModel.Report().round, std::size_t{1});
    QCOMPARE(f.viewModel.LaunchesAlreadyMade(), std::size_t{2});

    f.viewModel.Answer(BisectionAnswer::ItCrashed);

    QCOMPARE(f.viewModel.Stage(), BisectionStage::Finished);
    QCOMPARE(f.viewModel.Report().round, std::size_t{2});
    QCOMPARE(f.viewModel.Report().story.size(), std::size_t{2});
    QCOMPARE(f.viewModel.LaunchesAlreadyMade(), std::size_t{2});
}

QTEST_MAIN(BisectionViewModelTest)

#include "tst_bisection_view_model.moc"
