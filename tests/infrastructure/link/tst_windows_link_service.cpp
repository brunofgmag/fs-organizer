#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>

#include "domain/support/PathUtils.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/support/PathPrinting.h"

class WindowsLinkServiceTest : public QObject
{
    Q_OBJECT

private slots:
    static void AJunctionReadsBackTheTargetItWasCreatedWith();
    static void RemovingTheNodeSparesTheTargetAndEverythingInside();
};

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

    QVERIFY(linkService.CreateLink(linkPath, target, LinkType::Junction));

    const std::optional<std::filesystem::path> readBack = linkService.ReadLinkTarget(linkPath);
    QVERIFY(readBack.has_value());
    QVERIFY(readBack->string().starts_with(R"(\??\)"));
    QCOMPARE(ComparablePath(NormalizeReparseTarget(*readBack)), ComparablePath(target));
}

void WindowsLinkServiceTest::RemovingTheNodeSparesTheTargetAndEverythingInside()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("fsdreamteam-gsx-pro");
    const std::filesystem::path linkPath = disk.Destination("fsdreamteam-gsx-pro");

    WindowsLinkService linkService;
    QVERIFY(linkService.CreateLink(linkPath, target, LinkType::Junction));
    QVERIFY(std::filesystem::exists(linkPath / "manifest.json"));

    QVERIFY(linkService.RemoveReparseNode(linkPath));

    QVERIFY(!std::filesystem::exists(linkPath));
    QVERIFY(std::filesystem::is_directory(target));
    QVERIFY(std::filesystem::exists(target / "manifest.json"));
}

QTEST_APPLESS_MAIN(WindowsLinkServiceTest)

#include "tst_windows_link_service.moc"
