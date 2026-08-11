#include <QtTest/QtTest>

#include "tests/doubles/FakeUpdateService.h"
#include "viewmodel/UpdateViewModel.h"

namespace
{
    class UpdateViewModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void WithUpdatesOffNothingIsCheckedAndTheScreenSaysWhy();
        static void TheManualModeDoesNotLookOnItsOwn();
        static void TheNotifyModeSaysAVersionIsThereWithoutDownloadingIt();
        static void TheAutomaticModeDownloadsAsSoonAsItKnows();
        static void AStagedUpdateIsAppliedOnExitUnlessTheModeIsManual();
        static void ACheckNobodyAskedForFailsQuietly();
        static void ACheckTheUserAskedForShowsTheFailure();
        static void ChoosingAutomaticWhileAVersionWaitsStartsTheDownload();
        static void AQuietCheckHoldsTheDoorSoAHandCheckDoesNotStackOnTopOfIt();
        static void AVersionThatBroughtNoFileIsNotOfferedForDownload();
    };
}

namespace
{
    UpdateInfo Version(const std::string& version)
    {
        UpdateInfo info;
        info.version = version;
        info.zipUrl = "https://example.invalid/fs-organizer-" + version + ".zip";
        info.shaUrl = info.zipUrl + ".sha256";
        info.zipName = "fs-organizer-" + version + ".zip";

        return info;
    }
}

void UpdateViewModelTest::WithUpdatesOffNothingIsCheckedAndTheScreenSaysWhy()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Automatic, false);

    viewModel.Check();
    viewModel.CheckQuietly();

    QCOMPARE(service.checks, 0);
    QVERIFY(!viewModel.CanCheck());
    QVERIFY(viewModel.WhatIsGoingOn().contains(QStringLiteral("FSORG_NO_UPDATES")));
}

void UpdateViewModelTest::TheManualModeDoesNotLookOnItsOwn()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Manual, true);

    viewModel.CheckQuietly();
    QCOMPARE(service.checks, 0);

    viewModel.Check();
    QCOMPARE(service.checks, 1);
}

void UpdateViewModelTest::TheNotifyModeSaysAVersionIsThereWithoutDownloadingIt()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true);

    viewModel.CheckQuietly();
    service.SayTheCheckFound(Version("0.2.0"), true);

    QCOMPARE(viewModel.State(), UpdateState::Available);
    QCOMPARE(service.downloads, 0);
    QVERIFY(viewModel.CanDownload());
    QVERIFY(viewModel.WhatIsGoingOn().contains(QStringLiteral("0.2.0")));
}

void UpdateViewModelTest::TheAutomaticModeDownloadsAsSoonAsItKnows()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Automatic, true);

    viewModel.CheckQuietly();
    service.SayTheCheckFound(Version("0.2.0"), true);

    QCOMPARE(service.downloads, 1);
    QCOMPARE(QString::fromStdString(service.asked.version), QStringLiteral("0.2.0"));
    QCOMPARE(viewModel.State(), UpdateState::Downloading);

    service.SayTheStageFinished(true);

    QCOMPARE(viewModel.State(), UpdateState::ReadyToApply);
}

void UpdateViewModelTest::AStagedUpdateIsAppliedOnExitUnlessTheModeIsManual()
{
    FakeUpdateService service;
    UpdateViewModel automatic(service, UpdateMode::Automatic, true);

    QVERIFY(!automatic.ShouldApplyOnExit());

    service.SayTheStageFinished(true);

    QVERIFY(automatic.ShouldApplyOnExit());

    UpdateViewModel manual(service, UpdateMode::Manual, true);

    QVERIFY(!manual.ShouldApplyOnExit());
}

void UpdateViewModelTest::ACheckNobodyAskedForFailsQuietly()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true);

    viewModel.CheckQuietly();
    service.SayTheCheckFailed("HTTP 503");

    QCOMPARE(viewModel.State(), UpdateState::Idle);
}

void UpdateViewModelTest::ACheckTheUserAskedForShowsTheFailure()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true);

    viewModel.Check();
    service.SayTheCheckFailed("HTTP 503");

    QCOMPARE(viewModel.State(), UpdateState::Failed);
    QCOMPARE(viewModel.WhatIsGoingOn(), QStringLiteral("HTTP 503"));
}

void UpdateViewModelTest::ChoosingAutomaticWhileAVersionWaitsStartsTheDownload()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true);

    viewModel.Check();
    service.SayTheCheckFound(Version("0.2.0"), true);

    QCOMPARE(service.downloads, 0);

    const QSignalSpy chosen(&viewModel, &UpdateViewModel::ModeChosen);
    viewModel.ChooseMode(UpdateMode::Automatic);

    QCOMPARE(chosen.count(), 1);
    QCOMPARE(service.downloads, 1);
}

void UpdateViewModelTest::AQuietCheckHoldsTheDoorSoAHandCheckDoesNotStackOnTopOfIt()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true);

    viewModel.CheckQuietly();

    QCOMPARE(service.checks, 1);
    QVERIFY2(!viewModel.CanCheck(), "the silent check left the door open to stack another on top");

    viewModel.Check();

    QCOMPARE(service.checks, 1);
}

void UpdateViewModelTest::AVersionThatBroughtNoFileIsNotOfferedForDownload()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true);

    UpdateInfo withoutFiles;
    withoutFiles.version = "0.2.0";

    viewModel.Check();
    service.SayTheCheckFound(withoutFiles, true);

    QCOMPARE(viewModel.State(), UpdateState::Available);
    QVERIFY2(!viewModel.CanDownload(), "the screen offered to download a version that brought no file at all");
}

QTEST_APPLESS_MAIN(UpdateViewModelTest)

#include "tst_update_view_model.moc"
