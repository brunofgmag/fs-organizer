#include <QtTest/QtTest>

#include <algorithm>

#include "domain/tree/AddonTree.h"
#include "tests/support/ArchiveInstallFixture.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/ArchiveRefusalText.h"

namespace
{
    class ArchiveInstallViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnArchiveWithANewAddonOffersItAsNewInEveryLibrary();
        static void AnAddonThatIsAlreadyInstalledReplacesItAndSaysWhichVersionAndWhere();
        static void AnEarlierCopyInTheQuarantineIsOfferedWithItsVersion();
        static void AFolderThatIsNotAnAddonIsInTheWay();
        static void AnArchiveWithNothingToInstallCarriesItsSentence();
        static void AnArchiveThatCouldNotBeReadCarriesItsSentenceWithItsName();
        static void ThreeArchivesAreReadInTheOrderTheyWereGiven();
        static void TheSameAddonCanBeNewInOneLibraryAndReplaceInAnother();
        static void WhileTheSimulatorRunsTheStandingsAreNewAndTheProcessIsNamed();
        static void TheCategoriesOfEachLibraryAreTheOnesTheImportOffers();
        static void TheCategoryTheDropLandedOnIsCarried();
        static void AFolderInOneCategoryMarksOnlyThatCategory();
        static void EveryCategoryOfEveryLibraryIsAskedWithOneScanWhenReadingAndWhenCheckingAgain();
        static void TheOfferKeepsTheCategoriesItAskedAboutEvenIfTheSessionReadsTheLibraryMeanwhile();
        static void TheOccupantTheOfferShowedTravelsToTheInstall();
        static void ADiskThatDisagreesWithTheOfferGetsItsOwnLineInTheReport();
        static void StoppingTheReadingHalfwayEndsItWithoutAnOffer();
        static void ReadingAsksTheServiceOffTheCallingThread();

        static void CheckingAgainGivesTheRealStandingsWithoutListingTheArchivesAgain();
        static void CheckingAgainBeforeReadingAnythingDoesNothing();

        static void EachArchiveWithChosenAddonsIsInstalledOnceWithTheChosenRootsAndTheCategory();
        static void AnArchiveWithNothingChosenIsNotTouched();
        static void TheEarlierQuarantinedCopyIsDiscardedOnlyWhereTheUserAsked();
        static void TheArchivesTheStepsAndTheProgressAreAnnouncedInOrder();
        static void AReplacementAnnouncesTheSameThreeStepsInOrder();
        static void StoppingDuringTheFirstArchiveLeavesTheSecondUninstalled();
        static void WhenEverythingLandsThereIsNoReport();
        static void WhenOnlySomeLandTheReportListsWhatDidNot();
        static void AnArchiveThatStoppedWholeIsOneLineWithoutAnAddonName();
        static void WhenNothingLandsTheReportSaysSo();
        static void AnAddonWhoseLinksCouldNotBeMadeCountsAsInstalledAndKeepsItsLine();
        static void WhenTheOnlyTroubleIsTheLinksTheReportIsKeptAsInstalledButNotLinked();
        static void AFolderThatCouldNotBeRemovedAfterAnInstallIsTheOnlyLineOfItsArchive();
        static void AFolderThatCouldNotBeRemovedAfterAFailureIsToldBesideTheFailure();
        static void AFolderThatCouldNotBeRemovedIsToldBesideTheLinksThatWereLost();
        static void AnExtractionThatTookTooLongSaysSoAndNothingMore();

        static void AfterTheEndTheSessionIsAskedToReadTheLibraryAgain();
        static void TheLandedFoldersAreAnnouncedOnlyOnceTheNewReadingArrives();
        static void AReadingAlreadyRunningWhenTheInstallEndsDoesNotAnnounceBeforeTheNewReading();
        static void AReadingTheUserCancelledStillAnnouncesTheLandedFoldersAlthoughTheSnapshotLacksThem();
        static void NothingIsAnnouncedToEnableWhenNothingLanded();
        static void NothingIsAnnouncedToEnableWhenTheBoxWasNotChecked();
        static void CancellingAfterAnAddonLandedAnnouncesItExactlyOnceOnTheNextReading();
        static void CancellingBeforeAnythingLandedLeavesNoConnectionToEnable();

        static void ASecondReadWhileOneRunsDoesNothing();
        static void AnInstallWhileAReadingRunsDoesNothing();
        static void AReadingWhileAnInstallRunsDoesNothing();

        static void WhatCannotBeInstalledBecomesAReportWithOneLinePerRefusedArchive();

        static void AFolderIsAFolderForTheViewModelAndAnArchiveIsNot();
        static void AFolderIsOfferedLikeAnArchiveAndTheOfferSaysWhichIsWhich();
        static void AFolderThatCouldNotBeReadCarriesTheSentenceOfAFolder();
        static void AJunctionToAFolderThatReachesTheReadingCarriesTheSentenceOfAFolder();
        static void AFolderIsAnnouncedAsCopyingAndAnArchiveAsExtracting();
        static void AFolderThatCouldNotBeCopiedIsToldAsACopy();
        static void AFolderThatTookTooLongToCopyIsToldAsACopy();
        static void AFolderTheCopyOfWhichStayedBehindIsToldAsACopy();
        static void AFolderThatIsInTheWayOfTheInstallIsToldInBothDirections();
    };
}

namespace
{
    std::size_t TimesListed(const FakeArchiveReader& reader)
    {
        return reader.listed.size();
    }

    QString NameOf(const InstallStep step)
    {
        switch (step)
        {
        case InstallStep::Extracting: return QStringLiteral("Extracting");
        case InstallStep::Copying: return QStringLiteral("Copying");
        case InstallStep::Checking: return QStringLiteral("Checking");
        case InstallStep::PuttingIntoPlace: return QStringLiteral("PuttingIntoPlace");
        }

        return {};
    }

    InstallOutcome TheEndOf(const QSignalSpy& finished)
    {
        return finished.back().front().value<InstallOutcome>();
    }

    InstallOffer TheOfferOf(const QSignalSpy& offers)
    {
        return offers.back().front().value<InstallOffer>();
    }
}

