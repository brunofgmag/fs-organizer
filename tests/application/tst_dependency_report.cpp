#include <QtTest/QtTest>

#include <chrono>
#include <string>
#include <vector>

#include "application/DependencyReport.h"
#include "tests/doubles/FakeSimulatorPackages.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    const std::filesystem::path kLibrary = "D:/Library";

    TreeNode AddonNode(const std::filesystem::path& path, Manifest manifest)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Addon;
        node.path = path;
        node.addon = Addon{.folderPath = path, .manifest = std::move(manifest)};

        return node;
    }

    TreeNode LibraryNode(std::vector<TreeNode> children)
    {
        TreeNode node;
        node.kind = TreeNodeKind::Library;
        node.path = kLibrary;
        node.children = std::move(children);

        return node;
    }

    Manifest Declaring(std::vector<DeclaredDependency> dependencies)
    {
        Manifest manifest;
        manifest.dependencies = std::move(dependencies);

        return manifest;
    }

    Addon AddonDeclaring(std::vector<DeclaredDependency> dependencies)
    {
        return Addon{.folderPath = kLibrary / "Utils" / "sim-rate-selector",
                     .manifest = Declaring(std::move(dependencies))};
    }

    ProfileSnapshot SnapshotOf(std::vector<TreeNode> addons, const std::vector<std::filesystem::path>& enabled)
    {
        ProfileSnapshot snapshot;
        snapshot.libraries.push_back(LibraryNode(std::move(addons)));
        snapshot.enabled = EnabledAddons(enabled);

        return snapshot;
    }

    class DependencyReportTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddonThatDeclaresNothingIsReportedWithNothing();
        static void ADependencyNamingAnAddonOfThisLibraryIsAnsweredByTheLibrary();
        static void TheLibraryAnswerWinsEvenWhenTheSimulatorAlsoCarriesThePackage();
        static void TheFolderNamesAreComparedWithoutCaseOnBothSides();
        static void TheLibraryAnswerSaysWhetherThatAddonIsOnAndAlsoWhenItIsOff();
        static void ADependencyTheSimulatorCarriesIsAnsweredByTheSimulator();
        static void TheDateOfTheListTravelsOnlyWhenTheListAnswered();
        static void TheAccountFolderTravelsWithTheDateWhenThereWasMoreThanOneList();
        static void WhatTheReadListDoesNotCarryIsNotVerifiableAndNeverMissing();
        static void WithNoListAtAllEveryUnmatchedDependencyIsNotVerifiable();
        static void NothingButTheFolderNameTakesPartInTheMatching();
        static void TheDeclaredVersionIsCarriedAsTheManifestWroteIt();
    };
}

void DependencyReportTest::AnAddonThatDeclaresNothingIsReportedWithNothing()
{
    const FakeSimulatorPackages packages;
    const ProfileSnapshot snapshot = SnapshotOf({}, {});

    const DependencyReport report = ReportDependencies(AddonDeclaring({}), snapshot, packages);

    QVERIFY(report.answers.empty());
    QVERIFY(!report.listTakenAt.has_value());
}

void DependencyReportTest::ADependencyNamingAnAddonOfThisLibraryIsAnsweredByTheLibrary()
{
    const FakeSimulatorPackages packages;
    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "pmdg-global-lib", {})}, {});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"pmdg-global-lib", "1.0.0"}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(1));
    QCOMPARE(report.answers[0].name, std::string("pmdg-global-lib"));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::InThisLibrary);
}

void DependencyReportTest::TheLibraryAnswerWinsEvenWhenTheSimulatorAlsoCarriesThePackage()
{
    FakeSimulatorPackages packages;
    packages.ReportAsInstalled("pmdg-global-lib");

    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "pmdg-global-lib", {})}, {});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"pmdg-global-lib", "1.0.0"}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(1));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::InThisLibrary);
}

void DependencyReportTest::TheFolderNamesAreComparedWithoutCaseOnBothSides()
{
    const FakeSimulatorPackages packages;
    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "PMDG-Global-Lib", {})}, {});

    const DependencyReport report = ReportDependencies(AddonDeclaring({{"pmdg-global-LIB", ""}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(1));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::InThisLibrary);
}

void DependencyReportTest::TheLibraryAnswerSaysWhetherThatAddonIsOnAndAlsoWhenItIsOff()
{
    const FakeSimulatorPackages packages;
    const std::filesystem::path lib = kLibrary / "Aircrafts" / "pmdg-global-lib";
    const std::filesystem::path sound = kLibrary / "Aircrafts" / "pmdg-sound-base";

    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(lib, {}), AddonNode(sound, {})}, {sound});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"pmdg-global-lib", ""}, {"pmdg-sound-base", ""}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(2));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::InThisLibrary);
    QCOMPARE(report.answers[0].enabled, false);
    QCOMPARE(report.answers[1].resolution, DependencyResolution::InThisLibrary);
    QCOMPARE(report.answers[1].enabled, true);
}

void DependencyReportTest::ADependencyTheSimulatorCarriesIsAnsweredByTheSimulator()
{
    FakeSimulatorPackages packages;
    packages.ReportAsInstalled("asobo-vcockpits-core");

    const ProfileSnapshot snapshot = SnapshotOf({}, {});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"asobo-vcockpits-core", "0.1.12"}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(1));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::InTheSimulator);
    QVERIFY(report.answers[0].libraryVersion.empty());
    QCOMPARE(report.answers[0].enabled, false);
}

