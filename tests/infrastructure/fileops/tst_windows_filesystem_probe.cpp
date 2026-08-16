#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <windows.h>

#include <aclapi.h>

#include <vector>

#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "tests/support/StdFilesystemProbe.h"

namespace
{
    class WindowsFilesystemProbeTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ADanglingJunctionIsStillAnEntryThatOccupiesItsPath();
        static void ChildDirectoriesReportsDanglingJunctionsToo();
        static void AnUnmountedDriveLetterIsNotAnAvailableVolume();
        static void AJunctionIsAReparsePointAndARealFolderIsNot();
        static void OnlyARealFolderIsPhysicalAndALiveJunctionOverItIsNot();
        static void FreeSpaceIsOnlyAnswerableForAFolderThatAlreadyExists();
        static void AFolderThatIsNotThereIsNotTheSameAsOneThatRefusesTheWrite();
        static void AFolderThatWillNotTakeAFileSaysPermissionIsWhatStoppedIt();
        static void TheProbeCallsAFolderHeldExactlyWhenTheRenameIsRefused();
        static void AFolderReportsWhenItWasLastWrittenTo();
        static void TheStandardLibraryDoubleAnswersAJunctionTheSameWayThisProbeDoes();
        static void EveryQuestionAboutAnEntryPastTheOldCeilingIsAnswerable();
        static void ChildrenOfAFolderPastTheOldCeilingComeBackTheWayTheCallerNamesThem();
        static void TheStandardLibraryDoubleAnswersPastTheOldCeilingTheSameWayThisProbeDoes();
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return {directory.path().toStdString()};
        }

        [[nodiscard]] std::filesystem::path AddFolder(const std::string& relativePath) const
        {
            const std::filesystem::path folder = Root() / relativePath;
            std::filesystem::create_directories(folder);
            return folder;
        }

        [[nodiscard]] std::filesystem::path AddLiveJunction(const std::string& relativePath,
                                                            const std::filesystem::path& target) const
        {
            const std::filesystem::path linkPath = Root() / relativePath;
            std::filesystem::create_directories(linkPath.parent_path());

            WindowsLinkService linkService;
            [&]
            {
                QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);
            }();

            return linkPath;
        }

        [[nodiscard]] std::filesystem::path AddDanglingJunction(const std::string& relativePath) const
        {
            const std::filesystem::path target = AddFolder("doomed-target");
            const std::filesystem::path linkPath = Root() / relativePath;
            std::filesystem::create_directories(linkPath.parent_path());

            WindowsLinkService linkService;
            [&]
            {
                QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);
            }();
            std::filesystem::remove_all(target);

            return linkPath;
        }
    };
}

void WindowsFilesystemProbeTest::ADanglingJunctionIsStillAnEntryThatOccupiesItsPath()
{
    const Disk disk;
    const std::filesystem::path linkPath = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(!std::filesystem::exists(linkPath));
    QVERIFY(filesystemProbe.EntryExistsWithoutFollowingLinks(linkPath));
    QVERIFY(!filesystemProbe.TargetDirectoryExists(linkPath));
}

void WindowsFilesystemProbeTest::ChildDirectoriesReportsDanglingJunctionsToo()
{
    const Disk disk;
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;
    const std::vector<std::filesystem::path> children = filesystemProbe.ChildDirectories(destination);

    QCOMPARE(children.size(), std::size_t{2});
    QVERIFY(std::ranges::find(children, physical) != children.end());
    QVERIFY(std::ranges::find(children, dangling) != children.end());
}

void WindowsFilesystemProbeTest::AnUnmountedDriveLetterIsNotAnAvailableVolume()
{
    const DWORD mounted = GetLogicalDrives();
    char absentLetter = 0;
    for (char letter = 'Z'; letter >= 'D'; --letter)
    {
        if ((mounted & (1u << (letter - 'A'))) == 0)
        {
            absentLetter = letter;
            break;
        }
    }
    QVERIFY2(absentLetter != 0, "every drive letter from D to Z is mounted on this machine");

    const Disk disk;
    const WindowsFilesystemProbe filesystemProbe;

    const auto absent = std::filesystem::path(std::string(1, absentLetter) + ":/Portable Library/orbx-airport");

    QVERIFY(!filesystemProbe.VolumeIsAvailable(absent));
    QVERIFY(filesystemProbe.VolumeIsAvailable(disk.Root()));
}