void ArchiveInstallViewModelTest::AnArchiveWithANewAddonOffersItAsNewInEveryLibrary()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.2.3"}});

    const InstallOffer offer = f.Reads({kFirst});

    QCOMPARE(offer.archives.size(), std::size_t{1});

    const InstallArchiveLine& archive = offer.archives.front();
    QCOMPARE(archive.fileName, QStringLiteral("first.zip"));
    QCOMPARE(archive.path, AsText(kFirst));
    QVERIFY(archive.refusal.isEmpty());
    QCOMPARE(archive.addons.size(), std::size_t{1});

    const InstallAddonLine& addon = archive.addons.front();
    QCOMPARE(addon.folderName, QStringLiteral("alpha"));
    QCOMPARE(addon.version, QStringLiteral("1.2.3"));
    QCOMPARE(addon.bytes, qulonglong{kManifestBytes + kSceneryBytes});
    QCOMPARE(addon.root, QStringLiteral("alpha"));
    QCOMPARE(addon.inEachLibrary.size(), std::size_t{1});
    QCOMPARE(addon.inEachLibrary.front().standing, InstallStanding::New);
    QVERIFY(offer.runningSimulator.isEmpty());
    QVERIFY(offer.droppedOn.isEmpty());
    QCOMPARE(offer.libraries.size(), std::size_t{1});
    QCOMPARE(offer.libraries.front().path, AsText(kLibrary));
    QVERIFY(offer.libraries.front().shown.startsWith(QStringLiteral("MSFS 2024")));
    QVERIFY(offer.libraries.front().shown.endsWith(AsText(kLibrary)));
}

void ArchiveInstallViewModelTest::AnAddonThatIsAlreadyInstalledReplacesItAndSaysWhichVersionAndWhere()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0");
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}, {"fresh", "1.0.0"}});

    const InstallOffer offer = f.Reads({kFirst});

    const InstallAddonLine& replaces = offer.archives.front().addons[0];
    QCOMPARE(replaces.version, QStringLiteral("2.0.0"));
    QCOMPARE(replaces.inEachLibrary.front().standing, InstallStanding::Replaces);
    QCOMPARE(replaces.inEachLibrary.front().installedVersion, QStringLiteral("1.0.0"));
    QCOMPARE(replaces.inEachLibrary.front().installedIn, AsText(std::filesystem::path{"Utils"}));
    QVERIFY(replaces.inEachLibrary.front().quarantinedVersion.isEmpty());
    QCOMPARE(offer.archives.front().addons[1].inEachLibrary.front().standing, InstallStanding::New);
}

void ArchiveInstallViewModelTest::AnEarlierCopyInTheQuarantineIsOfferedWithItsVersion()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0");
    f.TheQuarantineHoldsASimbridge("0.9.0");
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});

    const InstallOffer offer = f.Reads({kFirst});

    const InstallStandingInALibrary& standing = offer.archives.front().addons.front().inEachLibrary.front();
    QCOMPARE(standing.standing, InstallStanding::ReplacesAndTheQuarantineHoldsOne);
    QCOMPARE(standing.installedVersion, QStringLiteral("1.0.0"));
    QCOMPARE(standing.quarantinedVersion, QStringLiteral("0.9.0"));
}

void ArchiveInstallViewModelTest::AFolderThatIsNotAnAddonIsInTheWay()
{
    Fixture f;
    f.fileSystem.AddFile(kLibrary / "blocked" / "readme.txt", kMegabyte);
    f.ArchiveHolds(kFirst, {{"blocked", "1.0.0"}, {"fresh", "1.0.0"}});

    const InstallOffer offer = f.Reads({kFirst});

    QCOMPARE(offer.archives.front().addons[0].inEachLibrary.front().standing, InstallStanding::AFolderIsInTheWay);
    QCOMPARE(offer.archives.front().addons[1].inEachLibrary.front().standing, InstallStanding::New);
}

void ArchiveInstallViewModelTest::AnArchiveWithNothingToInstallCarriesItsSentence()
{
    Fixture f;
    f.archives.At(kFirst).Lists({File("readme.txt", 1)});
    f.ArchiveHolds(kSecond, {{"alpha", "1.0.0"}});

    const InstallOffer offer = f.Reads({kFirst, kSecond});

    QCOMPARE(offer.archives.size(), std::size_t{2});
    QCOMPARE(offer.archives[0].refusal, WhyTheArchiveIsRefused(NothingToInstall::NoPackage, SourceKind::Archive));
    QVERIFY(!offer.archives[0].refusal.isEmpty());
    QVERIFY(offer.archives[0].addons.empty());
    QVERIFY(offer.archives[1].refusal.isEmpty());
    QCOMPARE(offer.archives[1].addons.size(), std::size_t{1});
}

void ArchiveInstallViewModelTest::AnArchiveThatCouldNotBeReadCarriesItsSentenceWithItsName()
{
    Fixture f;
    f.ArchiveHolds(kSecond, {{"alpha", "1.0.0"}});
    f.archives.At(kSecond).FailsToList(ArchiveFailure::CouldNotOpen);

    const InstallOffer offer = f.Reads({kSecond});

    QCOMPARE(offer.archives.front().refusal,
             WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("second.rar"), SourceKind::Archive));
    QVERIFY(offer.archives.front().refusal.contains(QStringLiteral("Windows 10")));
    QVERIFY(offer.archives.front().addons.empty());
}

void ArchiveInstallViewModelTest::ThreeArchivesAreReadInTheOrderTheyWereGiven()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    f.ArchiveHolds(kThird, {{"gamma", "1.0.0"}});
    const QSignalSpy reading(&f.viewModel, &ArchiveInstallViewModel::ReadingArchive);

    const InstallOffer offer = f.Reads({kThird, kFirst, kSecond});

    QCOMPARE(reading.size(), 3);
    for (int index = 0; index < 3; ++index)
    {
        QCOMPARE(reading.at(index).at(0).toInt(), index + 1);
        QCOMPARE(reading.at(index).at(1).toInt(), 3);
    }
    QCOMPARE(reading.at(0).at(2).toString(), QStringLiteral("third.7z"));
    QCOMPARE(reading.at(1).at(2).toString(), QStringLiteral("first.zip"));
    QCOMPARE(reading.at(2).at(2).toString(), QStringLiteral("second.rar"));
    QCOMPARE(offer.archives.size(), std::size_t{3});
    QCOMPARE(offer.archives[0].fileName, QStringLiteral("third.7z"));
    QCOMPARE(offer.archives[1].fileName, QStringLiteral("first.zip"));
    QCOMPARE(offer.archives[2].fileName, QStringLiteral("second.rar"));
}

