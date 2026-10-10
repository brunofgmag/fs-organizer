#include <QtTest/QtTest>

#include "domain/installing/PackagesInAnArchive.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    class PackagesInAnArchiveTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void APackageAtDepthOneIsFoundByItsManifest();
        static void APackageAtDepthTwoNamesItsInnerFolder();
        static void RootsAtDifferentDepthsLiveInTheSameArchive();
        static void APackageFolderNeedNotAppearAsAnEntryOfItsOwn();
        static void AManifestNestedInsideAPackageIsNotASecondPackage();
        static void TheOutermostRootWinsEvenWhenTheNestedOneIsListedFirst();
        static void ANestedRootWithTheSameNameAsItsOuterOneIsNotAClash();
        static void TheManifestNameIsMatchedWithoutCaringAboutCase();
        static void ARootIsMatchedWithoutCaringAboutTheCaseOfItsFolder();
        static void PackagesComeInTheOrderTheirRootsFirstAppear();
        static void ADotDotSegmentRefusesTheWholeArchive();
        static void AnAbsolutePathRefusesTheWholeArchive();
        static void ADrivePathRefusesTheWholeArchive();
        static void ANameWithAQuestionMarkRefusesTheWholeArchive();
        static void AnEmptyPathRefusesTheWholeArchive();
        static void ALinkRefusesTheWholeArchive();
        static void TwoPathsDifferingOnlyInCaseRefuseTheWholeArchive();
        static void AFileAndAFolderWithTheSamePathRefuseTheWholeArchive();
        static void AFileThatIsAlsoTheFolderOfAnotherEntryRefusesTheWholeArchive();
        static void SafetyIsJudgedBeforeAnyOtherRule();
        static void AManifestAtTheTopLevelHasNoFolder();
        static void AManifestAtTheTopLevelRefusesEvenBesideAGoodPackage();
        static void TwoRootsWithTheSameNameShareAName();
        static void TwoRootsWhoseNamesDifferOnlyInCaseShareAName();
        static void AnArchiveOfOnlyIniFilesHasNoPackage();
        static void AnArchiveWithAnExeAndNoPackageOnlyHoldsAnInstaller();
        static void AnMsiInAnyCaseAndFolderAlsoOnlyHoldsAnInstaller();
        static void AFolderNamedLikeAnExeIsNotAnInstaller();
        static void AnInstallerBesideAPackageDoesNotRefuse();
        static void AnEmptyListingHasNoPackage();
        static void ThePackageCountsItsFilesAndTheArchiveCountsEverything();
        static void FolderEntriesCountNothing();
        static void ThePackageKeepsItsManifestEntryAsListed();
        static void ANestedManifestIsNeverThePackageManifest();
        static void AReservedWindowsNameRefusesTheWholeArchive_data();
        static void AReservedWindowsNameRefusesTheWholeArchive();
        static void ANameThatOnlyResemblesAReservedOneIsFine_data();
        static void ANameThatOnlyResemblesAReservedOneIsFine();
        static void APackageFolderTheAppReservesRefusesTheWholeArchive_data();
        static void APackageFolderTheAppReservesRefusesTheWholeArchive();
        static void ANameTheAppReservesIsFineAnywhereButTheRootFolder();
        static void ARepeatedManifestRefusesTheWholeArchive();
    };
}

namespace
{
    ArchiveEntry File(std::string path, const std::uintmax_t bytes = 1)
    {
        return ArchiveEntry{.path = std::move(path), .bytes = bytes, .kind = ArchiveEntryKind::File};
    }

    ArchiveEntry Folder(std::string path, const std::uintmax_t bytes = 0)
    {
        return ArchiveEntry{.path = std::move(path), .bytes = bytes, .kind = ArchiveEntryKind::Folder};
    }

    ArchiveEntry Link(std::string path)
    {
        return ArchiveEntry{.path = std::move(path), .bytes = 0, .kind = ArchiveEntryKind::Other};
    }

    QString Shown(const std::string& text)
    {
        return QString::fromStdString(text);
    }

