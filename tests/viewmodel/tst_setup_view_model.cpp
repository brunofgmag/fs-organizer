#include <QtTest/QtTest>

#include <filesystem>
#include <vector>

#include "tests/doubles/FakeCatalogScanner.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeLibraryIdGenerator.h"
#include "tests/doubles/FakeSettingsRepository.h"
#include "tests/doubles/FakeSimulatorLocator.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/PathPrinting.h"
#include "viewmodel/SetupViewModel.h"

class SetupViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void EveryDetectedCandidateIsProposedNotJustTheFirst();
    static void ADestinationThatDoesNotExistIsRejected();
    static void ADestinationThatIsNotWritableIsRejected();
    static void AFolderNotNamedCommunityIsAcceptedWithAWarning();
    static void ACommunityFolderIsAcceptedWithoutWarning();
    static void EachRegisteredLibraryGetsItsOwnGeneratedIdentity();
    static void CompletingSetupPersistsTheChosenCandidateWithItsLibraries();
    static void AManuallyPointedFolderBecomesACandidateThatCanBeChosen();
    static void RegisteringALibraryReportsTheCategoriesAndAddonsFoundInside();
    static void AFolderInsideAnAlreadyRegisteredLibraryIsRefused();
    static void ASecondProfileIsAppendedAndKeepsADistinctIdentity();
};

namespace
{
    SimulatorCandidate Candidate(const SimulatorVariant variant,
                                 const std::filesystem::path& packages,
                                 const std::vector<std::string>& destinationNames = {"Community"})
    {
        SimulatorCandidate candidate;
        candidate.variant = variant;
        candidate.packagesPath = packages;

        for (const std::string& name : destinationNames)
        {
            candidate.destinations.push_back(packages / name);
        }

        return candidate;
    }

    TreeNode Category(const std::filesystem::path& path, const int addons)
    {
        TreeNode category;
        category.kind = TreeNodeKind::Category;
        category.path = path;

        for (int index = 0; index < addons; ++index)
        {
            TreeNode addon;
            addon.kind = TreeNodeKind::Addon;
            addon.path = path / ("addon-" + std::to_string(index));
            addon.addon = Addon{addon.path, {}};
            category.children.push_back(addon);
        }

        return category;
    }

    TreeNode LibraryTree(const std::filesystem::path& root, const std::vector<std::pair<std::string, int>>& categories)
    {
        TreeNode library;
        library.kind = TreeNodeKind::Library;
        library.path = root;

        for (const auto& [name, addons] : categories)
        {
            library.children.push_back(Category(root / name, addons));
        }

        return library;
    }

    struct Fixture
    {
        InMemoryFileSystem fileSystem;
        FakeSimulatorLocator locator{{}};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeSettingsRepository settings;
        FakeLibraryIdGenerator identities;
        FakeCatalogScanner catalog;
        SetupService service{locator, filesystemProbe, settings, identities, catalog};
        SetupViewModel viewModel{service};
    };
}

void SetupViewModelTest::EveryDetectedCandidateIsProposedNotJustTheFirst()
{
    const FakeSimulatorLocator locator({
        Candidate(SimulatorVariant::MSFS2020, "C:/Packages"),
        Candidate(SimulatorVariant::MSFS2024, "E:/Flight Simulator 2024"),
        Candidate(SimulatorVariant::MSFS2024, "F:/Second Install"),
    });

    InMemoryFileSystem fileSystem;
    const FakeFilesystemProbe filesystemProbe(fileSystem);
    FakeSettingsRepository settings;
    const FakeLibraryIdGenerator identities;
    const FakeCatalogScanner catalog;

    SetupService service(locator, filesystemProbe, settings, identities, catalog);
    SetupViewModel viewModel(service);
    viewModel.Detect();

    const std::vector<SimulatorCandidate> proposed = viewModel.Candidates();

    QCOMPARE(proposed.size(), std::size_t{3});
    QCOMPARE(proposed[0].packagesPath, std::filesystem::path("C:/Packages"));
    QCOMPARE(proposed[2].packagesPath, std::filesystem::path("F:/Second Install"));
}