void ArchiveInstallViewModelTest::TheSameAddonCanBeNewInOneLibraryAndReplaceInAnother()
{
    Fixture f(TwoLibraries());
    f.ASimbridgeIsInstalled("1.0.0");
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});

    const InstallOffer offer = f.Reads({kFirst});

    QCOMPARE(offer.libraries.size(), std::size_t{2});
    QCOMPARE(offer.libraries[0].path, AsText(kLibrary));
    QCOMPARE(offer.libraries[1].path, AsText(kSpare));
    QCOMPARE(offer.libraries[1].shown, AsText(kSpare));

    const std::vector<InstallStandingInALibrary>& standings = offer.archives.front().addons.front().inEachLibrary;
    QCOMPARE(standings.size(), std::size_t{2});
    QCOMPARE(standings[0].standing, InstallStanding::Replaces);
    QCOMPARE(standings[1].standing, InstallStanding::New);
}

void ArchiveInstallViewModelTest::WhileTheSimulatorRunsTheStandingsAreNewAndTheProcessIsNamed()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0");
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    f.processProbe.ReportTheSimulatorAsRunning();

    const InstallOffer offer = f.Reads({kFirst});

    QCOMPARE(offer.runningSimulator, QStringLiteral("FlightSimulator2024.exe"));
    QCOMPARE(offer.archives.front().addons.front().inEachLibrary.front().standing, InstallStanding::New);
    QCOMPARE(offer.archives.front().addons.front().version, QStringLiteral("2.0.0"));
}

void ArchiveInstallViewModelTest::TheCategoriesOfEachLibraryAreTheOnesTheImportOffers()
{
    Fixture f;
    f.fileSystem.AddDirectory(kSceneries);
    f.TheLibraryHoldsNodes({},
                           {TreeNode{.kind = TreeNodeKind::Category, .path = kUtils, .declaredAsCategory = true},
                            TreeNode{.kind = TreeNodeKind::Category, .path = kSceneries, .declaredAsCategory = true}});
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});

    const InstallOffer offer = f.Reads({kFirst});

    const std::vector<InstallCategoryChoice>& categories = offer.libraries.front().categories;
    QVERIFY(categories.size() >= std::size_t{3});
    QCOMPARE(categories.front().path, AsText(kLibrary));
    QCOMPARE(categories.front().shown, QObject::tr("(library root)"));

    const auto utils = std::ranges::find(categories, AsText(kUtils), &InstallCategoryChoice::path);
    const auto sceneries = std::ranges::find(categories, AsText(kSceneries), &InstallCategoryChoice::path);
    QVERIFY(utils != categories.end());
    QVERIFY(sceneries != categories.end());
    QCOMPARE(utils->shown, AsText(std::filesystem::path{"Utils"}));
    QCOMPARE(sceneries->shown, AsText(std::filesystem::path{"Sceneries"}));
}

void ArchiveInstallViewModelTest::TheCategoryTheDropLandedOnIsCarried()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});

    QCOMPARE(f.Reads({kFirst}, kUtils).droppedOn, AsText(kUtils));
    QVERIFY(f.Reads({kFirst}).droppedOn.isEmpty());
}

void ArchiveInstallViewModelTest::AFolderInOneCategoryMarksOnlyThatCategory()
{
    Fixture f;
    f.fileSystem.AddDirectory(kSceneries);
    f.TheLibraryHoldsNodes({}, {Fixture::ADeclaredCategory(kUtils), Fixture::ADeclaredCategory(kSceneries)});
    f.fileSystem.AddFile(kUtils / "blocked" / "readme.txt", kMegabyte);
    f.fileSystem.AddFile(kLibrary / "atroot" / "readme.txt", kMegabyte);
    f.ArchiveHolds(kFirst, {{"blocked", "1.0.0"}, {"atroot", "1.0.0"}, {"fresh", "1.0.0"}});

    const InstallOffer offer = f.Reads({kFirst});

    const std::vector<InstallCategoryChoice>& categories = offer.libraries.front().categories;
    const InstallAddonLine& blocked = offer.archives.front().addons[0];
    const InstallAddonLine& atRoot = offer.archives.front().addons[1];
    const InstallAddonLine& fresh = offer.archives.front().addons[2];

    QCOMPARE(blocked.inEachCategory.size(), std::size_t{1});
    QCOMPARE(blocked.inEachCategory.front().size(), categories.size());
    QCOMPARE(fresh.inEachCategory.front().size(), categories.size());

    for (std::size_t at = 0; at < categories.size(); ++at)
    {
        const bool isUtils = categories[at].path == AsText(kUtils);
        const bool isTheRoot = categories[at].path == AsText(kLibrary);

        QCOMPARE(blocked.inEachCategory.front()[at].standing,
                 isUtils ? InstallStanding::AFolderIsInTheWay : InstallStanding::New);
        QCOMPARE(atRoot.inEachCategory.front()[at].standing,
                 isTheRoot ? InstallStanding::AFolderIsInTheWay : InstallStanding::New);
        QCOMPARE(fresh.inEachCategory.front()[at].standing, InstallStanding::New);
    }

    QCOMPARE(blocked.inEachLibrary.front().standing, InstallStanding::New);
    QCOMPARE(atRoot.inEachLibrary.front().standing, InstallStanding::New);
    QCOMPARE(fresh.inEachLibrary.front().standing, InstallStanding::New);
}

void ArchiveInstallViewModelTest::EveryCategoryOfEveryLibraryIsAskedWithOneScanWhenReadingAndWhenCheckingAgain()
{
    Fixture f(TwoLibraries());
    f.fileSystem.AddDirectory(kSceneries);
    f.TheLibraryHoldsNodes({}, {Fixture::ADeclaredCategory(kUtils), Fixture::ADeclaredCategory(kSceneries)});
    f.catalog.SetTree(kSpare, TreeNode{.kind = TreeNodeKind::Library, .path = kSpare});
    f.session.ShowActiveProfile();
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const std::size_t before = f.catalog.scanned;

    const InstallOffer offer = f.Reads({kFirst});

    std::size_t categories = 0;
    for (const InstallLibraryChoice& library : offer.libraries)
    {
        categories += library.categories.size();
    }

    QVERIFY2(categories > f.session.Profile().libraries.size(), "there are more categories than libraries");
    QCOMPARE(f.catalog.scanned - before, f.session.Profile().libraries.size());

    const std::size_t beforeChecking = f.catalog.scanned;

    f.viewModel.CheckAgain();

    QCOMPARE(f.catalog.scanned - beforeChecking, f.session.Profile().libraries.size());
}