void WindowsFilesystemProbeTest::AJunctionIsAReparsePointAndARealFolderIsNot()
{
    const Disk disk;
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.IsReparsePoint(dangling));
    QVERIFY(!filesystemProbe.IsReparsePoint(physical));
    QVERIFY(!filesystemProbe.IsReparsePoint(disk.Root() / "Community/never-created"));
}

void WindowsFilesystemProbeTest::OnlyARealFolderIsPhysicalAndALiveJunctionOverItIsNot()
{
    const Disk disk;
    const std::filesystem::path physical = disk.AddFolder("Library/Aircrafts/aerosoft-crj");
    const std::filesystem::path live = disk.AddLiveJunction("Addon Manager/aerosoft-crj", physical);
    const std::filesystem::path dangling = disk.AddDanglingJunction("Addon Manager/ag-airport-bgqq");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.PhysicalDirectoryExists(physical));
    QVERIFY2(!filesystemProbe.PhysicalDirectoryExists(live),
             "a junction left by the importer read as a folder the other program put back");
    QVERIFY(!filesystemProbe.PhysicalDirectoryExists(dangling));
    QVERIFY(!filesystemProbe.PhysicalDirectoryExists(disk.Root() / "Addon Manager/never-created"));

    QVERIFY2(filesystemProbe.TargetDirectoryExists(live),
             "the two probes stopped disagreeing, so one of them is not answering what it promises");
    QVERIFY(!filesystemProbe.TargetDirectoryExists(dangling));
}

void WindowsFilesystemProbeTest::FreeSpaceIsOnlyAnswerableForAFolderThatAlreadyExists()
{
    const Disk disk;
    const std::filesystem::path category = disk.AddFolder("Utils");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.FreeSpaceOn(category).has_value());
    QVERIFY(!filesystemProbe.FreeSpaceOn(category / "flybywire-externaltools-simbridge").has_value());
}

namespace
{
    std::vector<BYTE> SidOfWhoeverIsRunning()
    {
        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == 0)
        {
            return {};
        }

        DWORD size = 0;
        static_cast<void>(GetTokenInformation(token, TokenUser, nullptr, 0, &size));

        std::vector<BYTE> information(size);
        const BOOL read = GetTokenInformation(token, TokenUser, information.data(), size, &size);
        CloseHandle(token);

        if (read == 0)
        {
            return {};
        }

        const PSID inside = reinterpret_cast<TOKEN_USER*>(information.data())->User.Sid;
        std::vector<BYTE> sid(GetLengthSid(inside));

        if (CopySid(static_cast<DWORD>(sid.size()), sid.data(), inside) == 0)
        {
            return {};
        }

        return sid;
    }

    bool DenyAddingFilesTo(const std::filesystem::path& folder, std::vector<BYTE>& user)
    {
        if (user.empty())
        {
            return false;
        }

        EXPLICIT_ACCESS_W denial{};
        denial.grfAccessPermissions = FILE_ADD_FILE;
        denial.grfAccessMode = DENY_ACCESS;
        denial.grfInheritance = NO_INHERITANCE;
        denial.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        denial.Trustee.TrusteeType = TRUSTEE_IS_USER;
        denial.Trustee.ptstrName = static_cast<LPWSTR>(static_cast<void*>(user.data()));

        std::wstring native = folder.wstring();

        PACL existing = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (GetNamedSecurityInfoW(native.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                  &existing, nullptr, &descriptor)
            != ERROR_SUCCESS)
        {
            return false;
        }

        PACL updated = nullptr;
        const DWORD merged = SetEntriesInAclW(1, &denial, existing, &updated);
        const DWORD applied = merged == ERROR_SUCCESS
            ? SetNamedSecurityInfoW(native.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, updated,
                                    nullptr)
            : merged;

        LocalFree(updated);
        LocalFree(descriptor);

        return applied == ERROR_SUCCESS;
    }
}

void WindowsFilesystemProbeTest::AFolderThatIsNotThereIsNotTheSameAsOneThatRefusesTheWrite()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Addon Manager/MSFS");

    const WindowsFilesystemProbe filesystemProbe;

    QCOMPARE(filesystemProbe.ProbeWritable(folder), WriteAccess::ItAccepts);
    QCOMPARE(filesystemProbe.ProbeWritable(folder / "never-created"), WriteAccess::TheFolderIsNotThere);
    QVERIFY2(!std::filesystem::exists(folder / ".fsorg-write-probe"),
             "the probe file outlived the probe, so it will be walked as content later");
}

