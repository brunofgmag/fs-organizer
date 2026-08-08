#include <QtTest/QtTest>

#include "domain/linking/EntryClassifier.h"
#include "tests/support/PathPrinting.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLinkService.h"
#include "tests/doubles/InMemoryFileSystem.h"

namespace
{
    class EntryClassifierTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ALinkIntoALibraryIsManaged();
        static void ALiveLinkOutsideEveryLibraryIsExternal();
        static void ALinkWhoseTargetIsGoneIsBroken();
        static void ALinkOnAnAbsentVolumeIsUnavailableRatherThanBroken();
        static void APhysicalFolderIsUnmanaged();
        static void AnAddonLinkedInTwoDestinationsIsDuplicated();
        static void AnExtendedLengthPrefixOnTheTargetIsNormalized();
        static void LibraryMatchingIgnoresPathCase();
        static void OnlyManagedEntriesReportTheirTargetAsAnEnabledAddon();
        static void ADuplicatedAddonIsStillEnabledAndReportedOnce();
        static void EveryLinkThatPointsAtAnAddonFolderIsListedAcrossDestinations();
        static void AnImportedExternalWhoseFolderTheOtherProgramRecreatedIsDivergent();
        static void ADivergentEntryCarriesTheFolderTheOtherProgramOwns();
        static void AnImportedExternalWhoseFolderIsStillALinkIsManaged();
        static void AnImportedExternalWhoseLibraryCopyIsGoneIsVanished();
        static void AVanishedEntryCarriesTheFolderTheOtherProgramOwns();
        static void AnOriginOnAnAbsentVolumeIsNotADivergence();
        static void ADivergentAddonIsStillEnabled();
        static void OnlyTheAddonsThatCameFromAnotherProgramCostAnExtraLook();
        static void ADivergentAddonLinkedTwiceIsDuplicatedAndStillKnowsAboutTheSecondCopy();
    };

    constexpr auto kVendorFolder = "C:/Program Files (x86)/Addon Manager/MSFS/gsx-pro";
    constexpr auto kLibraryCopy = "D:/Library/Utilities/gsx-pro";
}

namespace
{
    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeLinkService linkService{fileSystem};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        EntryClassifier classifier{linkService, filesystemProbe};
    };
}

void EntryClassifierTest::ALinkIntoALibraryIsManaged()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Managed);
    QCOMPARE(entries.front().target, std::filesystem::path("D:/Library/Aircrafts/aerosoft-crj"));
}

void EntryClassifierTest::ALiveLinkOutsideEveryLibraryIsExternal()
{
    const std::filesystem::path foreignTarget = "C:/Program Files (x86)/Addon Manager/MSFS/fsdreamteam-gsx-pro";

    Fixture f;
    f.fileSystem.AddDirectory(foreignTarget);
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/fsdreamteam-gsx-pro", foreignTarget);

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::External);
    QCOMPARE(entries.front().target, foreignTarget);
}

void EntryClassifierTest::ALinkWhoseTargetIsGoneIsBroken()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/ag-airport-bgqq-qaanaaq", "D:/Library/Sceneries/ag-airport-bgqq-qaanaaq");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Broken);
}

void EntryClassifierTest::ALinkOnAnAbsentVolumeIsUnavailableRatherThanBroken()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/orbx-airport", "Z:/Portable Library/orbx-airport");
    f.fileSystem.MarkVolumeUnavailable("Z:/");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"Z:/Portable Library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Unavailable);
}

void EntryClassifierTest::APhysicalFolderIsUnmanaged()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Sim/Community/asfs");
    f.fileSystem.AddFile("E:/Sim/Community/asfs/manifest.json");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Unmanaged);
    QVERIFY(entries.front().target.empty());
}

void EntryClassifierTest::AnAddonLinkedInTwoDestinationsIsDuplicated()
{
    const std::filesystem::path addonFolder = "D:/Library/Aircrafts/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddDirectory(addonFolder);
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Sim/Community2024");
    f.fileSystem.AddLink("E:/Sim/Community/pmdg-aircraft-77w", addonFolder);
    f.fileSystem.AddLink("E:/Sim/Community2024/pmdg-aircraft-77w", addonFolder);

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community", "E:/Sim/Community2024"}, {"D:/Library"});

    QCOMPARE(entries.size(), std::size_t{2});
    for (const DestinationEntry& entry : entries)
    {
        QCOMPARE(entry.classification, EntryClassification::Duplicated);
    }
}