    QString FolderNameShown(const PackageInAnArchive& package)
    {
        return Shown(FolderNameOf(package).Text());
    }

    QString RootShown(const PackageInAnArchive& package)
    {
        return Shown(EntryPathOf(package));
    }

    void ExpectRefusal(const std::vector<ArchiveEntry>& entries, const NothingToInstall expected)
    {
        const WhatAnArchiveHolds holds = PackagesIn(entries);

        QCOMPARE(holds.refusal, expected);
        QVERIFY(holds.packages.empty());
        QCOMPARE(holds.files, std::size_t{0});
        QCOMPARE(holds.bytes, std::uintmax_t{0});
    }
}

void PackagesInAnArchiveTest::APackageAtDepthOneIsFoundByItsManifest()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        Folder("[RR WL] pacote ação"),
        File("[RR WL] pacote ação/manifest.json", 10),
        File("[RR WL] pacote ação/layout.json", 20),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(holds.packages.front().root.size(), std::size_t{1});
    QCOMPARE(FolderNameShown(holds.packages.front()), QStringLiteral("[RR WL] pacote ação"));
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("[RR WL] pacote ação"));
}

void PackagesInAnArchiveTest::APackageAtDepthTwoNamesItsInnerFolder()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("Pacotes [2024]/Aeroporto ção/manifest.json"),
        File("Pacotes [2024]/Aeroporto ção/scenery/a.bgl"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(holds.packages.front().root.size(), std::size_t{2});
    QCOMPARE(FolderNameShown(holds.packages.front()), QStringLiteral("Aeroporto ção"));
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("Pacotes [2024]/Aeroporto ção"));
}

void PackagesInAnArchiveTest::RootsAtDifferentDepthsLiveInTheSameArchive()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("shallow/manifest.json"),
        File("shallow/a.bin"),
        File("group/deep/manifest.json"),
        File("group/deep/b.bin"),
        File("group/readme.txt"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{2});
    QCOMPARE(RootShown(holds.packages[0]), QStringLiteral("shallow"));
    QCOMPARE(FolderNameShown(holds.packages[0]), QStringLiteral("shallow"));
    QCOMPARE(RootShown(holds.packages[1]), QStringLiteral("group/deep"));
    QCOMPARE(FolderNameShown(holds.packages[1]), QStringLiteral("deep"));
}

void PackagesInAnArchiveTest::APackageFolderNeedNotAppearAsAnEntryOfItsOwn()
{
    const WhatAnArchiveHolds holds = PackagesIn({File("only/manifest.json")});

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("only"));
}

void PackagesInAnArchiveTest::AManifestNestedInsideAPackageIsNotASecondPackage()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("outer/manifest.json", 5),
        File("outer/inner/manifest.json", 7),
        File("outer/inner/x.bin", 11),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("outer"));
    QCOMPARE(holds.packages.front().files, std::size_t{3});
    QCOMPARE(holds.packages.front().bytes, std::uintmax_t{23});
}

void PackagesInAnArchiveTest::TheOutermostRootWinsEvenWhenTheNestedOneIsListedFirst()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("outer/inner/manifest.json"),
        File("outer/manifest.json"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("outer"));
    QCOMPARE(holds.packages.front().files, std::size_t{2});
}

void PackagesInAnArchiveTest::ANestedRootWithTheSameNameAsItsOuterOneIsNotAClash()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("pack/manifest.json"),
        File("pack/sub/pack/manifest.json"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("pack"));
}

void PackagesInAnArchiveTest::TheManifestNameIsMatchedWithoutCaringAboutCase()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("Pacote/Manifest.JSON"),
        File("Pacote/layout.json"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("Pacote"));
    QCOMPARE(holds.packages.front().files, std::size_t{2});
}

void PackagesInAnArchiveTest::ARootIsMatchedWithoutCaringAboutTheCaseOfItsFolder()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("Pack/manifest.json", 1),
        File("PACK/data.bin", 2),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(RootShown(holds.packages.front()), QStringLiteral("Pack"));
    QCOMPARE(holds.packages.front().files, std::size_t{2});
    QCOMPARE(holds.packages.front().bytes, std::uintmax_t{3});
}