void WindowsFilesystemProbeTest::AFolderThatWillNotTakeAFileSaysPermissionIsWhatStoppedIt()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Addon Manager/MSFS");

    std::vector<BYTE> user = SidOfWhoeverIsRunning();
    if (!DenyAddingFilesTo(folder, user))
    {
        QFAIL("the deny entry could not be staged, so the mapping this test exists for was never exercised");
    }

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY2(filesystemProbe.TargetDirectoryExists(folder), "a denial of writes is not a folder that went away");
    QCOMPARE(filesystemProbe.ProbeWritable(folder), WriteAccess::PermissionIsDenied);
}

void WindowsFilesystemProbeTest::TheProbeCallsAFolderHeldExactlyWhenTheRenameIsRefused()
{
    const Disk disk;
    const std::filesystem::path addon = disk.AddFolder("Community/rkapps-fsrealistic");
    const std::filesystem::path service = disk.AddFolder("Community/rkapps-fsrealistic/service");
    const std::filesystem::path quarantine = disk.AddFolder("_fsorganizer-quarantine");
    const std::filesystem::path landing = quarantine / "rkapps-fsrealistic";

    const WindowsFilesystemProbe filesystemProbe;
    QVERIFY(!filesystemProbe.SomethingIsHoldingItOpen(addon));

    const HANDLE viewer =
        CreateFileW(service.wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    QVERIFY2(viewer != INVALID_HANDLE_VALUE, "the subfolder could not be held the way a folder view holds it");

    QVERIFY(filesystemProbe.SomethingIsHoldingItOpen(addon));
    QVERIFY2(MoveFileExW(addon.wstring().c_str(), landing.wstring().c_str(), 0) == FALSE,
             "a handle below it is what the probe is answering about, so the rename has to be refused");

    CloseHandle(viewer);

    QVERIFY(!filesystemProbe.SomethingIsHoldingItOpen(addon));
    QVERIFY(MoveFileExW(addon.wstring().c_str(), landing.wstring().c_str(), 0) != FALSE);
}

void WindowsFilesystemProbeTest::AFolderReportsWhenItWasLastWrittenTo()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddFolder("Utils/simbridge");

    const WindowsFilesystemProbe filesystemProbe;
    const std::optional<std::chrono::system_clock::time_point> written = filesystemProbe.LastWriteTime(folder);

    QVERIFY(written.has_value());
    QVERIFY(std::chrono::abs(std::chrono::system_clock::now() - *written) < std::chrono::minutes{5});
    QVERIFY(!filesystemProbe.LastWriteTime(folder / "never-existed").has_value());
}

void WindowsFilesystemProbeTest::TheStandardLibraryDoubleAnswersAJunctionTheSameWayThisProbeDoes()
{
    const Disk disk;
    const std::filesystem::path destination = disk.AddFolder("Community");
    const std::filesystem::path physical = disk.AddFolder("Community/asfs");
    const std::filesystem::path live = disk.AddFolder("live-target");
    const std::filesystem::path liveLink = destination / "ag-airport-live";

    WindowsLinkService linkService;
    QCOMPARE(linkService.CreateLink(liveLink, live, LinkType::Junction), LinkFailure::None);

    const std::filesystem::path dangling = disk.AddDanglingJunction("Community/ag-airport-bgqq");

    const WindowsFilesystemProbe production;
    const StdFilesystemProbe double_;

    for (const std::filesystem::path& entry : {physical, liveLink, dangling})
    {
        QCOMPARE(double_.IsReparsePoint(entry), production.IsReparsePoint(entry));
        QCOMPARE(double_.EntryExistsWithoutFollowingLinks(entry), production.EntryExistsWithoutFollowingLinks(entry));
    }

    std::vector<std::filesystem::path> byTheDouble = double_.ChildDirectories(destination);
    std::vector<std::filesystem::path> byProduction = production.ChildDirectories(destination);
    std::ranges::sort(byTheDouble);
    std::ranges::sort(byProduction);

    QCOMPARE(byProduction.size(), std::size_t{3});
    QCOMPARE(byTheDouble, byProduction);
}

