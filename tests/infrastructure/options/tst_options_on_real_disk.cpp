#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>

#include "application/LibraryOrganizer.h"
#include "application/ProfileService.h"
#include "application/Session.h"
#include "domain/journal/OperationLog.h"
#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFileOperations.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/id/UuidLibraryIdGenerator.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/settings/JsonSettingsRepository.h"
#include "infrastructure/sim/WindowsProcessProbe.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/doubles/RecordingSessionObserver.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class OptionsOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AskingForASymlinkEitherLandsOnDiskOrNamesThePrivilegeItLacks();
        static void UnregisteringKeepsEveryJunctionOnDiskAndCallsThemThirdParty();
        static void RegisteringTheLibraryBackRebuildsTheTreeUnderANewIdentity();
        static void RepointingADestinationCarriesThePinnedCategoryIntoTheFileOnDisk();
    };
}

namespace
{
    constexpr auto kAddon = "pmdg-aircraft-77w";
    constexpr auto kOtherAddon = "fenix-a320";

    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return std::filesystem::path(directory.path().toStdWString());
        }

        [[nodiscard]] std::filesystem::path Library() const
        {
            return Root() / "Library";
        }

        [[nodiscard]] std::filesystem::path Community() const
        {
            return Root() / "Sim" / "Community";
        }

        [[nodiscard]] std::filesystem::path Extra() const
        {
            return Root() / "Sim" / "Community2024";
        }

        [[nodiscard]] std::filesystem::path Landing() const
        {
            return Root() / "Sim" / "Community2025";
        }

        [[nodiscard]] std::filesystem::path SettingsFile() const
        {
            return Root() / "settings.json";
        }

        void AddAddon(const std::string& name) const
        {
            const std::filesystem::path folder = Library() / "Aircrafts" / name;
            std::filesystem::create_directories(folder);

            std::ofstream stream(folder / "manifest.json", std::ios::binary);
            stream << R"({"title": ")" << name << R"(", "package_version": "1.0.0"})";
        }

        Disk()
        {
            std::filesystem::create_directories(Community());
            std::filesystem::create_directories(Extra());
            std::filesystem::create_directories(Landing());
            AddAddon(kAddon);
            AddAddon(kOtherAddon);
        }
    };

    struct Stack
    {
        explicit Stack(const Disk& disk, const LinkType linkType = LinkType::Junction)
            : journalFile(disk.Root() / "journal" / "operations.jsonl"),
              journal(journalFile),
              settingsFile(disk.SettingsFile()),
              settings(settingsFile),
              service(catalog, classifier, linking, log, identities, linkType),
              organizer(catalog, filesystemProbe, files, linking, classifier, processProbe, log, linkType),
              session(service, organizer, settings, processProbe, runner, observer)
        {
        }

        WindowsLinkService linkService;
        WindowsFilesystemProbe filesystemProbe;
        WindowsFileOperations files;
        LinkingEngine linking{linkService, filesystemProbe};
        EntryClassifier classifier{linkService, filesystemProbe};
        SystemClock clock;
        std::filesystem::path journalFile;
        JsonlOperationJournal journal;
        OperationLog log{journal, clock};
        UuidLibraryIdGenerator identities;
        JsonManifestParser manifestParser;
        FilesystemScanner catalog{manifestParser, filesystemProbe};
        WindowsProcessProbe processProbe{std::vector<std::string>{}};
        std::filesystem::path settingsFile;
        JsonSettingsRepository settings;
        ProfileService service;
        LibraryOrganizer organizer;
        InlineBackgroundRunner runner;
        RecordingSessionObserver observer;
        Session session;
    };

    [[nodiscard]] std::string WriteAProfileFor(const Disk& disk, Stack& stack)
    {
        SimulatorProfile profile;
        profile.id = "msfs2024";
        profile.variant = SimulatorVariant::MSFS2024;
        profile.destinations = {disk.Community(), disk.Extra()};
        profile.defaultDestination = disk.Community();
        profile.libraries = {Library{.id = "library-1", .path = disk.Library(), .label = "Biblioteca"}};

        AppSettings settings;
        settings.profiles = {profile};
        settings.activeProfileId = profile.id;

        [[maybe_unused]] const bool saved = stack.settings.Save(settings);

        return profile.id;
    }

    [[nodiscard]] const TreeNode* AddonUnder(const Stack& stack, const std::string& name)
    {
        return AddonNamed(stack.session.Snapshot().libraries, name);
    }
}

