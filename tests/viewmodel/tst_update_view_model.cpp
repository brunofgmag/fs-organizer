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
        static void AStoredAutomaticThatCannotUpdateItselfNeverDownloads();
        static void ChoosingAutomaticIsIgnoredWhereTheProgramCannotUpdateItself();
        static void AVersionFoundWhereTheProgramCannotUpdateItselfSendsTheUserToFlightsimTo();
        static void NothingIsAppliedOnExitWhereTheProgramCannotUpdateItself();
        static void EachDeliverySaysWhetherTheProgramUpdatesItself();
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
    UpdateViewModel viewModel(service, UpdateMode::Automatic, false, UpdateDelivery::SelfUpdate);

    viewModel.Check();
    viewModel.CheckQuietly();

    QCOMPARE(service.checks, 0);
    QVERIFY(!viewModel.CanCheck());
    QVERIFY(viewModel.WhatIsGoingOn().contains(QStringLiteral("FSORG_NO_UPDATES")));
}

void UpdateViewModelTest::TheManualModeDoesNotLookOnItsOwn()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Manual, true, UpdateDelivery::SelfUpdate);

    viewModel.CheckQuietly();
    QCOMPARE(service.checks, 0);

    viewModel.Check();
    QCOMPARE(service.checks, 1);
}

void UpdateViewModelTest::TheNotifyModeSaysAVersionIsThereWithoutDownloadingIt()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);

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
    UpdateViewModel viewModel(service, UpdateMode::Automatic, true, UpdateDelivery::SelfUpdate);

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
    UpdateViewModel automatic(service, UpdateMode::Automatic, true, UpdateDelivery::SelfUpdate);

    QVERIFY(!automatic.ShouldApplyOnExit());

    service.SayTheStageFinished(true);

    QVERIFY(automatic.ShouldApplyOnExit());

    UpdateViewModel manual(service, UpdateMode::Manual, true, UpdateDelivery::SelfUpdate);

    QVERIFY(!manual.ShouldApplyOnExit());
}

void UpdateViewModelTest::ACheckNobodyAskedForFailsQuietly()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);

    viewModel.CheckQuietly();
    service.SayTheCheckFailed("HTTP 503");

    QCOMPARE(viewModel.State(), UpdateState::Idle);
}

void UpdateViewModelTest::ACheckTheUserAskedForShowsTheFailure()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);

    viewModel.Check();
    service.SayTheCheckFailed("HTTP 503");

    QCOMPARE(viewModel.State(), UpdateState::Failed);
    QCOMPARE(viewModel.WhatIsGoingOn(), QStringLiteral("HTTP 503"));
}

void UpdateViewModelTest::ChoosingAutomaticWhileAVersionWaitsStartsTheDownload()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);

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
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);

    viewModel.CheckQuietly();

    QCOMPARE(service.checks, 1);
    QVERIFY2(!viewModel.CanCheck(), "the silent check left the door open to stack another on top");

    viewModel.Check();

    QCOMPARE(service.checks, 1);
}

void UpdateViewModelTest::AVersionThatBroughtNoFileIsNotOfferedForDownload()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);

    UpdateInfo withoutFiles;
    withoutFiles.version = "0.2.0";

    viewModel.Check();
    service.SayTheCheckFound(withoutFiles, true);

    QCOMPARE(viewModel.State(), UpdateState::Available);
    QVERIFY2(!viewModel.CanDownload(), "the screen offered to download a version that brought no file at all");
}

void UpdateViewModelTest::AStoredAutomaticThatCannotUpdateItselfNeverDownloads()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Automatic, true, UpdateDelivery::NoticeOnly);

    QCOMPARE(viewModel.Mode(), UpdateMode::Notify);

    viewModel.CheckQuietly();
    service.SayTheCheckFound(Version("0.2.0"), true);

    QCOMPARE(service.downloads, 0);
    QCOMPARE(viewModel.State(), UpdateState::Available);
    QVERIFY2(!viewModel.CanDownload(), "a copy that cannot update itself offered to download the new version");

    viewModel.Download();

    QCOMPARE(service.downloads, 0);
}

void UpdateViewModelTest::ChoosingAutomaticIsIgnoredWhereTheProgramCannotUpdateItself()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::NoticeOnly);

    viewModel.Check();
    service.SayTheCheckFound(Version("0.2.0"), true);

    const QSignalSpy chosen(&viewModel, &UpdateViewModel::ModeChosen);
    viewModel.ChooseMode(UpdateMode::Automatic);

    QCOMPARE(chosen.count(), 0);
    QCOMPARE(viewModel.Mode(), UpdateMode::Notify);
    QCOMPARE(service.downloads, 0);
}

void UpdateViewModelTest::AVersionFoundWhereTheProgramCannotUpdateItselfSendsTheUserToFlightsimTo()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Notify, true, UpdateDelivery::NoticeOnly);

    viewModel.Check();
    service.SayTheCheckFound(Version("0.2.0"), true);

    QVERIFY(viewModel.WhatIsGoingOn().contains(QStringLiteral("0.2.0")));
    QVERIFY2(viewModel.WhatIsGoingOn().contains(QStringLiteral("flightsim.to")),
             "the notice did not say where the new version is downloaded from");
}

void UpdateViewModelTest::NothingIsAppliedOnExitWhereTheProgramCannotUpdateItself()
{
    FakeUpdateService service;
    UpdateViewModel viewModel(service, UpdateMode::Automatic, true, UpdateDelivery::NoticeOnly);

    service.SayTheStageFinished(true);

    QVERIFY(service.HasStagedUpdate());
    QVERIFY2(!viewModel.ShouldApplyOnExit(), "a copy that cannot update itself asked to apply an update on exit");
    QVERIFY(viewModel.State() != UpdateState::ReadyToApply);

    viewModel.Check();
    service.SayTheCheckFound(Version("0.2.0"), true);

    QVERIFY2(viewModel.State() != UpdateState::ReadyToApply, "a check reported an update ready to install");
}

void UpdateViewModelTest::EachDeliverySaysWhetherTheProgramUpdatesItself()
{
    FakeUpdateService service;
    const UpdateViewModel itself(service, UpdateMode::Notify, true, UpdateDelivery::SelfUpdate);
    const UpdateViewModel noticeOnly(service, UpdateMode::Notify, true, UpdateDelivery::NoticeOnly);

    QVERIFY(itself.UpdatesItself());
    QVERIFY(!noticeOnly.UpdatesItself());
}

QTEST_APPLESS_MAIN(UpdateViewModelTest)

#include "tst_update_view_model.moc"
