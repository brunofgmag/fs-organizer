#include <QtTest/QtTest>

#include "domain/linking/LinkingEngine.h"
#include "tests/doubles/FakeFileOperations.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"

class LinkingEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void EnablingIntoAFreeDestinationLinksToTheAddonFolder();
    void DisablingRemovesTheReparseNodeAndLeavesTheTargetIntact();
    void DisablingRefusesWhenThePathIsNotAReparsePoint();
    void EnablingRefusesWhenTheDestinationHoldsARealFolder();
    void EnablingReplacesAStaleLinkAtTheDestination();
    void EnablingRefusesWhenTheDestinationHoldsALiveForeignLink();
};

void LinkingEngineTest::EnablingIntoAFreeDestinationLinksToTheAddonFolder()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    fileSystem.AddDirectory("E:/Sim/Community");

    FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const LinkingEngine engine(linkService, fileOperations);

    const Addon addon{"D:/Library/Aircrafts/aerosoft-crj"};
    const LinkOutcome outcome = engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QVERIFY2(outcome.Succeeded(), outcome.Message().c_str());
    QVERIFY(fileSystem.IsLink("E:/Sim/Community/aerosoft-crj"));
    QCOMPARE(fileSystem.LinkTarget("E:/Sim/Community/aerosoft-crj").value(),
             std::filesystem::path("D:/Library/Aircrafts/aerosoft-crj"));
}

void LinkingEngineTest::DisablingRemovesTheReparseNodeAndLeavesTheTargetIntact()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    fileSystem.AddFile("D:/Library/Aircrafts/aerosoft-crj/manifest.json");
    fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj/SimObjects");
    fileSystem.AddFile("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf");
    fileSystem.AddDirectory("E:/Sim/Community");
    fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");

    FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const LinkingEngine engine(linkService, fileOperations);

    const LinkOutcome outcome = engine.Disable("E:/Sim/Community/aerosoft-crj");

    QVERIFY2(outcome.Succeeded(), outcome.Message().c_str());
    QVERIFY(!fileSystem.Exists("E:/Sim/Community/aerosoft-crj"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/manifest.json"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects"));
    QVERIFY(fileSystem.Exists("D:/Library/Aircrafts/aerosoft-crj/SimObjects/model.gltf"));
}

void LinkingEngineTest::DisablingRefusesWhenThePathIsNotAReparsePoint()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("E:/Sim/Community");
    fileSystem.AddDirectory("E:/Sim/Community/asfs");
    fileSystem.AddFile("E:/Sim/Community/asfs/manifest.json");

    FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const LinkingEngine engine(linkService, fileOperations);

    const LinkOutcome outcome = engine.Disable("E:/Sim/Community/asfs");

    QVERIFY(!outcome.Succeeded());
    QVERIFY(fileSystem.Exists("E:/Sim/Community/asfs"));
    QVERIFY(fileSystem.Exists("E:/Sim/Community/asfs/manifest.json"));
}

void LinkingEngineTest::EnablingRefusesWhenTheDestinationHoldsARealFolder()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/Library/Utils/flybywire-externaltools-simbridge");
    fileSystem.AddFile("D:/Library/Utils/flybywire-externaltools-simbridge/manifest.json");
    fileSystem.AddDirectory("E:/Sim/Community");
    fileSystem.AddDirectory("E:/Sim/Community/flybywire-externaltools-simbridge");
    fileSystem.AddFile("E:/Sim/Community/flybywire-externaltools-simbridge/manifest.json");

    FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const LinkingEngine engine(linkService, fileOperations);

    const Addon addon{"D:/Library/Utils/flybywire-externaltools-simbridge"};
    const LinkOutcome outcome = engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QVERIFY(!outcome.Succeeded());
    QVERIFY(outcome.Conflict().has_value());
    QCOMPARE(outcome.Conflict()->DestinationPath,
             std::filesystem::path("E:/Sim/Community/flybywire-externaltools-simbridge"));
    QCOMPARE(outcome.Conflict()->LibraryPath,
             std::filesystem::path("D:/Library/Utils/flybywire-externaltools-simbridge"));
    QVERIFY(!fileSystem.IsLink("E:/Sim/Community/flybywire-externaltools-simbridge"));
    QVERIFY(fileSystem.Exists("E:/Sim/Community/flybywire-externaltools-simbridge/manifest.json"));
}

void LinkingEngineTest::EnablingReplacesAStaleLinkAtTheDestination()
{
    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/Library/Sceneries/tlc-bgjn");
    fileSystem.AddDirectory("E:/Sim/Community");
    fileSystem.AddLink("E:/Sim/Community/tlc-bgjn", "D:/Library/Sceneries/tlc-bgjn-removed");

    FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const LinkingEngine engine(linkService, fileOperations);

    const Addon addon{"D:/Library/Sceneries/tlc-bgjn"};
    const LinkOutcome outcome = engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QVERIFY2(outcome.Succeeded(), outcome.Message().c_str());
    QCOMPARE(fileSystem.LinkTarget("E:/Sim/Community/tlc-bgjn").value(),
             std::filesystem::path("D:/Library/Sceneries/tlc-bgjn"));
}

void LinkingEngineTest::EnablingRefusesWhenTheDestinationHoldsALiveForeignLink()
{
    const std::filesystem::path foreignTarget =
            "C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro";

    InMemoryFileSystem fileSystem;
    fileSystem.AddDirectory("D:/Library/Sceneries/fsdreamteam-gsx-pro");
    fileSystem.AddDirectory(foreignTarget);
    fileSystem.AddDirectory("E:/Sim/Community");
    fileSystem.AddLink("E:/Sim/Community/fsdreamteam-gsx-pro", foreignTarget);

    FakeLinkService linkService(fileSystem);
    const FakeFileOperations fileOperations(fileSystem);
    const LinkingEngine engine(linkService, fileOperations);

    const Addon addon{"D:/Library/Sceneries/fsdreamteam-gsx-pro"};
    const LinkOutcome outcome = engine.Enable(addon, "E:/Sim/Community", LinkType::Junction);

    QVERIFY(!outcome.Succeeded());
    QVERIFY(outcome.Occupation().has_value());
    QCOMPARE(outcome.Occupation()->ExistingTarget, foreignTarget);
    QCOMPARE(fileSystem.LinkTarget("E:/Sim/Community/fsdreamteam-gsx-pro").value(), foreignTarget);
}

QTEST_APPLESS_MAIN(LinkingEngineTest)

#include "tst_linking_engine.moc"