void ArchiveInstallViewModelTest::TheOfferKeepsTheCategoriesItAskedAboutEvenIfTheSessionReadsTheLibraryMeanwhile()
{
    Fixture f;
    f.fileSystem.AddDirectory(kSceneries);
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);
    f.runner.defer = true;

    f.viewModel.Read({kFirst}, {});
    f.TheLibraryHoldsNodes({}, {Fixture::ADeclaredCategory(kSceneries)});
    f.runner.RunPendingWork();
    f.runner.FinishNewestDone();
    f.runner.Finish();

    QCOMPARE(offers.size(), 1);

    const InstallOffer offer = TheOfferOf(offers);
    const std::vector<InstallCategoryChoice>& categories = offer.libraries.front().categories;

    QVERIFY(std::ranges::none_of(categories,
                                 [](const InstallCategoryChoice& category)
                                 {
                                     return category.path == AsText(kSceneries);
                                 }));
    QCOMPARE(offer.archives.front().addons.front().inEachCategory.front().size(), categories.size());
}

void ArchiveInstallViewModelTest::TheOccupantTheOfferShowedTravelsToTheInstall()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}, {"fresh", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    const InstallAddonLine& replaces = offer.archives.front().addons[0];
    QCOMPARE(replaces.inEachLibrary.front().occupant, AsText(kInLibrary));
    QCOMPARE(replaces.inEachCategory.front().front().occupant, AsText(kInLibrary));
    QVERIFY(offer.archives.front().addons[1].inEachLibrary.front().occupant.isEmpty());

    const ChosenAddon chosen = Fixture::Chooses(offer, 0, 0);
    QCOMPARE(chosen.replaces, AsText(kInLibrary));
    QVERIFY(Fixture::Chooses(offer, 0, 1).replaces.isEmpty());

    f.viewModel.Install({chosen, Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), false);

    QCOMPARE(finished.size(), 1);
    QVERIFY(!TheEndOf(finished).report.has_value());
    QCOMPARE(TheEndOf(finished).installed, 2);
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary));
}

void ArchiveInstallViewModelTest::ADiskThatDisagreesWithTheOfferGetsItsOwnLineInTheReport()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    ChosenAddon chosen = Fixture::Chooses(offer, 0, 0);
    chosen.replaces.clear();

    f.viewModel.Install({chosen}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::NothingWasInstalled);
    QCOMPARE(outcome.installed, 0);
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().lines.front().addon, QStringLiteral("simbridge"));
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::TheDiskDisagreesWithTheScan, SourceKind::Archive));
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);
    QVERIFY(!f.fileSystem.Exists(kHeldInLibrary));
}

void ArchiveInstallViewModelTest::StoppingTheReadingHalfwayEndsItWithoutAnOffer()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    f.ArchiveHolds(kThird, {{"gamma", "1.0.0"}});
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);
    const QSignalSpy stopped(&f.viewModel, &ArchiveInstallViewModel::ReadingStopped);

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::ReadingArchive, &f.viewModel,
                     [&f](const int archive)
                     {
                         if (archive == 2)
                         {
                             f.viewModel.Cancel();
                         }
                     });

    f.viewModel.Read({kFirst, kSecond, kThird}, {});

    QCOMPARE(offers.size(), 0);
    QCOMPARE(stopped.size(), 1);
    QCOMPARE(f.archives.At(kThird).listed.size(), std::size_t{0});
    QVERIFY(!f.viewModel.Busy());
}

void ArchiveInstallViewModelTest::ReadingAsksTheServiceOffTheCallingThread()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);
    f.runner.defer = true;

    f.viewModel.Read({kFirst}, {});

    QCOMPARE(f.runner.HowManyPending(), std::size_t{1});
    QCOMPARE(TimesListed(f.archives.At(kFirst)), std::size_t{0});
    QCOMPARE(offers.size(), 0);

    f.runner.Finish();

    QCOMPARE(TimesListed(f.archives.At(kFirst)), std::size_t{1});
    QCOMPARE(offers.size(), 1);
}

void ArchiveInstallViewModelTest::CheckingAgainGivesTheRealStandingsWithoutListingTheArchivesAgain()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0");
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    f.processProbe.ReportTheSimulatorAsRunning();
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);

    f.viewModel.Read({kFirst}, kUtils);

    QCOMPARE(offers.size(), 1);
    QCOMPARE(TheOfferOf(offers).archives.front().addons.front().inEachLibrary.front().standing, InstallStanding::New);

    const std::size_t listed = TimesListed(f.archives.At(kFirst));
    const std::size_t read = f.archives.At(kFirst).read.size();
    f.processProbe.ReportTheSimulatorAsClosed();

    f.viewModel.CheckAgain();

    QCOMPARE(offers.size(), 2);

    const InstallOffer again = TheOfferOf(offers);
    QVERIFY(again.runningSimulator.isEmpty());
    QCOMPARE(again.archives.front().addons.front().inEachLibrary.front().standing, InstallStanding::Replaces);
    QCOMPARE(again.archives.front().addons.front().inEachLibrary.front().installedVersion, QStringLiteral("1.0.0"));
    QCOMPARE(again.droppedOn, AsText(kUtils));
    QCOMPARE(TimesListed(f.archives.At(kFirst)), listed);
    QCOMPARE(f.archives.At(kFirst).read.size(), read);
}

void ArchiveInstallViewModelTest::CheckingAgainBeforeReadingAnythingDoesNothing()
{
    Fixture f;
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);
    const int runs = f.runner.runs;

    f.viewModel.CheckAgain();

    QCOMPARE(offers.size(), 0);
    QCOMPARE(f.runner.runs, runs);
}

void ArchiveInstallViewModelTest::EachArchiveWithChosenAddonsIsInstalledOnceWithTheChosenRootsAndTheCategory()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"gamma", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst, kSecond});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    const QSignalSpy started(&f.viewModel, &ArchiveInstallViewModel::InstallingArchive);

    f.viewModel.Install({Fixture::Chooses(offer, 1, 0), Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), false);

    QCOMPARE(finished.size(), 1);
    QCOMPARE(started.size(), 2);
    QCOMPARE(started.at(0).at(2).toString(), QStringLiteral("first.zip"));
    QCOMPARE(started.at(1).at(2).toString(), QStringLiteral("second.rar"));
    QCOMPARE(f.archives.At(kFirst).extractions.size(), std::size_t{1});
    QCOMPARE(f.archives.At(kSecond).extractions.size(), std::size_t{1});
    QCOMPARE(f.archives.At(kFirst).extractions.front().archive, kFirst);
    QVERIFY(f.fileSystem.Exists(kUtils / "beta" / "manifest.json"));
    QVERIFY(f.fileSystem.Exists(kUtils / "gamma" / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    QVERIFY(!f.fileSystem.Exists(kLibrary / "beta"));

    const InstallOutcome outcome = TheEndOf(finished);
    QCOMPARE(outcome.installed, 2);
    QCOMPARE(outcome.landed.size(), std::size_t{2});
    QVERIFY(std::ranges::find(outcome.landed, kUtils / "beta") != outcome.landed.end());
    QVERIFY(std::ranges::find(outcome.landed, kUtils / "gamma") != outcome.landed.end());
}

void ArchiveInstallViewModelTest::AnArchiveWithNothingChosenIsNotTouched()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst, kSecond});
    const QSignalSpy started(&f.viewModel, &ArchiveInstallViewModel::InstallingArchive);

    f.viewModel.Install({Fixture::Chooses(offer, 1, 0)}, AsText(kUtils), false);

    QCOMPARE(started.size(), 1);
    QCOMPARE(started.front().at(0).toInt(), 1);
    QCOMPARE(started.front().at(1).toInt(), 1);
    QCOMPARE(started.front().at(2).toString(), QStringLiteral("second.rar"));
    QCOMPARE(TimesListed(f.archives.At(kFirst)), std::size_t{1});
    QCOMPARE(f.archives.At(kFirst).extractions.size(), std::size_t{0});
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
}