void PackagesInAnArchiveTest::PackagesComeInTheOrderTheirRootsFirstAppear()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("zulu/manifest.json"),
        File("alpha/manifest.json"),
        File("mike/manifest.json"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{3});
    QCOMPARE(FolderNameShown(holds.packages[0]), QStringLiteral("zulu"));
    QCOMPARE(FolderNameShown(holds.packages[1]), QStringLiteral("alpha"));
    QCOMPARE(FolderNameShown(holds.packages[2]), QStringLiteral("mike"));
}

void PackagesInAnArchiveTest::ADotDotSegmentRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("good/../../escape.txt")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::AnAbsolutePathRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("/etc/passwd")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::ADrivePathRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("C:/Windows/evil.dll")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::ANameWithAQuestionMarkRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("good/arquivo ?.txt")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::AnEmptyPathRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::ALinkRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), Link("good/shortcut")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::TwoPathsDifferingOnlyInCaseRefuseTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("good/Texture.dds"), File("good/texture.DDS")},
                  NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::AFileAndAFolderWithTheSamePathRefuseTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json"), File("good/data"), Folder("good/DATA")},
                  NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::AFileThatIsAlsoTheFolderOfAnotherEntryRefusesTheWholeArchive()
{
    ExpectRefusal({File("pkg"), File("pkg/manifest.json")}, NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File("pkg/manifest.json"), File("pkg")}, NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File("pkg/manifest.json"), File("pkg/data"), File("PKG/DATA/inner.bin")},
                  NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File("deep/pkg/manifest.json"), File("Deep")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::SafetyIsJudgedBeforeAnyOtherRule()
{
    ExpectRefusal({File("manifest.json"), File("../escape.txt")}, NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File("a/pack/manifest.json"), File("b/pack/manifest.json"), Link("b/pack/link")},
                  NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::AManifestAtTheTopLevelHasNoFolder()
{
    ExpectRefusal({File("manifest.json"), File("layout.json")}, NothingToInstall::ThePackageHasNoFolder);
}

void PackagesInAnArchiveTest::AManifestAtTheTopLevelRefusesEvenBesideAGoodPackage()
{
    ExpectRefusal({File("good/manifest.json"), File("Manifest.json")}, NothingToInstall::ThePackageHasNoFolder);
}

void PackagesInAnArchiveTest::TwoRootsWithTheSameNameShareAName()
{
    ExpectRefusal({File("first/pack/manifest.json"), File("second/pack/manifest.json")},
                  NothingToInstall::TwoPackagesShareAName);
}

void PackagesInAnArchiveTest::TwoRootsWhoseNamesDifferOnlyInCaseShareAName()
{
    ExpectRefusal({File("first/Pacote Ação/manifest.json"), File("pack/pacote ação/manifest.json")},
                  NothingToInstall::TwoPackagesShareAName);
}

void PackagesInAnArchiveTest::AnArchiveOfOnlyIniFilesHasNoPackage()
{
    ExpectRefusal({File("settings/a.ini"), File("b.ini"), Folder("settings")}, NothingToInstall::NoPackage);
}

void PackagesInAnArchiveTest::AnArchiveWithAnExeAndNoPackageOnlyHoldsAnInstaller()
{
    ExpectRefusal({File("readme.txt"), File("Setup/Install.exe")}, NothingToInstall::OnlyAnInstaller);
}

void PackagesInAnArchiveTest::AnMsiInAnyCaseAndFolderAlsoOnlyHoldsAnInstaller()
{
    ExpectRefusal({File("tools/PACOTE.MSI")}, NothingToInstall::OnlyAnInstaller);
    ExpectRefusal({File("tools/PACOTE.Exe")}, NothingToInstall::OnlyAnInstaller);
}

void PackagesInAnArchiveTest::AFolderNamedLikeAnExeIsNotAnInstaller()
{
    ExpectRefusal({Folder("setup.exe"), File("setup.exe/readme.txt")}, NothingToInstall::NoPackage);
}

void PackagesInAnArchiveTest::AnInstallerBesideAPackageDoesNotRefuse()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("Instalar.exe", 100),
        File("pacote/manifest.json", 1),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
}

