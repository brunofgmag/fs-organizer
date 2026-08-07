#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>
#include <numeric>
#include <ranges>
#include <string>

#include "application/SizeService.h"
#include "domain/importing/ImportPaths.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/WindowsFilesystemProbe.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/InlineBackgroundRunner.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class SizeOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddonReachedThroughAJunctionIsMeasuredAsTheFolderItPointsAt();
        static void AJunctionInsideAnAddonIsNotFollowedSoNothingIsCountedTwice();
        static void TheLinkThatEnablesAnAddonWouldMeasureTheSameBytesAndIsNeverTheOneMeasured();
        static void TheDestinationsScreenWeighsTheTargetAndGetsWhatTheLibraryScreenGets();
        static void TheLooseWalkAndTheLibraryWalkFillTheSameCache();
    };
}

namespace
{
    const std::string kManifest = R"({"title": "Sim Rate Selector", "package_version": "1.2.3"})";

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

        static void WriteBytes(const std::filesystem::path& file, const std::size_t bytes)
        {
            std::ofstream stream(file, std::ios::binary);
            const std::string payload(bytes, 'x');
            stream.write(payload.data(), static_cast<std::streamsize>(bytes));
        }

        [[nodiscard]] std::filesystem::path AddAddon(const std::string& relativePath, const std::size_t bytes) const
        {
            const std::filesystem::path folder = AddFolder(relativePath);
            std::ofstream(ManifestPathIn(folder), std::ios::binary) << kManifest;
            WriteBytes(folder / "content.bin", bytes);

            return folder;
        }

        static void Junction(const std::filesystem::path& linkPath, const std::filesystem::path& target)
        {
            std::filesystem::create_directories(linkPath.parent_path());

            WindowsLinkService linkService;
            [&]
            {
                QCOMPARE(linkService.CreateLink(linkPath, target, LinkType::Junction), LinkFailure::None);
            }();
        }
    };

    struct Measuring
    {
        JsonManifestParser manifestParser;
        WindowsFilesystemProbe filesystemProbe;
        FilesystemScanner scanner{manifestParser, filesystemProbe};
        FakeClock clock;
        InlineBackgroundRunner runner;
        SizeService service{scanner, filesystemProbe, clock, runner};
        MeasurementCaller caller = service.NewCaller();

        [[nodiscard]] SizeReport Measure(const std::filesystem::path& library)
        {
            SizeReport measured;
            service.Measure(
                {library}, caller, Freshness::MeasureAgain,
                [](const SizeProgress&)
                {
                    return true;
                },
                [&measured](const SizeReport& report)
                {
                    measured = report;
                });

            return measured;
        }

        [[nodiscard]] FolderSizeReport Weigh(const std::vector<std::filesystem::path>& folders)
        {
            FolderSizeReport weighed;
            service.MeasureFolders(
                folders, caller, Freshness::ReuseWhatIsKnown,
                [](const SizeProgress&)
                {
                    return true;
                },
                [&weighed](const FolderSizeReport& report)
                {
                    weighed = report;
                });

            return weighed;
        }

        [[nodiscard]] std::uintmax_t WalkedBytesOf(const std::filesystem::path& folder) const
        {
            const std::optional<std::vector<FileFingerprint>> files = filesystemProbe.FingerprintTree(folder);
            if (!files.has_value())
            {
                return 0;
            }

            const auto sizes = *files | std::views::transform(&FileFingerprint::size);

            return std::accumulate(sizes.begin(), sizes.end(), std::uintmax_t{0});
        }
    };

    [[nodiscard]] const MeasuredNode* OnlyAddonOf(const SizeReport& report)
    {
        if (report.libraries.size() != 1 || report.libraries.front().children.size() != 1)
        {
            return nullptr;
        }

        const MeasuredNode& category = report.libraries.front().children.front();

        return category.children.size() == 1 ? &category.children.front() : nullptr;
    }
}