void ArchiveInstallViewModelTest::TheEarlierQuarantinedCopyIsDiscardedOnlyWhereTheUserAsked()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.TheQuarantineHoldsASimbridge("0.9.0");
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0, false)}, AsText(kUtils), false);

    QCOMPARE(finished.size(), 1);
    QVERIFY(TheEndOf(finished).report.has_value());
    QCOMPARE(TheEndOf(finished).installed, 0);
    QCOMPARE(f.archives.At(kFirst).extractions.size(), std::size_t{0});
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kMegabyte);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0, true)}, AsText(kUtils), false);

    QCOMPARE(finished.size(), 2);
    QCOMPARE(TheEndOf(finished).installed, 1);
    QCOMPARE(f.archives.At(kFirst).extractions.size(), std::size_t{1});
    QCOMPARE(f.fileSystem.FileSize(kInLibrary / "manifest.json"), kManifestBytes);
}

void ArchiveInstallViewModelTest::TheArchivesTheStepsAndTheProgressAreAnnouncedInOrder()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}, {"gamma", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst, kSecond});
    QStringList events;

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::InstallingArchive, &f.viewModel,
                     [&events](const int archive, const int of, const QString& name)
                     {
                         events << QStringLiteral("archive %1 of %2 %3").arg(archive).arg(of).arg(name);
                     });
    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::StepChanged, &f.viewModel,
                     [&events](const InstallStep step)
                     {
                         events << NameOf(step);
                     });
    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::InstallingAddon, &f.viewModel,
                     [&events](const int addon, const int of, const QString& name)
                     {
                         events << QStringLiteral("addon %1 of %2 %3").arg(addon).arg(of).arg(name);
                     });
    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::Progressed, &f.viewModel,
                     [&events](const qulonglong copied, const qulonglong total)
                     {
                         Q_UNUSED(copied)
                         Q_UNUSED(total)

                         if (events.isEmpty() || events.back() != QStringLiteral("progress"))
                         {
                             events << QStringLiteral("progress");
                         }
                     });

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 1, 0), Fixture::Chooses(offer, 1, 1)},
                        AsText(kUtils), false);

    QCOMPARE(events,
             (QStringList{QStringLiteral("archive 1 of 2 first.zip"), QStringLiteral("Extracting"),
                          QStringLiteral("progress"), QStringLiteral("Checking"), QStringLiteral("PuttingIntoPlace"),
                          QStringLiteral("addon 1 of 1 alpha"), QStringLiteral("archive 2 of 2 second.rar"),
                          QStringLiteral("Extracting"), QStringLiteral("progress"), QStringLiteral("Checking"),
                          QStringLiteral("PuttingIntoPlace"), QStringLiteral("addon 1 of 2 beta"),
                          QStringLiteral("addon 2 of 2 gamma")}));
}

void ArchiveInstallViewModelTest::AReplacementAnnouncesTheSameThreeStepsInOrder()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    std::vector<InstallStep> steps;

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::StepChanged, &f.viewModel,
                     [&steps](const InstallStep step)
                     {
                         steps.push_back(step);
                     });

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    QCOMPARE(steps,
             (std::vector<InstallStep>{InstallStep::Extracting, InstallStep::Checking, InstallStep::PuttingIntoPlace}));
    QVERIFY(f.fileSystem.Exists(kHeldInLibrary));
}

void ArchiveInstallViewModelTest::StoppingDuringTheFirstArchiveLeavesTheSecondUninstalled()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst, kSecond});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    const QSignalSpy cancelling(&f.viewModel, &ArchiveInstallViewModel::Cancelling);

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::Progressed, &f.viewModel,
                     [&f]
                     {
                         f.viewModel.Cancel();
                     });

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 1, 0)}, AsText(kUtils), false);

    QCOMPARE(cancelling.size(), 1);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(f.archives.At(kSecond).extractions.size(), std::size_t{0});
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::NothingWasInstalled);
    QCOMPARE(outcome.report->installed, 0);
    QCOMPARE(outcome.report->notInstalled, 2);
    QCOMPARE(outcome.report->archives.size(), std::size_t{2});
    QCOMPARE(outcome.report->archives[1].fileName, QStringLiteral("second.rar"));
    QCOMPARE(outcome.report->archives[1].lines.size(), std::size_t{1});
    QVERIFY(outcome.report->archives[1].lines.front().addon.isEmpty());
    QCOMPARE(outcome.report->archives[1].lines.front().reason,
             WhyItWasNotInstalled(FileResult::Cancelled, SourceKind::Archive));
}

void ArchiveInstallViewModelTest::WhenEverythingLandsThereIsNoReport()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), false);

    QCOMPARE(finished.size(), 1);
    QVERIFY(!TheEndOf(finished).report.has_value());
    QCOMPARE(TheEndOf(finished).installed, 2);
}

void ArchiveInstallViewModelTest::WhenOnlySomeLandTheReportListsWhatDidNot()
{
    Fixture f;
    f.fileSystem.AddFile(kUtils / "blocked" / "readme.txt", kMegabyte);
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"blocked", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst, kSecond});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 0, 1), Fixture::Chooses(offer, 1, 0)},
                        AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::SomeWereNotInstalled);
    QCOMPARE(outcome.report->installed, 2);
    QCOMPARE(outcome.report->notInstalled, 1);
    QCOMPARE(outcome.installed, 2);
    QCOMPARE(outcome.report->archives.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().fileName, QStringLiteral("first.zip"));
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().lines.front().addon, QStringLiteral("blocked"));
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::TheOriginIsOccupied, SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AnArchiveThatStoppedWholeIsOneLineWithoutAnAddonName()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}, {"gamma", "1.0.0"}});
    f.archives.At(kSecond).FailsToExtract(ArchiveFailure::CouldNotExtract);
    const InstallOffer offer = f.Reads({kFirst, kSecond});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 1, 0), Fixture::Chooses(offer, 1, 1)},
                        AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::SomeWereNotInstalled);
    QCOMPARE(outcome.report->installed, 1);
    QCOMPARE(outcome.report->notInstalled, 2);
    QCOMPARE(outcome.report->archives.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().fileName, QStringLiteral("second.rar"));
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QVERIFY(outcome.report->archives.front().lines.front().addon.isEmpty());
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::CouldNotExtract, SourceKind::Archive));
}