void SetupViewModelTest::ADestinationThatDoesNotExistIsRejected()
{
    const Fixture f;

    QCOMPARE(f.viewModel.CheckDestination("E:/Flight Simulator 2024/Community"), DestinationCheck::RejectedMissing);
}

void SetupViewModelTest::ADestinationThatIsNotWritableIsRejected()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community");
    f.fileSystem.MarkReadOnly("E:/Flight Simulator 2024/Community");

    QCOMPARE(f.viewModel.CheckDestination("E:/Flight Simulator 2024/Community"), DestinationCheck::RejectedNotWritable);
}

void SetupViewModelTest::AFolderNotNamedCommunityIsAcceptedWithAWarning()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/fragtality-commbus-module");

    QCOMPARE(f.viewModel.CheckDestination("E:/Flight Simulator 2024/fragtality-commbus-module"),
             DestinationCheck::AcceptedButUnfamiliar);
}

void SetupViewModelTest::ACommunityFolderIsAcceptedWithoutWarning()
{
    Fixture f;
    f.fileSystem.AddDirectory("E:/Flight Simulator 2024/Community2024");
    f.fileSystem.AddDirectory("C:/Packages/community");

    QCOMPARE(f.viewModel.CheckDestination("E:/Flight Simulator 2024/Community2024"), DestinationCheck::Accepted);
    QCOMPARE(f.viewModel.CheckDestination("C:/Packages/community"), DestinationCheck::Accepted);
}

void SetupViewModelTest::EachRegisteredLibraryGetsItsOwnGeneratedIdentity()
{
    Fixture f;
    (void)f.viewModel.RegisterLibrary("D:/MSFS 2024", "MSFS 2024");
    (void)f.viewModel.RegisterLibrary("Z:/Portable Library", "Portátil");

    const std::vector<RegisteredLibrary> libraries = f.viewModel.Libraries();

    QCOMPARE(libraries.size(), std::size_t{2});
    QCOMPARE(libraries[0].library.id, std::string("library-1"));
    QCOMPARE(libraries[0].library.path, std::filesystem::path("D:/MSFS 2024"));
    QCOMPARE(libraries[0].library.label, std::string("MSFS 2024"));
    QCOMPARE(libraries[1].library.id, std::string("library-2"));
}

void SetupViewModelTest::CompletingSetupPersistsTheChosenCandidateWithItsLibraries()
{
    Fixture f;
    f.locator = FakeSimulatorLocator({
        Candidate(SimulatorVariant::MSFS2020, "C:/Packages"),
        Candidate(SimulatorVariant::MSFS2024, "E:/Flight Simulator 2024", {"Community", "Community2024"}),
    });

    f.viewModel.Detect();
    f.viewModel.ChooseCandidate(1);
    (void)f.viewModel.RegisterLibrary("D:/MSFS 2024", "MSFS 2024");
    f.viewModel.Complete();

    QCOMPARE(f.settings.saves, 1);
    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{1});

    const SimulatorProfile& profile = f.settings.stored.profiles.front();
    QCOMPARE(profile.variant, SimulatorVariant::MSFS2024);
    QCOMPARE(profile.id, std::string("msfs2024"));
    QCOMPARE(f.settings.stored.activeProfileId, profile.id);
    QCOMPARE(profile.destinations.size(), std::size_t{2});
    QCOMPARE(profile.defaultDestination, std::filesystem::path("E:/Flight Simulator 2024/Community"));
    QCOMPARE(profile.libraries.size(), std::size_t{1});
    QCOMPARE(profile.libraries.front().id, std::string("library-1"));
}