void WindowsFilesystemProbeTest::EveryQuestionAboutAnEntryPastTheOldCeilingIsAnswerable()
{
    const Disk disk;
    const std::filesystem::path deep = FolderPastTheCeiling(disk.Root(), "tfdidesign-aircraft-md11");
    QVERIFY(deep.wstring().size() > kOldPathCeiling);
    WriteFilePastTheCeiling(deep / "manifest.json", R"({"title": "MD-11"})");

    const WindowsFilesystemProbe filesystemProbe;

    QVERIFY(filesystemProbe.EntryExistsWithoutFollowingLinks(deep));
    QVERIFY(filesystemProbe.TargetDirectoryExists(deep));
    QVERIFY(!filesystemProbe.IsReparsePoint(deep));
    QVERIFY(filesystemProbe.FreeSpaceOn(deep).has_value());
    QVERIFY(filesystemProbe.LastWriteTime(deep).has_value());
    QVERIFY(filesystemProbe.VolumeIsAvailable(deep));

    const std::optional<TreeFingerprint> fingerprint = filesystemProbe.FingerprintTree(deep);
    QVERIFY(fingerprint.has_value());
    QCOMPARE(fingerprint->files.size(), std::size_t{1});
    QCOMPARE(fingerprint->files.front().relativePath, std::filesystem::path("manifest.json"));
    QCOMPARE(fingerprint->longestEntry, (deep / "manifest.json").wstring().size());
}

void WindowsFilesystemProbeTest::ChildrenOfAFolderPastTheOldCeilingComeBackTheWayTheCallerNamesThem()
{
    const Disk disk;
    const std::filesystem::path deep = FolderPastTheCeiling(disk.Root(), "Utils");
    const std::filesystem::path staging = FolderPastTheCeiling(deep, "tfdidesign-aircraft-md11.fsorg-partial");

    const WindowsFilesystemProbe filesystemProbe;
    const std::vector<std::filesystem::path> children = filesystemProbe.ChildDirectories(deep);

    QCOMPARE(children.size(), std::size_t{1});
    QCOMPARE(children.front(), staging);
    QVERIFY2(!children.front().wstring().starts_with(LR"(\\?\)"),
             "a path that leaves the port carrying the prefix stops matching every path the domain built by hand");
}

void WindowsFilesystemProbeTest::TheStandardLibraryDoubleAnswersPastTheOldCeilingTheSameWayThisProbeDoes()
{
    const Disk disk;
    const std::filesystem::path deep = FolderPastTheCeiling(disk.Root(), "Utils");
    const std::filesystem::path addon = FolderPastTheCeiling(deep, "tfdidesign-aircraft-md11");
    QVERIFY(addon.wstring().size() > kOldPathCeiling);
    WriteFilePastTheCeiling(addon / "manifest.json", R"({"title": "MD-11"})");

    const WindowsFilesystemProbe production;
    const StdFilesystemProbe double_;

    QCOMPARE(double_.EntryExistsWithoutFollowingLinks(addon), production.EntryExistsWithoutFollowingLinks(addon));
    QCOMPARE(double_.TargetDirectoryExists(addon), production.TargetDirectoryExists(addon));
    QCOMPARE(double_.IsReparsePoint(addon), production.IsReparsePoint(addon));
    QCOMPARE(double_.LastWriteTime(addon).has_value(), production.LastWriteTime(addon).has_value());
    QCOMPARE(double_.ContentsOf(addon / "manifest.json").value_or(std::string{}),
             production.ContentsOf(addon / "manifest.json").value_or(std::string{}));
    QCOMPARE(double_.ChildDirectories(deep), production.ChildDirectories(deep));

    const std::optional<TreeFingerprint> byTheDouble = double_.FingerprintTree(addon);
    const std::optional<TreeFingerprint> byProduction = production.FingerprintTree(addon);
    QVERIFY(byProduction.has_value());
    QVERIFY(byTheDouble.has_value());
    QCOMPARE(byTheDouble->files.size(), byProduction->files.size());
    QCOMPARE(byTheDouble->files.front().relativePath, byProduction->files.front().relativePath);
    QCOMPARE(byTheDouble->longestEntry, byProduction->longestEntry);
}

QTEST_APPLESS_MAIN(WindowsFilesystemProbeTest)

#include "tst_windows_filesystem_probe.moc"