void ArchiveInstallViewModelTest::WhenNothingLandsTheReportSaysSo()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.processProbe.ReportTheSimulatorAsRunning();

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), true);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::NothingWasInstalled);
    QCOMPARE(outcome.report->installed, 0);
    QCOMPARE(outcome.report->notInstalled, 2);
    QCOMPARE(outcome.installed, 0);
    QVERIFY(outcome.landed.empty());
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::TheSimulatorIsRunning, SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AnAddonWhoseLinksCouldNotBeMadeCountsAsInstalledAndKeepsItsLine()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}, {"blocked", "1.0.0"}});
    f.fileSystem.AddFile(kUtils / "blocked" / "readme.txt", kMegabyte);
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.linkService.MakeLinkCreationFail();

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.installed, 1);
    QCOMPARE(outcome.landed.size(), std::size_t{1});
    QCOMPARE(outcome.landed.front(), kInLibrary);
    QCOMPARE(outcome.report->installed, 1);
    QCOMPARE(outcome.report->notInstalled, 1);

    const std::vector<InstallReportLine>& lines = outcome.report->archives.front().lines;
    QCOMPARE(lines.size(), std::size_t{2});
    QCOMPARE(lines[0].addon, QStringLiteral("simbridge"));
    QCOMPARE(lines[0].reason, WhyItWasNotInstalled(FileResult::CouldNotCreateLink, SourceKind::Archive));
    QCOMPARE(lines[1].addon, QStringLiteral("blocked"));
}

void ArchiveInstallViewModelTest::WhenTheOnlyTroubleIsTheLinksTheReportIsKeptAsInstalledButNotLinked()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.linkService.MakeLinkCreationFail();

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QCOMPARE(outcome.installed, 1);
    QCOMPARE(outcome.landed.size(), std::size_t{1});
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::InstalledButNotLinked);
    QCOMPARE(outcome.report->installed, 1);
    QCOMPARE(outcome.report->notInstalled, 0);
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::CouldNotCreateLink, SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderThatCouldNotBeRemovedAfterAnInstallIsTheOnlyLineOfItsArchive()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.files.MakeTheRemovalFailFor(InstallStagingIn(kUtils));

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QCOMPARE(outcome.installed, 1);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::InstalledButAFolderStayed);
    QCOMPARE(outcome.report->installed, 1);
    QCOMPARE(outcome.report->notInstalled, 0);
    QCOMPARE(outcome.report->archives.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().fileName, QStringLiteral("first.zip"));
    QVERIFY(outcome.report->archives.front().lines.empty());
    QCOMPARE(outcome.report->archives.front().leftBehind, TheExtractionFolderStayed(SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderThatCouldNotBeRemovedAfterAFailureIsToldBesideTheFailure()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.archives.At(kFirst).FailsToExtract(ArchiveFailure::CouldNotExtract);
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.files.MakeTheRemovalFailFor(InstallStagingIn(kUtils));

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::NothingWasInstalled);
    QCOMPARE(outcome.report->notInstalled, 1);
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::CouldNotExtract, SourceKind::Archive));
    QCOMPARE(outcome.report->archives.front().leftBehind, TheExtractionFolderStayed(SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderThatCouldNotBeRemovedIsToldBesideTheLinksThatWereLost()
{
    Fixture f;
    f.ASimbridgeIsInstalled("1.0.0", true);
    f.ArchiveHolds(kFirst, {{"simbridge", "2.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.linkService.MakeLinkCreationFail();
    f.files.MakeTheRemovalFailFor(InstallStagingIn(kUtils));

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::InstalledButNotLinked);
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QCOMPARE(outcome.report->archives.front().leftBehind, TheExtractionFolderStayed(SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AnExtractionThatTookTooLongSaysSoAndNothingMore()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.archives.At(kFirst).FailsAfterWritingEverything(ArchiveFailure::TookTooLong);
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::NothingWasInstalled);
    QCOMPARE(outcome.report->archives.front().lines.size(), std::size_t{1});
    QVERIFY(outcome.report->archives.front().lines.front().addon.isEmpty());
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyTheArchiveIsRefused(ArchiveFailure::TookTooLong, QStringLiteral("first.zip"), SourceKind::Archive));
    QVERIFY(outcome.report->archives.front().leftBehind.isEmpty());
}

void ArchiveInstallViewModelTest::AfterTheEndTheSessionIsAskedToReadTheLibraryAgain()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy scans(&f.notifier, &SessionNotifier::ScanStarted);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    QCOMPARE(scans.size(), 1);
}

void ArchiveInstallViewModelTest::TheLandedFoldersAreAnnouncedOnlyOnceTheNewReadingArrives()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    f.runner.defer = true;

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), true);
    f.runner.Finish();

    QCOMPARE(finished.size(), 1);
    QCOMPARE(landed.size(), 0);
    QVERIFY(f.runner.Pending());

    f.runner.Finish();

    QCOMPARE(landed.size(), 1);

    const auto folders = landed.front().front().value<std::vector<std::filesystem::path>>();
    QCOMPARE(folders.size(), std::size_t{2});
    QVERIFY(std::ranges::find(folders, kUtils / "alpha") != folders.end());
    QVERIFY(std::ranges::find(folders, kUtils / "beta") != folders.end());
}

void ArchiveInstallViewModelTest::AReadingAlreadyRunningWhenTheInstallEndsDoesNotAnnounceBeforeTheNewReading()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    f.runner.defer = true;

    f.session.ShowActiveProfile();
    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), true);

    QCOMPARE(f.runner.HowManyPending(), std::size_t{2});

    f.runner.FinishNewestDone();

    QCOMPARE(finished.size(), 1);
    QCOMPARE(landed.size(), 0);

    f.runner.Finish();

    QCOMPARE(landed.size(), 0);
    QVERIFY2(f.runner.Pending(), "the reading that was running makes room for the one the install asked for");

    f.runner.Finish();

    QCOMPARE(landed.size(), 1);
}