void OptionsOnRealDiskTest::AskingForASymlinkEitherLandsOnDiskOrNamesThePrivilegeItLacks()
{
    const Disk disk;
    Stack symbolic(disk, LinkType::Symbolic);
    static_cast<void>(WriteAProfileFor(disk, symbolic));
    symbolic.session.ShowActiveProfile();

    const TreeNode* addon = AddonUnder(symbolic, kAddon);
    QVERIFY2(addon != nullptr, "the scan did not find the addon, so asking for the link proves nothing");

    const std::vector<LinkOperationResult> asked =
        symbolic.service.SetEnabled(symbolic.session.Profile(), symbolic.session.Snapshot(), {addon}, true);

    QCOMPARE(asked.size(), std::size_t{1});

    const std::filesystem::path linkPath = disk.Community() / kAddon;

    if (asked.front().outcome.Succeeded())
    {
        qInfo("this machine creates directory symlinks: Developer Mode is on, or the session is elevated");
        QVERIFY(symbolic.filesystemProbe.IsReparsePoint(linkPath));

        symbolic.session.ShowActiveProfile();

        const TreeNode* linked = AddonUnder(symbolic, kAddon);
        QVERIFY(linked != nullptr);

        const std::vector<LinkOperationResult> undone =
            symbolic.service.SetEnabled(symbolic.session.Profile(), symbolic.session.Snapshot(), {linked}, false);

        QCOMPARE(undone.size(), std::size_t{1});
        QVERIFY2(!std::filesystem::exists(linkPath), "disabling left the link in the destination");
    }
    else
    {
        qInfo("this machine refuses directory symlinks, which is the case US-12.2 describes");
        QCOMPARE(asked.front().outcome.Failure(), LinkFailure::PrivilegeNotHeld);
        QVERIFY2(!std::filesystem::exists(linkPath), "the refusal left rubbish behind in the destination");
    }

    Stack junction(disk, LinkType::Junction);
    junction.session.ShowActiveProfile();

    const TreeNode* again = AddonUnder(junction, kAddon);
    QVERIFY(again != nullptr);

    const std::vector<LinkOperationResult> retried =
        junction.service.SetEnabled(junction.session.Profile(), junction.session.Snapshot(), {again}, true);

    QCOMPARE(retried.size(), std::size_t{1});
    QCOMPARE(retried.front().outcome.Failure(), LinkFailure::None);
    QVERIFY(junction.filesystemProbe.IsReparsePoint(linkPath));
    QCOMPARE(junction.linkService.ReadLinkTarget(linkPath).value_or(std::filesystem::path{}),
             disk.Library() / "Aircrafts" / kAddon);
}

void OptionsOnRealDiskTest::UnregisteringKeepsEveryJunctionOnDiskAndCallsThemThirdParty()
{
    const Disk disk;
    Stack stack(disk);
    static_cast<void>(WriteAProfileFor(disk, stack));
    stack.session.ShowActiveProfile();

    const TreeNode* addon = AddonUnder(stack, kAddon);
    QVERIFY(addon != nullptr);
    static_cast<void>(stack.service.SetEnabled(stack.session.Profile(), stack.session.Snapshot(), {addon}, true));

    const std::filesystem::path linkPath = disk.Community() / kAddon;
    QVERIFY2(stack.filesystemProbe.IsReparsePoint(linkPath),
             "the junction was not created, so demanding it alive afterwards proves nothing");

    stack.session.UnregisterLibrary("library-1");

    QVERIFY2(stack.filesystemProbe.IsReparsePoint(linkPath), "unregistering deleted the link from the destination");
    QVERIFY2(std::filesystem::exists(disk.Library() / "Aircrafts" / kAddon), "unregistering deleted the real folder");
    QVERIFY(stack.session.Profile().libraries.empty());

    const auto& entries = stack.session.Snapshot().entries;
    const auto found = std::ranges::find_if(entries,
                                            [&linkPath](const DestinationEntry& entry)
                                            {
                                                return ComparablePath(entry.path) == ComparablePath(linkPath);
                                            });

    QVERIFY2(found != entries.end(), "the next scan did not see the link left behind in the destination");
    QCOMPARE(found->classification, EntryClassification::External);
}

void OptionsOnRealDiskTest::RegisteringTheLibraryBackRebuildsTheTreeUnderANewIdentity()
{
    const Disk disk;
    Stack stack(disk);
    static_cast<void>(WriteAProfileFor(disk, stack));
    stack.session.ShowActiveProfile();

    QCOMPARE(stack.session.Snapshot().libraries.size(), std::size_t{1});
    const std::size_t addonsBefore = CountAddons(stack.session.Snapshot().libraries.front());
    QCOMPARE(addonsBefore, std::size_t{2});

    stack.session.UnregisterLibrary("library-1");
    QVERIFY(stack.session.Snapshot().libraries.empty());

    const LibraryReport report = stack.session.RegisterLibrary(disk.Library());

    QVERIFY2(report.Accepted(), "registering the same folder back was refused");
    QCOMPARE(stack.session.Profile().libraries.size(), std::size_t{1});
    QCOMPARE(stack.session.Snapshot().libraries.size(), std::size_t{1});
    QCOMPARE(CountAddons(stack.session.Snapshot().libraries.front()), addonsBefore);

    QVERIFY2(stack.session.Profile().libraries.front().id != LibraryId("library-1"),
             "the library came back with the old identifier, and the note promises it is new");
}

void OptionsOnRealDiskTest::RepointingADestinationCarriesThePinnedCategoryIntoTheFileOnDisk()
{
    const Disk disk;
    Stack stack(disk);
    static_cast<void>(WriteAProfileFor(disk, stack));
    stack.session.ShowActiveProfile();

    const TreeNode* library = &stack.session.Snapshot().libraries.front();
    const auto category = std::ranges::find_if(library->children,
                                               [](const TreeNode& child)
                                               {
                                                   return child.path.filename() == "Aircrafts";
                                               });

    QVERIFY2(category != library->children.end(), "the scan did not find the category that would be pinned");

    stack.session.OverrideDestination({&*category}, disk.Extra());

    const std::optional<AppSettings> pinned = stack.settings.Load();
    QVERIFY(pinned.has_value());
    QCOMPARE(pinned->profiles.front().destinationOverrides.size(), std::size_t{1});
    QCOMPARE(pinned->profiles.front().destinationOverrides.front().destination, disk.Extra());

    stack.session.RepointDestination(disk.Extra(), disk.Landing());

    const std::optional<AppSettings> moved = stack.settings.Load();
    QVERIFY(moved.has_value());
    QCOMPARE(moved->profiles.front().destinations[1], disk.Landing());
    QCOMPARE(moved->profiles.front().destinationOverrides.front().destination, disk.Landing());
    QVERIFY2(std::filesystem::exists(disk.Extra()), "switching the destination path deleted the old folder");
}

QTEST_MAIN(OptionsOnRealDiskTest)

#include "tst_options_on_real_disk.moc"