void SizeOnRealDiskTest::AnAddonReachedThroughAJunctionIsMeasuredAsTheFolderItPointsAt()
{
    const Disk disk;
    const std::filesystem::path elsewhere = disk.AddAddon("elsewhere/tfdidesign-aircraft-md11", 4096);
    const std::filesystem::path library = disk.AddFolder("Library");
    Disk::Junction(library / "Utils" / "tfdidesign-aircraft-md11", elsewhere);

    Measuring measuring;
    const SizeReport report = measuring.Measure(library);

    const MeasuredNode* addon = OnlyAddonOf(report);
    QVERIFY(addon != nullptr);
    QCOMPARE(addon->kind, TreeNodeKind::Addon);
    QVERIFY(addon->measured);
    QCOMPARE(addon->bytes, std::uintmax_t{4096} + kManifest.size());
    QCOMPARE(report.libraries.front().bytes, std::uintmax_t{4096} + kManifest.size());
}

void SizeOnRealDiskTest::AJunctionInsideAnAddonIsNotFollowedSoNothingIsCountedTwice()
{
    const Disk disk;
    const std::filesystem::path fat = disk.AddFolder("elsewhere/fat");
    Disk::WriteBytes(fat / "huge.bin", 90'000);

    const std::filesystem::path library = disk.AddFolder("Library");
    const std::filesystem::path addon = disk.AddAddon("Library/Utils/sim-rate-selector", 1000);
    Disk::Junction(addon / "linked-in", fat);

    Measuring measuring;
    const SizeReport report = measuring.Measure(library);

    const MeasuredNode* measured = OnlyAddonOf(report);
    QVERIFY(measured != nullptr);
    QCOMPARE(measured->bytes, std::uintmax_t{1000} + kManifest.size());
    QVERIFY(measured->bytes < 90'000);
}

void SizeOnRealDiskTest::TheLinkThatEnablesAnAddonWouldMeasureTheSameBytesAndIsNeverTheOneMeasured()
{
    const Disk disk;
    const std::filesystem::path library = disk.AddFolder("Library");
    const std::filesystem::path addon = disk.AddAddon("Library/Utils/sim-rate-selector", 2048);
    const std::filesystem::path enabled = disk.Root() / "Community" / "sim-rate-selector";
    Disk::Junction(enabled, addon);

    Measuring measuring;
    const SizeReport report = measuring.Measure(library);

    const MeasuredNode* measured = OnlyAddonOf(report);
    QVERIFY(measured != nullptr);
    QCOMPARE(measured->path, addon);
    QCOMPARE(measured->bytes, std::uintmax_t{2048} + kManifest.size());

    QCOMPARE(measuring.WalkedBytesOf(enabled), std::uintmax_t{2048} + kManifest.size());
    QCOMPARE(measuring.service.BytesOf(enabled), std::optional<std::uintmax_t>{});
}

void SizeOnRealDiskTest::TheDestinationsScreenWeighsTheTargetAndGetsWhatTheLibraryScreenGets()
{
    const Disk disk;
    const std::filesystem::path library = disk.AddFolder("Library");
    const std::filesystem::path addon = disk.AddAddon("Library/Utils/sim-rate-selector", 2048);
    const std::filesystem::path enabled = disk.Root() / "Community" / "sim-rate-selector";
    Disk::Junction(enabled, addon);

    Measuring measuring;

    const FolderSizeReport fromTheDestinations = measuring.Weigh({addon});
    const FolderSizeReport fromTheLibrary = measuring.Weigh({addon});

    QCOMPARE(fromTheDestinations.bytes, std::uintmax_t{2048} + kManifest.size());
    QCOMPARE(fromTheDestinations.bytes, fromTheLibrary.bytes);
    QCOMPARE(measuring.WalkedBytesOf(enabled), fromTheDestinations.bytes);
    QCOMPARE(measuring.service.BytesOf(enabled), std::optional<std::uintmax_t>{});
}

void SizeOnRealDiskTest::TheLooseWalkAndTheLibraryWalkFillTheSameCache()
{
    const Disk disk;
    const std::filesystem::path library = disk.AddFolder("Library");
    const std::filesystem::path addon = disk.AddAddon("Library/Utils/sim-rate-selector", 2048);

    Measuring measuring;
    static_cast<void>(measuring.Measure(library));

    Disk::WriteBytes(addon / "grown.bin", 5000);

    const FolderSizeReport weighed = measuring.Weigh({addon});

    QCOMPARE(weighed.bytes, std::uintmax_t{2048} + kManifest.size());
    QCOMPARE(weighed.measured, std::size_t{1});
}

QTEST_APPLESS_MAIN(SizeOnRealDiskTest)

#include "tst_size_on_real_disk.moc"