void ArchiveInstallViewModelTest::AReadingTheUserCancelledStillAnnouncesTheLandedFoldersAlthoughTheSnapshotLacksThem()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    f.runner.defer = true;

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), true);
    f.runner.Finish();

    TreeNode library{.kind = TreeNodeKind::Library, .path = kLibrary};
    library.children = {AddonNodeDeclaring(kUtils / "alpha", "1.0.0")};
    f.catalog.SetTree(kLibrary, std::move(library));
    f.session.CancelScan();
    f.runner.Finish();

    QCOMPARE(landed.size(), 1);
    QVERIFY2(AddonNamed(f.session.Snapshot().libraries, "alpha") == nullptr,
             "a reading the user cancelled is not adopted");
}

void ArchiveInstallViewModelTest::NothingIsAnnouncedToEnableWhenNothingLanded()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    f.processProbe.ReportTheSimulatorAsRunning();

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), true);
    f.session.ShowActiveProfile();

    QCOMPARE(landed.size(), 0);
}

void ArchiveInstallViewModelTest::NothingIsAnnouncedToEnableWhenTheBoxWasNotChecked()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);
    f.session.ShowActiveProfile();

    QCOMPARE(landed.size(), 0);
}

void ArchiveInstallViewModelTest::CancellingAfterAnAddonLandedAnnouncesItExactlyOnceOnTheNextReading()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    f.runner.defer = true;

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::InstallingAddon, &f.viewModel,
                     [&f](const int addon)
                     {
                         if (addon == 1)
                         {
                             f.viewModel.Cancel();
                         }
                     });

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 0, 1)}, AsText(kUtils), true);
    f.runner.Finish();

    QCOMPARE(finished.size(), 1);
    QCOMPARE(TheEndOf(finished).landed, (std::vector<std::filesystem::path>{kUtils / "alpha"}));
    QVERIFY(f.fileSystem.Exists(kUtils / "alpha" / "manifest.json"));
    QVERIFY(!f.fileSystem.Exists(kUtils / "beta"));
    QCOMPARE(landed.size(), 0);
    QVERIFY(f.runner.Pending());

    f.runner.Finish();

    QCOMPARE(landed.size(), 1);
    QCOMPARE(landed.front().front().value<std::vector<std::filesystem::path>>(),
             (std::vector<std::filesystem::path>{kUtils / "alpha"}));

    f.session.ShowActiveProfile();
    f.runner.Finish();
    f.session.ShowActiveProfile();
    f.runner.Finish();

    QCOMPARE(landed.size(), 1);
}

void ArchiveInstallViewModelTest::CancellingBeforeAnythingLandedLeavesNoConnectionToEnable()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    const QSignalSpy landed(&f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    f.runner.defer = true;

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::Progressed, &f.viewModel,
                     [&f]
                     {
                         f.viewModel.Cancel();
                     });

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), true);
    f.runner.Finish();

    QCOMPARE(finished.size(), 1);
    QVERIFY(TheEndOf(finished).landed.empty());
    QVERIFY(f.runner.Pending());

    f.runner.Finish();
    f.session.ShowActiveProfile();
    f.runner.Finish();

    QCOMPARE(landed.size(), 0);
}

void ArchiveInstallViewModelTest::ASecondReadWhileOneRunsDoesNothing()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);
    f.runner.defer = true;

    f.viewModel.Read({kFirst}, {});

    QVERIFY(f.viewModel.Busy());

    f.viewModel.Read({kSecond}, {});
    f.viewModel.CheckAgain();

    QCOMPARE(f.runner.HowManyPending(), std::size_t{1});

    f.runner.Finish();

    QCOMPARE(offers.size(), 1);
    QCOMPARE(TheOfferOf(offers).archives.front().fileName, QStringLiteral("first.zip"));
    QCOMPARE(TimesListed(f.archives.At(kSecond)), std::size_t{0});
    QVERIFY(!f.viewModel.Busy());
}

void ArchiveInstallViewModelTest::AnInstallWhileAReadingRunsDoesNothing()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.runner.defer = true;

    f.viewModel.Read({kFirst}, {});
    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    QCOMPARE(f.runner.HowManyPending(), std::size_t{1});

    f.runner.Finish();

    QCOMPARE(finished.size(), 0);
    QCOMPARE(f.archives.At(kFirst).extractions.size(), std::size_t{0});
}

void ArchiveInstallViewModelTest::AReadingWhileAnInstallRunsDoesNothing()
{
    Fixture f;
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    f.ArchiveHolds(kSecond, {{"beta", "1.0.0"}});
    const InstallOffer offer = f.Reads({kFirst});
    const QSignalSpy offers(&f.viewModel, &ArchiveInstallViewModel::OfferReady);
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.runner.defer = true;

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    QVERIFY(f.viewModel.Busy());

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);
    f.viewModel.Read({kSecond}, {});
    f.viewModel.CheckAgain();

    QCOMPARE(f.runner.HowManyPending(), std::size_t{1});

    f.runner.Finish();

    QCOMPARE(offers.size(), 0);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(TimesListed(f.archives.At(kSecond)), std::size_t{0});
    QCOMPARE(f.archives.At(kFirst).extractions.size(), std::size_t{1});
}

void ArchiveInstallViewModelTest::WhatCannotBeInstalledBecomesAReportWithOneLinePerRefusedArchive()
{
    Fixture f;
    f.archives.At(kFirst).Lists({File("readme.txt", 1)});
    f.ArchiveHolds(kSecond, {{"alpha", "1.0.0"}});
    f.archives.At(kThird).FailsToList(ArchiveFailure::NeedsAPassword);
    const InstallOffer offer = f.Reads({kFirst, kSecond, kThird});

    const InstallReport report = ArchiveInstallViewModel::ReportOfWhatCannotBeInstalled(offer);

    QCOMPARE(report.kind, InstallReportKind::NothingToInstall);
    QCOMPARE(report.installed, 0);
    QCOMPARE(report.notInstalled, 0);
    QCOMPARE(report.archives.size(), std::size_t{2});
    QCOMPARE(report.archives[0].fileName, QStringLiteral("first.zip"));
    QCOMPARE(report.archives[0].lines.size(), std::size_t{1});
    QVERIFY(report.archives[0].lines.front().addon.isEmpty());
    QCOMPARE(report.archives[0].lines.front().reason, offer.archives[0].refusal);
    QCOMPARE(report.archives[1].fileName, QStringLiteral("third.7z"));
    QCOMPARE(report.archives[1].lines.front().reason, offer.archives[2].refusal);
}

namespace
{
    const std::filesystem::path kDroppedFolder = "D:/Downloads/Meu Addon";