void PackagesInAnArchiveTest::AnEmptyListingHasNoPackage()
{
    ExpectRefusal({}, NothingToInstall::NoPackage);
}

void PackagesInAnArchiveTest::ThePackageCountsItsFilesAndTheArchiveCountsEverything()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("Leiame.txt", 100),
        File("one/manifest.json", 1),
        File("one/a.bgl", 20),
        File("two/manifest.json", 2),
        File("two/sub/b.bgl", 300),
        File("two/sub/c.bgl", 4000),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{2});
    QCOMPARE(holds.packages[0].files, std::size_t{2});
    QCOMPARE(holds.packages[0].bytes, std::uintmax_t{21});
    QCOMPARE(holds.packages[1].files, std::size_t{3});
    QCOMPARE(holds.packages[1].bytes, std::uintmax_t{4302});
    QCOMPARE(holds.files, std::size_t{6});
    QCOMPARE(holds.bytes, std::uintmax_t{4423});
}

void PackagesInAnArchiveTest::FolderEntriesCountNothing()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        Folder("one", 4096),
        File("one/manifest.json", 8),
        Folder("one/sub", 4096),
        Folder("loose", 4096),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(holds.packages.front().files, std::size_t{1});
    QCOMPARE(holds.packages.front().bytes, std::uintmax_t{8});
    QCOMPARE(holds.files, std::size_t{1});
    QCOMPARE(holds.bytes, std::uintmax_t{8});
}

void PackagesInAnArchiveTest::ThePackageKeepsItsManifestEntryAsListed()
{
    const WhatAnArchiveHolds holds = PackagesIn({
        File("Pacote/layout.json"),
        File("Pacote/Manifest.JSON"),
        File("Outro/manifest.json"),
    });

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{2});
    QCOMPARE(Shown(holds.packages[0].manifest), QStringLiteral("Pacote/Manifest.JSON"));
    QCOMPARE(Shown(holds.packages[1].manifest), QStringLiteral("Outro/manifest.json"));
}

void PackagesInAnArchiveTest::ANestedManifestIsNeverThePackageManifest()
{
    const WhatAnArchiveHolds nestedFirst = PackagesIn({
        File("Pacote/sub/manifest.json"),
        File("Pacote/manifest.json"),
    });
    const WhatAnArchiveHolds nestedLast = PackagesIn({
        File("Pacote/manifest.json"),
        File("Pacote/sub/manifest.json"),
    });

    QCOMPARE(nestedFirst.packages.size(), std::size_t{1});
    QCOMPARE(Shown(nestedFirst.packages.front().manifest), QStringLiteral("Pacote/manifest.json"));
    QCOMPARE(nestedLast.packages.size(), std::size_t{1});
    QCOMPARE(Shown(nestedLast.packages.front().manifest), QStringLiteral("Pacote/manifest.json"));
}

void PackagesInAnArchiveTest::AReservedWindowsNameRefusesTheWholeArchive_data()
{
    QTest::addColumn<std::string>("name");

    QTest::newRow("nul") << std::string("NUL");
    QTest::newRow("nul lower") << std::string("nul");
    QTest::newRow("con mixed") << std::string("Con");
    QTest::newRow("prn") << std::string("PRN");
    QTest::newRow("aux") << std::string("aux");
    QTest::newRow("com1") << std::string("COM1");
    QTest::newRow("com9 lower") << std::string("com9");
    QTest::newRow("lpt1") << std::string("LPT1");
    QTest::newRow("lpt9 lower") << std::string("lpt9");
    QTest::newRow("with an extension") << std::string("NUL.txt");
    QTest::newRow("with two extensions") << std::string("aux.tar.gz");
    QTest::newRow("spaces before the extension") << std::string("CON .txt");
    QTest::newRow("com superscript one") << std::string("COM\xC2\xB9");
    QTest::newRow("lpt superscript three") << std::string("lpt\xC2\xB3.log");
    QTest::newRow("conin") << std::string("CONIN$");
    QTest::newRow("conout") << std::string("conout$");
}

