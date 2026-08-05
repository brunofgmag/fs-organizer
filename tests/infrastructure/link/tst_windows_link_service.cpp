#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <windows.h>
#include <winioctl.h>

#include <fstream>

#include "domain/support/PathUtils.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class WindowsLinkServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AJunctionReadsBackTheTargetItWasCreatedWith();
        static void RemovingTheNodeSparesTheTargetAndEverythingInside();
        static void ASymlinkEitherLandsOrSaysThePrivilegeIsMissing();
        static void AJunctionWhoseLinkPathPassesTheOldCeilingStillLandsAndReadsBack();
        static void TheTargetWrittenInsideTheJunctionNeverCarriesTheExtendedPrefix();
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

        [[nodiscard]] std::filesystem::path AddAddonFolder(const std::string& name) const
        {
            const std::filesystem::path folder = Root() / "library" / name;
            std::filesystem::create_directories(folder);

            std::ofstream file(folder / "manifest.json", std::ios::binary);
            file << R"({"title": "Kept"})";

            return folder;
        }

        [[nodiscard]] std::filesystem::path Destination(const std::string& name) const
        {
            const std::filesystem::path destination = Root() / "Community";
            std::filesystem::create_directories(destination);
            return destination / name;
        }
    };
}

void WindowsLinkServiceTest::AJunctionReadsBackTheTargetItWasCreatedWith()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("pmdg-aircraft-77w");
    const std::filesystem::path linkPath = disk.Destination("pmdg-aircraft-77w");

    WindowsLinkService linkService;

    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);

    const std::optional<std::filesystem::path> readBack = linkService.ReadLinkTarget(linkPath);
    QVERIFY(readBack.has_value());
    QVERIFY(!readBack->string().starts_with(R"(\??\)"));
    QCOMPARE(ComparablePath(*readBack), ComparablePath(target));
}

void WindowsLinkServiceTest::RemovingTheNodeSparesTheTargetAndEverythingInside()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("fsdreamteam-gsx-pro");
    const std::filesystem::path linkPath = disk.Destination("fsdreamteam-gsx-pro");

    WindowsLinkService linkService;
    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);
    QVERIFY(std::filesystem::exists(linkPath / "manifest.json"));

    QVERIFY(linkService.RemoveReparseNode(linkPath));

    QVERIFY(!std::filesystem::exists(linkPath));
    QVERIFY(std::filesystem::is_directory(target));
    QVERIFY(std::filesystem::exists(target / "manifest.json"));
}

void WindowsLinkServiceTest::ASymlinkEitherLandsOrSaysThePrivilegeIsMissing()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("aerosoft-crj");
    const std::filesystem::path linkPath = disk.Destination("aerosoft-crj");

    WindowsLinkService linkService;
    const LinkFailure outcome = linkService.CreateLink(linkPath, target, LinkType::Symbolic);

    QVERIFY2(outcome == LinkFailure::None || outcome == LinkFailure::PrivilegeNotHeld,
             qPrintable(QStringLiteral("creating the symlink returned %1, which does not tell the user what to do")
                            .arg(QTest::toString(outcome))));

    if (outcome != LinkFailure::None)
    {
        QVERIFY(!std::filesystem::exists(linkPath));
        return;
    }

    const std::optional<std::filesystem::path> readBack = linkService.ReadLinkTarget(linkPath);
    QVERIFY(readBack.has_value());
    QCOMPARE(ComparablePath(*readBack), ComparablePath(target));
}

namespace
{
    [[nodiscard]] std::wstring ReparseBufferOf(const std::filesystem::path& linkPath)
    {
        const HANDLE handle = CreateFileW(
            linkPath.wstring().c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return {};
        }

        std::vector<char> raw(MAXIMUM_REPARSE_DATA_BUFFER_SIZE, 0);
        DWORD returned = 0;
        const BOOL queried = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, raw.data(),
                                             static_cast<DWORD>(raw.size()), &returned, nullptr);
        CloseHandle(handle);

        if (queried == FALSE)
        {
            return {};
        }

        return {reinterpret_cast<const wchar_t*>(raw.data()), returned / sizeof(wchar_t)};
    }
}

void WindowsLinkServiceTest::AJunctionWhoseLinkPathPassesTheOldCeilingStillLandsAndReadsBack()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("tfdidesign-aircraft-md11");
    const std::filesystem::path destination = FolderPastTheCeiling(disk.Root(), "Community");
    const std::filesystem::path linkPath = destination / "tfdidesign-aircraft-md11";
    QVERIFY(linkPath.wstring().size() > kOldPathCeiling);

    WindowsLinkService linkService;
    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);

    const std::optional<std::filesystem::path> readBack = linkService.ReadLinkTarget(linkPath);
    QVERIFY(readBack.has_value());
    QCOMPARE(ComparablePath(*readBack), ComparablePath(target));

    QVERIFY(linkService.RemoveReparseNode(linkPath));
    QVERIFY(!ExistsPastTheCeiling(linkPath));
    QVERIFY(std::filesystem::exists(target / "manifest.json"));
}

void WindowsLinkServiceTest::TheTargetWrittenInsideTheJunctionNeverCarriesTheExtendedPrefix()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("pmdg-aircraft-77w");
    const std::filesystem::path linkPath = disk.Destination("pmdg-aircraft-77w");

    WindowsLinkService linkService;
    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);

    std::filesystem::path expected = target;
    expected.make_preferred();

    const std::wstring buffer = ReparseBufferOf(linkPath);
    QVERIFY(!buffer.empty());
    QVERIFY2(buffer.find(LR"(\??\)" + expected.wstring()) != std::wstring::npos,
             "the substitute name is no longer the object namespace prefix followed by the raw target");
    QVERIFY2(buffer.find(LR"(\\?\)") == std::wstring::npos,
             "the extended prefix reached the reparse buffer, which is how a junction ends up corrupted");
}

QTEST_APPLESS_MAIN(WindowsLinkServiceTest)

#include "tst_windows_link_service.moc"
