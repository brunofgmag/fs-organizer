#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>

#include "infrastructure/link/PosixLinkService.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PosixLinkServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ALinkReadsBackTheTargetItWasCreatedWith();
        static void JunctionAndSymbolicBothMakeASymlinkBecauseLinuxHasNoOther();
        static void RemovingTheLinkSparesTheTargetAndEverythingInside();
        static void ALinkWhoseTargetIsGoneStillReadsBackAndStillRemoves();
        static void AFolderThatIsNotALinkIsNeitherReadNorRemoved();
        static void AFileThatIsNotALinkIsNeitherReadNorRemoved();
        static void AnExistingEntryIsNotReplacedByTheLink();
        static void ALinkWhoseFolderCannotBeWrittenSaysSoInsteadOfPretendingSuccess();
        static void ReadingManyLinksAnswersEachInItsPlace();
    };
}

namespace
{
    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
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

void PosixLinkServiceTest::ALinkReadsBackTheTargetItWasCreatedWith()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("pmdg-aircraft-77w");
    const std::filesystem::path linkPath = disk.Destination("pmdg-aircraft-77w");

    PosixLinkService linkService;

    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Symbolic), LinkFailure::None);
    QVERIFY(std::filesystem::is_symlink(linkPath));
    QVERIFY(std::filesystem::exists(linkPath / "manifest.json"));

    const std::optional<std::filesystem::path> readBack = linkService.ReadLinkTarget(linkPath);
    QVERIFY(readBack.has_value());
    QCOMPARE(*readBack, target);
}

void PosixLinkServiceTest::JunctionAndSymbolicBothMakeASymlinkBecauseLinuxHasNoOther()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("fenix-a320");

    PosixLinkService linkService;

    for (const LinkType type : {LinkType::Junction, LinkType::Symbolic})
    {
        const std::filesystem::path linkPath = disk.Destination(type == LinkType::Junction ? "junction" : "symbolic");

        QCOMPARE(linkService.CreateLink(linkPath, target, type), LinkFailure::None);
        QVERIFY(std::filesystem::is_symlink(linkPath));
    }
}

void PosixLinkServiceTest::RemovingTheLinkSparesTheTargetAndEverythingInside()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("aerosoft-crj");
    const std::filesystem::path linkPath = disk.Destination("aerosoft-crj");

    PosixLinkService linkService;
    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Symbolic), LinkFailure::None);

    QVERIFY(linkService.RemoveReparseNode(linkPath));

    QVERIFY(!std::filesystem::exists(std::filesystem::symlink_status(linkPath)));
    QVERIFY(std::filesystem::is_regular_file(target / "manifest.json"));
}

void PosixLinkServiceTest::ALinkWhoseTargetIsGoneStillReadsBackAndStillRemoves()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("hype-atr");
    const std::filesystem::path linkPath = disk.Destination("hype-atr");

    PosixLinkService linkService;
    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Symbolic), LinkFailure::None);
    std::filesystem::remove_all(target);

    const std::optional<std::filesystem::path> readBack = linkService.ReadLinkTarget(linkPath);
    QVERIFY(readBack.has_value());
    QCOMPARE(*readBack, target);
    QVERIFY(linkService.RemoveReparseNode(linkPath));
    QVERIFY(!std::filesystem::exists(std::filesystem::symlink_status(linkPath)));
}

void PosixLinkServiceTest::AFolderThatIsNotALinkIsNeitherReadNorRemoved()
{
    const Disk disk;
    const std::filesystem::path folder = disk.AddAddonFolder("real-folder");

    PosixLinkService linkService;

    QVERIFY(!linkService.ReadLinkTarget(folder).has_value());
    QVERIFY(!linkService.RemoveReparseNode(folder));
    QVERIFY(std::filesystem::is_regular_file(folder / "manifest.json"));
}

void PosixLinkServiceTest::AFileThatIsNotALinkIsNeitherReadNorRemoved()
{
    const Disk disk;
    const std::filesystem::path file = disk.AddAddonFolder("holder") / "manifest.json";

    PosixLinkService linkService;

    QVERIFY(!linkService.ReadLinkTarget(file).has_value());
    QVERIFY(!linkService.RemoveReparseNode(file));
    QVERIFY(std::filesystem::is_regular_file(file));
}

void PosixLinkServiceTest::AnExistingEntryIsNotReplacedByTheLink()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("gsx-pro");
    const std::filesystem::path occupied = disk.AddAddonFolder("occupied");

    PosixLinkService linkService;

    QCOMPARE(linkService.CreateLink(occupied, target, LinkType::Symbolic), LinkFailure::CouldNotCreateLink);
    QVERIFY(!std::filesystem::is_symlink(occupied));
    QVERIFY(std::filesystem::is_regular_file(occupied / "manifest.json"));
}

void PosixLinkServiceTest::ALinkWhoseFolderCannotBeWrittenSaysSoInsteadOfPretendingSuccess()
{
    const Disk disk;
    const std::filesystem::path target = disk.AddAddonFolder("navigraph");
    const std::filesystem::path linkPath = disk.Root() / "not-there" / "navigraph";

    PosixLinkService linkService;

    QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Symbolic), LinkFailure::CouldNotCreateLink);
    QVERIFY(!std::filesystem::exists(std::filesystem::symlink_status(linkPath)));
}

void PosixLinkServiceTest::ReadingManyLinksAnswersEachInItsPlace()
{
    const Disk disk;
    const std::filesystem::path first = disk.AddAddonFolder("first");
    const std::filesystem::path second = disk.AddAddonFolder("second");

    PosixLinkService linkService;
    QCOMPARE(linkService.CreateLink(disk.Destination("first"), first, LinkType::Symbolic), LinkFailure::None);
    QCOMPARE(linkService.CreateLink(disk.Destination("second"), second, LinkType::Symbolic), LinkFailure::None);

    const std::vector<std::optional<std::filesystem::path>> read = linkService.ReadLinkTargets(
        {disk.Destination("second"), disk.Destination("nothing"), disk.Destination("first")});

    QCOMPARE(read.size(), std::size_t{3});
    QCOMPARE(read[0], std::optional(second));
    QVERIFY(!read[1].has_value());
    QCOMPARE(read[2], std::optional(first));
}

QTEST_APPLESS_MAIN(PosixLinkServiceTest)

#include "tst_posix_link_service.moc"