void EntryClassifierTest::AnExtendedLengthPrefixOnTheTargetIsNormalized()
{
    const std::filesystem::path realTarget = "E:/Aerosoft One Library/Add-ons/aerosoft-aircraft-a346-pro";

    Fixture f;
    f.fileSystem.AddDirectory(realTarget);
    f.fileSystem.AddDirectory("C:/Packages/Community");
    f.fileSystem.AddLink("C:/Packages/Community/aerosoft-aircraft-a346-pro",
                         R"(\\?\E:\Aerosoft One Library\Add-ons\aerosoft-aircraft-a346-pro)");

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"C:/Packages/Community"}, {"E:/Aerosoft One Library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().target, realTarget);
    QCOMPARE(entries.front().classification, EntryClassification::Managed);
}

void EntryClassifierTest::LibraryMatchingIgnoresPathCase()
{
    Fixture f;
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"d:/library"});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Managed);
}

void EntryClassifierTest::OnlyManagedEntriesReportTheirTargetAsAnEnabledAddon()
{
    const std::filesystem::path enabledAddon = "D:/Library/Aircrafts/aerosoft-crj";

    Fixture f;
    f.fileSystem.AddDirectory(enabledAddon);
    f.fileSystem.AddDirectory("C:/Program Files/Other/foreign-addon");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", enabledAddon);
    f.fileSystem.AddLink("E:/Sim/Community/foreign-addon", "C:/Program Files/Other/foreign-addon");
    f.fileSystem.AddLink("E:/Sim/Community/tlc-bgjn", "D:/Library/Sceneries/tlc-bgjn");
    f.fileSystem.AddDirectory("E:/Sim/Community/asfs");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"});
    const std::vector<std::filesystem::path> enabled = EnabledAddonFolders(entries);

    QCOMPARE(entries.size(), std::size_t{4});
    QCOMPARE(enabled.size(), std::size_t{1});
    QCOMPARE(enabled.front(), enabledAddon);
}

void EntryClassifierTest::ADuplicatedAddonIsStillEnabledAndReportedOnce()
{
    const std::filesystem::path addonFolder = "D:/Library/Aircrafts/pmdg-aircraft-77w";

    Fixture f;
    f.fileSystem.AddDirectory(addonFolder);
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Sim/Community2024");
    f.fileSystem.AddLink("E:/Sim/Community/pmdg-aircraft-77w", addonFolder);
    f.fileSystem.AddLink("E:/Sim/Community2024/pmdg-aircraft-77w", addonFolder);

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community", "E:/Sim/Community2024"}, {"D:/Library"});
    const std::vector<std::filesystem::path> enabled = EnabledAddonFolders(entries);

    QCOMPARE(enabled.size(), std::size_t{1});
    QCOMPARE(enabled.front(), addonFolder);
}

void EntryClassifierTest::EveryLinkThatPointsAtAnAddonFolderIsListedAcrossDestinations()
{
    const std::filesystem::path addon = "D:/Library/Aircrafts/tfdi-md11";

    Fixture f;
    f.fileSystem.AddDirectory(addon);
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddDirectory("E:/Sim/Community2024");
    f.fileSystem.AddLink("E:/Sim/Community/tfdi-md11", addon);
    f.fileSystem.AddLink("E:/Sim/Community2024/tfdi-md11", addon);
    f.fileSystem.AddLink("E:/Sim/Community2024/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddDirectory("E:/Sim/Community2024/asfs");

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community", "E:/Sim/Community2024"}, {"D:/Library"});
    const std::vector<std::filesystem::path> pointing = LinksPointingAt(entries, addon);

    QCOMPARE(pointing.size(), std::size_t{2});
    QCOMPARE(pointing.front(), std::filesystem::path("E:/Sim/Community/tfdi-md11"));
    QCOMPARE(pointing.back(), std::filesystem::path("E:/Sim/Community2024/tfdi-md11"));
    QVERIFY(LinksPointingAt(entries, "D:/Library/Aircrafts/never-linked").empty());
}

namespace
{
    Fixture AnImportedExternal()
    {
        Fixture f;
        f.fileSystem.AddDirectory(kLibraryCopy);
        f.fileSystem.AddDirectory("E:/Sim/Community");
        f.fileSystem.AddLink("E:/Sim/Community/gsx-pro", kLibraryCopy);
        f.fileSystem.AddLink(kVendorFolder, kLibraryCopy);

        return f;
    }

    std::vector<ExternalAddon> TheOneWeImported()
    {
        return {ExternalAddon{.addonFolder = kLibraryCopy, .externalPath = kVendorFolder}};
    }
}

void EntryClassifierTest::AnImportedExternalWhoseFolderTheOtherProgramRecreatedIsDivergent()
{
    Fixture f = AnImportedExternal();
    QVERIFY(f.fileSystem.RemoveNode(kVendorFolder));
    f.fileSystem.AddDirectory(kVendorFolder);

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Divergent);
}

