#include <QtTest/QtTest>

#include "domain/linking/LinkingEngine.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"

class LinkingEngineTest : public QObject
{
    Q_OBJECT

private slots:
    static void EnablingIntoAFreeDestinationLinksToTheAddonFolder();
    static void DisablingRemovesTheReparseNodeAndLeavesTheTargetIntact();
    static void DisablingRefusesWhenThePathIsNotAReparsePoint();
    static void EnablingRefusesWhenTheDestinationHoldsARealFolder();
    static void EnablingReplacesAStaleLinkAtTheDestination();
    static void EnablingRefusesWhenTheDestinationHoldsALiveForeignLink();
    static void EnablingRefusesWhenTheExistingLinkTargetCannotBeRead();
    static void ADanglingLinkStillCountsAsAnExistingEntry();
};

namespace
{
    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFileOperations fileOperations{fileSystem};
        LinkingEngine engine{linkService, fileOperations};
    };
}

void LinkingEngineTest::EnablingIntoAFreeDestinationLinksToTheAddonFolder()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddDirectory("E:/Sim/Community");

    const Addon addon{"D:/Library/Aircrafts/aerosoft-crj"};
    const LinkOutcome outcome = f.engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QVERIFY(outcome.Succeeded());
    QVERIFY(f.fileSystem.IsLink("E:/Sim/Community/aerosoft-crj"));
    QCOMPARE(f.fileSystem.LinkTarget("E:/Sim/Community/aerosoft-crj").value(),
             std::filesystem::path("D:/Library/Aircrafts/aerosoft-crj"));
}

void LinkingEngineTest::DisablingRemovesTheReparseNodeAndLeavesTheTargetIntact()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddFile("D:/Library/Aircrafts/aerosoft-crj/manifest.json");
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj/SimObjects");
    f.fileSystem.AddFile("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");

    const LinkOutcome outcome = f.engine.Disable("E:/Sim/Community/aerosoft-crj");

    QVERIFY(outcome.Succeeded());
    QVERIFY(!f.fileSystem.Exists("E:/Sim/Community/aerosoft-crj"));
    QVERIFY(f.fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj"));
    QVERIFY(f.fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/manifest.json"));
    QVERIFY(f.fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects"));
    QVERIFY(f.fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf"));
}

void LinkingEngineTest::DisablingRefusesWhenThePathIsNotAReparsePoint()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Sim/Community/asfs");
    f.fileSystem.AddFile("E:/Sim/Community/asfs/manifest.json");

    const LinkOutcome outcome = f.engine.Disable("E:/Sim/Community/asfs");

    QCOMPARE(outcome.Failure(), LinkFailure::PathIsNotAReparsePoint);
    QVERIFY(f.fileSystem.Exists("E:/Sim/Community/asfs"));
    QVERIFY(f.fileSystem.Exists("E:/Sim/Community/asfs/manifest.json"));
}

void LinkingEngineTest::EnablingRefusesWhenTheDestinationHoldsARealFolder()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Utils/flybywire-externaltools-simbridge");
    f.fileSystem.AddFile("D:/Library/Utils/flybywire-externaltools-simbridge/manifest.json");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Sim/Community/flybywire-externaltools-simbridge");
    f.fileSystem.AddFile("E:/Sim/Community/flybywire-externaltools-simbridge/manifest.json");

    const Addon addon{"D:/Library/Utils/flybywire-externaltools-simbridge"};
    const LinkOutcome outcome = f.engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QCOMPARE(outcome.Failure(), LinkFailure::DestinationHoldsRealFolder);
    QVERIFY(outcome.Conflict().has_value());
    QCOMPARE(outcome.Conflict()->destinationPath,
             std::filesystem::path("E:/Sim/Community/flybywire-externaltools-simbridge"));
    QCOMPARE(outcome.Conflict()->libraryPath,
             std::filesystem::path("D:/Library/Utils/flybywire-externaltools-simbridge"));
    QVERIFY(!f.fileSystem.IsLink("E:/Sim/Community/flybywire-externaltools-simbridge"));
    QVERIFY(f.fileSystem.Exists("E:/Sim/Community/flybywire-externaltools-simbridge/manifest.json"));
}

void LinkingEngineTest::EnablingReplacesAStaleLinkAtTheDestination()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Sceneries/tlc-bgjn");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/tlc-bgjn", "D:/Library/Sceneries/tlc-bgjn-removed");

    const Addon addon{"D:/Library/Sceneries/tlc-bgjn"};
    const LinkOutcome outcome = f.engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QVERIFY(outcome.Succeeded());
    QCOMPARE(f.fileSystem.LinkTarget("E:/Sim/Community/tlc-bgjn").value(),
             std::filesystem::path("D:/Library/Sceneries/tlc-bgjn"));
}

void LinkingEngineTest::EnablingRefusesWhenTheDestinationHoldsALiveForeignLink()
{
    const std::filesystem::path foreignTarget = "C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro";

    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Sceneries/fsdreamteam-gsx-pro");
    f.fileSystem.AddDirectory(foreignTarget);
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/fsdreamteam-gsx-pro", foreignTarget);

    const Addon addon{"D:/Library/Sceneries/fsdreamteam-gsx-pro"};
    const LinkOutcome outcome = f.engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QCOMPARE(outcome.Failure(), LinkFailure::DestinationHoldsLiveLink);
    QVERIFY(outcome.Occupation().has_value());
    QCOMPARE(outcome.Occupation()->existingTarget, foreignTarget);
    QCOMPARE(f.fileSystem.LinkTarget("E:/Sim/Community/fsdreamteam-gsx-pro").value(), foreignTarget);
}

void LinkingEngineTest::EnablingRefusesWhenTheExistingLinkTargetCannotBeRead()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Sceneries/tlc-bgkk");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLinkWithUnreadableTarget("E:/Sim/Community/tlc-bgkk");

    const Addon addon{"D:/Library/Sceneries/tlc-bgkk"};
    const LinkOutcome outcome = f.engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QCOMPARE(outcome.Failure(), LinkFailure::UnreadableLinkTarget);
    QVERIFY(f.fileSystem.Exists("E:/Sim/Community/tlc-bgkk"));
}

void LinkingEngineTest::ADanglingLinkStillCountsAsAnExistingEntry()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/ag-airport-bgqq-qaanaaq",
                         "D:/Library/Sceneries/ag-airport-bgqq-qaanaaq");

    QVERIFY(!f.fileOperations.TargetDirectoryExists("D:/Library/Sceneries/ag-airport-bgqq-qaanaaq"));
    QVERIFY(f.fileOperations.EntryExistsWithoutFollowingLinks("E:/Sim/Community/ag-airport-bgqq-qaanaaq"));
}

QTEST_APPLESS_MAIN(LinkingEngineTest)

#include "tst_linking_engine.moc"