    void ThereIsAFolderDropped(Fixture& f)
    {
        f.fileSystem.AddDirectory(kDroppedFolder.parent_path());
        f.fileSystem.AddDirectory(kDroppedFolder);
    }
}

void ArchiveInstallViewModelTest::AFolderIsAFolderForTheViewModelAndAnArchiveIsNot()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.fileSystem.AddFileWithContents(kFirst, "PK");

    QVERIFY(f.viewModel.IsAFolder(kDroppedFolder));
    QVERIFY(!f.viewModel.IsAFolder(kFirst));
    QVERIFY(!f.viewModel.IsAFolder("D:/Downloads/missing"));
}

void ArchiveInstallViewModelTest::AFolderIsOfferedLikeAnArchiveAndTheOfferSaysWhichIsWhich()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});

    const InstallOffer offer = f.Reads({kDroppedFolder, kFirst});

    QCOMPARE(offer.archives.size(), std::size_t{2});
    QCOMPARE(offer.archives[0].fileName, QStringLiteral("Meu Addon"));
    QCOMPARE(offer.archives[0].path, AsText(kDroppedFolder));
    QCOMPARE(offer.archives[0].source, SourceKind::Folder);
    QCOMPARE(offer.archives[0].addons.size(), std::size_t{1});
    QCOMPARE(offer.archives[0].addons.front().folderName, QStringLiteral("Meu Addon"));
    QCOMPARE(offer.archives[1].source, SourceKind::Archive);
    QCOMPARE(Fixture::Chooses(offer, 0, 0).source, SourceKind::Folder);
    QCOMPARE(Fixture::Chooses(offer, 1, 0).source, SourceKind::Archive);
}

void ArchiveInstallViewModelTest::AJunctionToAFolderThatReachesTheReadingCarriesTheSentenceOfAFolder()
{
    Fixture f;
    const std::filesystem::path junction = "D:/Downloads/Shortcut";
    ThereIsAFolderDropped(f);
    f.fileSystem.AddLink(junction, kDroppedFolder);
    f.archives.At(junction).FailsToList(ArchiveFailure::CouldNotOpen);

    const InstallOffer offer = f.Reads({junction});

    QVERIFY(!f.viewModel.IsAFolder(junction));
    QCOMPARE(offer.archives.front().source, SourceKind::Folder);
    QCOMPARE(offer.archives.front().refusal,
             WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("Shortcut"), SourceKind::Folder));
    QVERIFY(offer.archives.front().refusal
            != WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("Shortcut"), SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderThatCouldNotBeReadCarriesTheSentenceOfAFolder()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.archives.At(kDroppedFolder).FailsToList(ArchiveFailure::CouldNotOpen);

    const InstallOffer offer = f.Reads({kDroppedFolder});

    QCOMPARE(offer.archives.front().refusal,
             WhyTheArchiveIsRefused(ArchiveFailure::CouldNotOpen, QStringLiteral("Meu Addon"), SourceKind::Folder));
    QVERIFY(!offer.archives.front().refusal.contains(QStringLiteral("archive")));
}

void ArchiveInstallViewModelTest::AFolderIsAnnouncedAsCopyingAndAnArchiveAsExtracting()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kDroppedFolder, kFirst});
    QStringList steps;

    QObject::connect(&f.viewModel, &ArchiveInstallViewModel::StepChanged, &f.viewModel,
                     [&steps](const InstallStep step)
                     {
                         steps << NameOf(step);
                     });

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0), Fixture::Chooses(offer, 1, 0)}, AsText(kUtils), false);

    QCOMPARE(
        steps,
        (QStringList{QStringLiteral("Copying"), QStringLiteral("Checking"), QStringLiteral("PuttingIntoPlace"),
                     QStringLiteral("Extracting"), QStringLiteral("Checking"), QStringLiteral("PuttingIntoPlace")}));
}

void ArchiveInstallViewModelTest::AFolderThatCouldNotBeCopiedIsToldAsACopy()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    f.archives.At(kDroppedFolder).FailsToExtract(ArchiveFailure::CouldNotExtract);
    const InstallOffer offer = f.Reads({kDroppedFolder});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->archives.front().fileName, QStringLiteral("Meu Addon"));
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::CouldNotExtract, SourceKind::Folder));
    QVERIFY(outcome.report->archives.front().lines.front().reason
            != WhyItWasNotInstalled(FileResult::CouldNotExtract, SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderThatTookTooLongToCopyIsToldAsACopy()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    f.archives.At(kDroppedFolder).FailsAfterWritingEverything(ArchiveFailure::TookTooLong);
    const InstallOffer offer = f.Reads({kDroppedFolder});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyTheArchiveIsRefused(ArchiveFailure::TookTooLong, QStringLiteral("Meu Addon"), SourceKind::Folder));
    QVERIFY(outcome.report->archives.front().lines.front().reason
            != WhyTheArchiveIsRefused(ArchiveFailure::TookTooLong, QStringLiteral("Meu Addon"), SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderTheCopyOfWhichStayedBehindIsToldAsACopy()
{
    Fixture f;
    ThereIsAFolderDropped(f);
    f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    const InstallOffer offer = f.Reads({kDroppedFolder});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);
    f.files.MakeTheRemovalFailFor(InstallStagingIn(kUtils));

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->kind, InstallReportKind::InstalledButAFolderStayed);
    QCOMPARE(outcome.report->archives.front().leftBehind, TheExtractionFolderStayed(SourceKind::Folder));
    QVERIFY(outcome.report->archives.front().leftBehind != TheExtractionFolderStayed(SourceKind::Archive));
}

void ArchiveInstallViewModelTest::AFolderThatIsInTheWayOfTheInstallIsToldInBothDirections()
{
    Fixture f;
    f.ArchiveHolds(kLibrary, {{"alpha", "1.0.0"}});
    const InstallOffer offer = f.Reads({kLibrary});
    const QSignalSpy finished(&f.viewModel, &ArchiveInstallViewModel::Finished);

    QCOMPARE(offer.archives.front().source, SourceKind::Folder);

    f.viewModel.Install({Fixture::Chooses(offer, 0, 0)}, AsText(kUtils), false);

    const InstallOutcome outcome = TheEndOf(finished);
    QVERIFY(outcome.report.has_value());
    QCOMPARE(outcome.report->archives.front().lines.front().reason,
             WhyItWasNotInstalled(FileResult::TheSourceIsInsideWhatWouldBeRemoved, SourceKind::Folder));
    QVERIFY(!f.fileSystem.Exists(kUtils / "alpha"));
}

QTEST_GUILESS_MAIN(ArchiveInstallViewModelTest)

#include "tst_archive_install_view_model.moc"