void EntryClassifierTest::ADivergentEntryCarriesTheFolderTheOtherProgramOwns()
{
    Fixture f = AnImportedExternal();
    QVERIFY(f.fileSystem.RemoveNode(kVendorFolder));
    f.fileSystem.AddDirectory(kVendorFolder);

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.front().externalOrigin, std::filesystem::path{kVendorFolder});
}

void EntryClassifierTest::AnImportedExternalWhoseFolderIsStillALinkIsManaged()
{
    const Fixture f = AnImportedExternal();

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Managed);
}

void EntryClassifierTest::AnImportedExternalWhoseLibraryCopyIsGoneIsVanished()
{
    Fixture f = AnImportedExternal();
    QVERIFY(f.fileSystem.RemoveNode(kLibraryCopy));

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Vanished);
}

void EntryClassifierTest::AVanishedEntryCarriesTheFolderTheOtherProgramOwns()
{
    Fixture f = AnImportedExternal();
    QVERIFY(f.fileSystem.RemoveNode(kLibraryCopy));

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.front().externalOrigin, std::filesystem::path{kVendorFolder});
}

void EntryClassifierTest::AnOriginOnAnAbsentVolumeIsNotADivergence()
{
    const std::filesystem::path awayFolder = "Y:/Vendor/MSFS/gsx-pro";

    Fixture f;
    f.fileSystem.AddDirectory(kLibraryCopy);
    f.fileSystem.AddDirectory("E:/Sim/Community");
    f.fileSystem.AddLink("E:/Sim/Community/gsx-pro", kLibraryCopy);
    f.fileSystem.MarkVolumeUnavailable("Y:/");

    const std::vector<DestinationEntry> entries = f.classifier.Resolve(
        {"E:/Sim/Community"}, {"D:/Library"}, {ExternalAddon{.addonFolder = kLibraryCopy, .externalPath = awayFolder}});

    QCOMPARE(entries.size(), std::size_t{1});
    QCOMPARE(entries.front().classification, EntryClassification::Managed);
}

void EntryClassifierTest::ADivergentAddonIsStillEnabled()
{
    Fixture f = AnImportedExternal();
    QVERIFY(f.fileSystem.RemoveNode(kVendorFolder));
    f.fileSystem.AddDirectory(kVendorFolder);

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.front().classification, EntryClassification::Divergent);
    QCOMPARE(EnabledAddonFolders(entries), std::vector<std::filesystem::path>{kLibraryCopy});
}

void EntryClassifierTest::OnlyTheAddonsThatCameFromAnotherProgramCostAnExtraLook()
{
    Fixture f = AnImportedExternal();
    f.fileSystem.AddDirectory("D:/Library/Aircrafts/aerosoft-crj");
    f.fileSystem.AddLink("E:/Sim/Community/aerosoft-crj", "D:/Library/Aircrafts/aerosoft-crj");

    static_cast<void>(f.classifier.Resolve({"E:/Sim/Community"}, {"D:/Library"}, TheOneWeImported()));

    QCOMPARE(f.filesystemProbe.TimesLookedAt(kVendorFolder), std::size_t{1});
    QCOMPARE(f.filesystemProbe.TimesLookedAt("D:/Library/Aircrafts/aerosoft-crj"), std::size_t{0});
}

void EntryClassifierTest::ADivergentAddonLinkedTwiceIsDuplicatedAndStillKnowsAboutTheSecondCopy()
{
    Fixture f = AnImportedExternal();
    QVERIFY(f.fileSystem.RemoveNode(kVendorFolder));
    f.fileSystem.AddDirectory(kVendorFolder);
    f.fileSystem.AddDirectory("E:/Sim/Community2024");
    f.fileSystem.AddLink("E:/Sim/Community2024/gsx-pro", kLibraryCopy);

    const std::vector<DestinationEntry> entries =
        f.classifier.Resolve({"E:/Sim/Community", "E:/Sim/Community2024"}, {"D:/Library"}, TheOneWeImported());

    QCOMPARE(entries.size(), std::size_t{2});
    for (const DestinationEntry& entry : entries)
    {
        QCOMPARE(entry.classification, EntryClassification::Duplicated);
        QVERIFY(entry.theOtherProgramTookItsFolderBack);
        QCOMPARE(entry.externalOrigin, std::filesystem::path{kVendorFolder});
    }
}

QTEST_APPLESS_MAIN(EntryClassifierTest)

#include "tst_entry_classifier.moc"