void SetupViewModelTest::AManuallyPointedFolderBecomesACandidateThatCanBeChosen()
{
    Fixture f;
    f.locator = FakeSimulatorLocator({Candidate(SimulatorVariant::MSFS2020, "C:/Packages")});
    f.viewModel.Detect();

    f.viewModel.AddManualCandidate("X:/Somewhere Else/Community", SimulatorVariant::MSFS2024);

    QCOMPARE(f.viewModel.Candidates().size(), std::size_t{2});

    f.viewModel.ChooseCandidate(1);
    f.viewModel.Complete();

    QCOMPARE(f.settings.saves, 1);

    const SimulatorProfile& profile = f.settings.stored.profiles.front();
    QCOMPARE(profile.variant, SimulatorVariant::MSFS2024);
    QCOMPARE(profile.destinations.size(), std::size_t{1});
    QCOMPARE(profile.destinations.front(), std::filesystem::path("X:/Somewhere Else/Community"));
    QCOMPARE(profile.defaultDestination, std::filesystem::path("X:/Somewhere Else/Community"));
}

void SetupViewModelTest::RegisteringALibraryReportsTheCategoriesAndAddonsFoundInside()
{
    Fixture f;
    f.catalog.SetTree("D:/MSFS 2024", LibraryTree("D:/MSFS 2024", {{"Sceneries", 173}, {"Liveries", 27}}));
    f.catalog.SetTree("D:/MSFS 2024/Sceneries", LibraryTree("D:/MSFS 2024/Sceneries", {}));

    QCOMPARE(f.viewModel.RegisterLibrary("D:/MSFS 2024", "MSFS 2024").check, LibraryCheck::Accepted);

    const std::vector<RegisteredLibrary> registered = f.viewModel.Libraries();
    QCOMPARE(registered.size(), std::size_t{1});
    QCOMPARE(registered.front().categories, std::size_t{2});
    QCOMPARE(registered.front().addons, std::size_t{200});
    QCOMPARE(registered.front().library.label, std::string("MSFS 2024"));
}

void SetupViewModelTest::AFolderInsideAnAlreadyRegisteredLibraryIsRefused()
{
    Fixture f;
    f.catalog.SetTree("D:/MSFS 2024", LibraryTree("D:/MSFS 2024", {{"Sceneries", 173}}));

    QCOMPARE(f.viewModel.RegisterLibrary("D:/MSFS 2024", "MSFS 2024").check, LibraryCheck::Accepted);
    QCOMPARE(f.viewModel.RegisterLibrary("D:/MSFS 2024/Sceneries", "Sceneries").check,
             LibraryCheck::RejectedInsideAnotherLibrary);
    QCOMPARE(f.viewModel.RegisterLibrary("d:/msfs 2024", "De novo").check, LibraryCheck::RejectedInsideAnotherLibrary);

    QCOMPARE(f.viewModel.Libraries().size(), std::size_t{1});
}

void SetupViewModelTest::ASecondProfileIsAppendedAndKeepsADistinctIdentity()
{
    Fixture f;

    SimulatorProfile existing;
    existing.id = "msfs2024";
    existing.variant = SimulatorVariant::MSFS2024;
    f.settings.stored.profiles = {existing};
    f.settings.stored.activeProfileId = "msfs2024";

    f.locator = FakeSimulatorLocator({
        Candidate(SimulatorVariant::MSFS2024, "F:/Second Install"),
        Candidate(SimulatorVariant::MSFS2020, "C:/Packages"),
    });

    f.viewModel.Detect();
    f.viewModel.ChooseCandidate(0);
    f.viewModel.Complete();

    QCOMPARE(f.settings.stored.profiles.size(), std::size_t{2});
    QCOMPARE(f.settings.stored.profiles[0].id, std::string("msfs2024"));
    QCOMPARE(f.settings.stored.profiles[1].id, std::string("msfs2024-2"));
    QCOMPARE(f.settings.stored.activeProfileId, std::string("msfs2024-2"));
}

QTEST_APPLESS_MAIN(SetupViewModelTest)

#include "tst_setup_view_model.moc"