void DependencyReportTest::TheDateOfTheListTravelsOnlyWhenTheListAnswered()
{
    const std::chrono::system_clock::time_point written =
        std::chrono::sys_days{std::chrono::year{2026} / std::chrono::August / 4} + std::chrono::hours{21};

    FakeSimulatorPackages packages;
    packages.ReportAsInstalled("asobo-vcockpits-core");
    packages.ReportTheListAsTakenAt(written);

    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "pmdg-global-lib", {})}, {});

    const DependencyReport carried =
        ReportDependencies(AddonDeclaring({{"asobo-vcockpits-core", ""}}), snapshot, packages);
    const DependencyReport fromTheLibraryAlone =
        ReportDependencies(AddonDeclaring({{"pmdg-global-lib", ""}}), snapshot, packages);

    QVERIFY(carried.listTakenAt.has_value());
    QCOMPARE(*carried.listTakenAt, written);
    QVERIFY(!fromTheLibraryAlone.listTakenAt.has_value());
}

void DependencyReportTest::TheAccountFolderTravelsWithTheDateWhenThereWasMoreThanOneList()
{
    FakeSimulatorPackages packages;
    packages.ReportAsInstalled("asobo-vcockpits-core");
    packages.ReportTheListAsReadFrom("NathosT");

    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "pmdg-global-lib", {})}, {});

    const DependencyReport named =
        ReportDependencies(AddonDeclaring({{"asobo-vcockpits-core", ""}}), snapshot, packages);
    const DependencyReport fromTheLibraryAlone =
        ReportDependencies(AddonDeclaring({{"pmdg-global-lib", ""}}), snapshot, packages);

    QCOMPARE(named.listAccountFolder, std::string("NathosT"));
    QCOMPARE(fromTheLibraryAlone.listAccountFolder, std::string());
}

void DependencyReportTest::WhatTheReadListDoesNotCarryIsNotVerifiableAndNeverMissing()
{
    FakeSimulatorPackages packages;
    packages.ReportAsInstalled("asobo-vcockpits-core");

    const ProfileSnapshot snapshot = SnapshotOf({}, {});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"fs-base-propdefs", "0.1.2"}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(1));
    QCOMPARE(packages.PresenceOf("fs-base-propdefs"), PackagePresence::Absent);
    QCOMPARE(report.answers[0].resolution, DependencyResolution::Unverifiable);
}

void DependencyReportTest::WithNoListAtAllEveryUnmatchedDependencyIsNotVerifiable()
{
    const FakeSimulatorPackages packages;
    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "pmdg-global-lib", {})}, {});

    const DependencyReport report = ReportDependencies(
        AddonDeclaring({{"pmdg-global-lib", ""}, {"fs-base-ui", ""}, {"as a346 light mod", ""}}), snapshot, packages);

    QCOMPARE(packages.PresenceOf("fs-base-ui"), PackagePresence::Unverifiable);
    QCOMPARE(report.answers.size(), std::size_t(3));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::InThisLibrary);
    QCOMPARE(report.answers[1].resolution, DependencyResolution::Unverifiable);
    QCOMPARE(report.answers[2].resolution, DependencyResolution::Unverifiable);
    QCOMPARE(report.answers[2].name, std::string("as a346 light mod"));
}

void DependencyReportTest::NothingButTheFolderNameTakesPartInTheMatching()
{
    FakeSimulatorPackages packages;
    packages.ReportAsInstalled("nothing-that-is-asked-about");

    Manifest wearingTheName;
    wearingTheName.title = "md11_light_mod";
    wearingTheName.packageVersion = "0.1.99";

    const ProfileSnapshot snapshot =
        SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "xmd11_light_mod_fs24", wearingTheName)}, {});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"md11_light_mod", "0.1.2"}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(1));
    QCOMPARE(report.answers[0].resolution, DependencyResolution::Unverifiable);
    QVERIFY(report.answers[0].libraryVersion.empty());
}

void DependencyReportTest::TheDeclaredVersionIsCarriedAsTheManifestWroteIt()
{
    const FakeSimulatorPackages packages;

    Manifest installed;
    installed.packageVersion = "1.0.1";

    const ProfileSnapshot snapshot = SnapshotOf({AddonNode(kLibrary / "Aircrafts" / "pmdg-global-lib", installed)}, {});

    const DependencyReport report =
        ReportDependencies(AddonDeclaring({{"pmdg-global-lib", "1.0.0"}, {"fs-base-ui", ""}}), snapshot, packages);

    QCOMPARE(report.answers.size(), std::size_t(2));
    QCOMPARE(report.answers[0].declaredVersion, std::string("1.0.0"));
    QCOMPARE(report.answers[0].libraryVersion, std::string("1.0.1"));
    QCOMPARE(report.answers[1].declaredVersion, std::string());
    QCOMPARE(report.answers[1].libraryVersion, std::string());
}

QTEST_APPLESS_MAIN(DependencyReportTest)

#include "tst_dependency_report.moc"