void PackagesInAnArchiveTest::AReservedWindowsNameRefusesTheWholeArchive()
{
    QFETCH(std::string, name);

    ExpectRefusal({File("good/manifest.json"), File("good/" + name)}, NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File("good/manifest.json"), Folder("good/" + name), File("good/" + name + "/a.bgl")},
                  NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File(name + "/manifest.json")}, NothingToInstall::AnEntryIsNotSafe);
}

void PackagesInAnArchiveTest::ANameThatOnlyResemblesAReservedOneIsFine_data()
{
    QTest::addColumn<std::string>("name");

    QTest::newRow("com0") << std::string("COM0");
    QTest::newRow("com10") << std::string("COM10");
    QTest::newRow("lpt") << std::string("LPT");
    QTest::newRow("console") << std::string("console.txt");
    QTest::newRow("nullable") << std::string("nullable");
    QTest::newRow("auxiliary") << std::string("auxiliary.dat");
    QTest::newRow("prefixed") << std::string("my-nul");
    QTest::newRow("com in the middle") << std::string("a.com1");
}

void PackagesInAnArchiveTest::ANameThatOnlyResemblesAReservedOneIsFine()
{
    QFETCH(std::string, name);

    const WhatAnArchiveHolds holds = PackagesIn({File("good/manifest.json"), File("good/" + name)});

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
}

void PackagesInAnArchiveTest::APackageFolderTheAppReservesRefusesTheWholeArchive_data()
{
    QTest::addColumn<std::string>("name");

    QTest::newRow("quarantine") << std::string("_fsorganizer-quarantine");
    QTest::newRow("quarantine in capitals") << std::string("_FSORGANIZER-Quarantine");
    QTest::newRow("partial") << std::string("Pacote.fsorg-partial");
    QTest::newRow("the install staging") << std::string("_install.fsorg-partial");
    QTest::newRow("swap") << std::string("Pacote.fsorg-swap");
    QTest::newRow("swap in capitals") << std::string("Pacote.FSORG-SWAP");
}

void PackagesInAnArchiveTest::APackageFolderTheAppReservesRefusesTheWholeArchive()
{
    QFETCH(std::string, name);

    ExpectRefusal({File(name + "/manifest.json")}, NothingToInstall::ThePackageNameIsReserved);
    ExpectRefusal({File("outer/" + name + "/manifest.json"), File("good/manifest.json")},
                  NothingToInstall::ThePackageNameIsReserved);
}

void PackagesInAnArchiveTest::ANameTheAppReservesIsFineAnywhereButTheRootFolder()
{
    const WhatAnArchiveHolds inside = PackagesIn({
        File("good/manifest.json"),
        File("good/_fsorganizer-quarantine/a.bgl"),
        File("good/scenery.fsorg-partial/b.bgl"),
        File("good/c.fsorg-swap"),
    });
    const WhatAnArchiveHolds above = PackagesIn({File("_fsorganizer-quarantine/good/manifest.json")});
    const WhatAnArchiveHolds inTheMiddle = PackagesIn({File("good.fsorg-partial.old/manifest.json")});

    QCOMPARE(inside.refusal, NothingToInstall::None);
    QCOMPARE(inside.packages.size(), std::size_t{1});
    QCOMPARE(above.refusal, NothingToInstall::None);
    QCOMPARE(above.packages.size(), std::size_t{1});
    QCOMPARE(inTheMiddle.refusal, NothingToInstall::None);
    QCOMPARE(inTheMiddle.packages.size(), std::size_t{1});
}

void PackagesInAnArchiveTest::ARepeatedManifestRefusesTheWholeArchive()
{
    ExpectRefusal({File("good/manifest.json", 10), File("good/layout.json"), File("good/manifest.json", 99)},
                  NothingToInstall::AnEntryIsNotSafe);
    ExpectRefusal({File("good/manifest.json"), File("good/manifest.json")}, NothingToInstall::AnEntryIsNotSafe);
}

QTEST_APPLESS_MAIN(PackagesInAnArchiveTest)

#include "tst_packages_in_an_archive.moc"
