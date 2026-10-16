#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <utility>

#include "application/ArchiveInstallService.h"
#include "application/ImportService.h"
#include "domain/importing/ImportEngine.h"
#include "domain/importing/ImportPaths.h"
#include "domain/journal/OperationLog.h"
#include "domain/ports/ImportedFolders.h"
#include "domain/tree/LibraryTrees.h"
#include "infrastructure/archive/ArchiveOrFolderReader.h"
#include "infrastructure/archive/LibarchiveArchiveReader.h"
#include "infrastructure/archive/PosixFolderArchiveReader.h"
#include "infrastructure/catalog/FilesystemScanner.h"
#include "infrastructure/catalog/JsonManifestParser.h"
#include "infrastructure/fileops/PosixFileOperations.h"
#include "infrastructure/fileops/PosixFilesystemProbe.h"
#include "infrastructure/fileops/PosixSidecarStore.h"
#include "infrastructure/journal/JsonlOperationJournal.h"
#include "infrastructure/link/PosixLinkService.h"
#include "infrastructure/platform/SystemClock.h"
#include "infrastructure/sim/ProcProcessProbe.h"
#include "tests/support/ArchiveBuilders.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PosixArchiveInstallOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ANewPackageLandsWithTheSameRelativePathsAndSizesAndNoStagingIsLeft();
        static void ANewerCopyReplacesTheOldOneThroughTheQuarantineAndTheLinkFollowsIt();
        static void AThirdCopyNeedsTheDiscardFlagWhileTheQuarantineHoldsTheSecond();
        static void AKilledInstallLeavesADiscardOnlyStagingThatIsNotAnAddonAndTheNextInstallRemovesIt();
        static void ReadOnlyFilesLeftInTheStagingDoNotKeepItAfterAnInstall();
        static void ReadOnlyFilesDoNotKeepTheStagingAfterAFailedVerification();
        static void TheSameFolderWrittenInTwoCasesIsInstalledFromARealZip();
        static void ASecondLooseArchiveWithTheSameFolderDoesNotReplaceTheFirst();
        static void ADroppedFolderInstallsItsPackageAndStaysExactlyAsItWas();
        static void ADroppedFolderHoldingTwoPackagesInstallsTheOneChosen();
        static void ADroppedFolderWithASymlinkInsideIsRefusedBeforeAnyWrite();
        static void ADroppedFolderThatIsTheInstalledPackageIsRefusedAndTheInstalledOneStaysIntact();
        static void ADroppedLibraryIsRefusedBeforeAnyWrite();
        static void ACancelledCopyOfADroppedFolderLeavesNeitherStagingNorPackage();
    };
}

namespace
{
    const NothingWasImported nothingWasImported;

    [[nodiscard]] std::string Utf8Of(const std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    const std::string kPackage = Utf8Of(u8"[RR WL] pacote ação");

    void WriteFile(const std::filesystem::path& file, const std::string& content)
    {
        std::filesystem::create_directories(file.parent_path());

        std::ofstream stream(file, std::ios::binary);
        stream << content;
    }

    [[nodiscard]] std::string ContentsOfFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] QStringList TreeOf(const std::filesystem::path& root)
    {
        QStringList lines;

        for (const std::filesystem::directory_entry& item : std::filesystem::recursive_directory_iterator(root))
        {
            const QString name = AsText(item.path().lexically_relative(root));
            lines << (item.is_directory() ? name + QStringLiteral("/")
                                          : QStringLiteral("%1 | %2").arg(name).arg(item.file_size()));
        }

        lines.sort();

        return lines;
    }

    [[nodiscard]] QString Shown(const std::string& text)
    {
        return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }

    [[nodiscard]] std::string StoredZipOf(const std::vector<std::pair<std::string, std::string>>& members)
    {
        std::vector<ArchiveItem> items;
        for (const auto& [name, content] : members)
        {
            items.push_back(ArchiveItem{.name = name, .content = content});
        }

        return ZipOf(items);
    }

    void MakeEverythingWritable(const std::filesystem::path& root)
    {
        for (const std::filesystem::directory_entry& item : std::filesystem::recursive_directory_iterator(root))
        {
            if (item.is_regular_file())
            {
                std::filesystem::permissions(item.path(), std::filesystem::perms::owner_write,
                                             std::filesystem::perm_options::add);
            }
        }
    }

    class ReadOnlyOnceExtracted final : public ArchiveReader
    {
    public:
        ReadOnlyOnceExtracted(const ArchiveReader& inner, std::filesystem::path alsoLeftOut)
            : inner_(inner), alsoLeftOut_(std::move(alsoLeftOut))
        {
        }

        [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path& archive) const override
        {
            return inner_.EntriesOf(archive);
        }

        [[nodiscard]] ArchiveText ContentsOf(const std::filesystem::path& archive,
                                             const std::string& entry,
                                             const std::size_t most) const override
        {
            return inner_.ContentsOf(archive, entry, most);
        }

        [[nodiscard]] ArchiveFailure
        ExtractAll(const std::filesystem::path& archive,
                   const std::vector<ArchiveEntry>& listed,
                   const std::filesystem::path& into,
                   const std::function<bool(const CopyProgress&)>& onProgress) const override
        {
            const ArchiveFailure failure = inner_.ExtractAll(archive, listed, into, onProgress);

            if (!alsoLeftOut_.empty())
            {
                std::filesystem::remove(into / alsoLeftOut_);
            }

            for (const std::filesystem::directory_entry& item : std::filesystem::recursive_directory_iterator(into))
            {
                if (item.is_regular_file())
                {
                    std::filesystem::permissions(item.path(),
                                                 std::filesystem::perms::owner_write
                                                     | std::filesystem::perms::group_write
                                                     | std::filesystem::perms::others_write,
                                                 std::filesystem::perm_options::remove);
                }
            }

            return failure;
        }

    private:
        const ArchiveReader& inner_;
        std::filesystem::path alsoLeftOut_;
    };

    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
        }

        [[nodiscard]] std::filesystem::path Destination() const
        {
            return Root() / "Sim" / "Community";
        }

        [[nodiscard]] std::filesystem::path LibraryRoot() const
        {
            return Root() / "Library";
        }

        [[nodiscard]] std::filesystem::path Category() const
        {
            return LibraryRoot() / "Utils";
        }

        [[nodiscard]] std::filesystem::path Package() const
        {
            return Category() / PathFromUtf8(kPackage);
        }

        [[nodiscard]] SimulatorProfile Profile() const
        {
            SimulatorProfile profile;
            profile.destinations = {Destination()};
            profile.defaultDestination = Destination();
            profile.libraries = {Library{.id = "lib-1", .path = LibraryRoot(), .label = "Biblioteca"}};

            return profile;
        }

        [[nodiscard]] std::filesystem::path Source(const std::string& version) const
        {
            return Root() / "source" / PathFromUtf8("v" + version) / PathFromUtf8(kPackage);
        }

        [[nodiscard]] std::filesystem::path Archive(const std::string& version) const
        {
            return Root() / "downloads" / PathFromUtf8("pacote-" + version + ".zip");
        }

        [[nodiscard]] bool
        MakeTheArchive(const std::string& version, const std::size_t sceneryBytes, const bool withAReadme = false) const
        {
            const std::filesystem::path source = Source(version);
            WriteFile(source / "manifest.json", R"({"title": "Pacote", "package_version": ")" + version + R"("})");
            WriteFile(source / "layout.json", R"({"content": []})");
            WriteFile(source / "scenery" / "world" / "a.bgl", std::string(sceneryBytes, 'b'));
            std::filesystem::create_directories(Archive(version).parent_path());

            std::vector<ArchiveItem> items{ArchiveItem{.name = kPackage, .folder = true}};
            for (const std::filesystem::directory_entry& item : std::filesystem::recursive_directory_iterator(source))
            {
                const std::string name = kPackage + "/" + AsUtf8(item.path().lexically_relative(source));
                items.push_back(item.is_directory()
                                    ? ArchiveItem{.name = name, .folder = true}
                                    : ArchiveItem{.name = name, .content = ContentsOfFile(item.path())});
            }

            if (withAReadme)
            {
                WriteFile(source.parent_path() / "readme.txt", "read me first");
                items.push_back(ArchiveItem{.name = "readme.txt", .content = "read me first"});
            }

            WriteFile(Archive(version), ZipOf(items));

            return std::filesystem::exists(Archive(version));
        }
    };

    struct Adapters
    {
        explicit Adapters(std::filesystem::path journalPath) : journalFile(std::move(journalPath))
        {
        }

        std::filesystem::path journalFile;
        PosixLinkService linkService{};
        PosixFilesystemProbe filesystemProbe{};
        PosixFileOperations files{};
        PosixSidecarStore sidecars{};
        LinkingEngine linking{linkService, filesystemProbe};
        SystemClock clock{};
        JsonlOperationJournal journal{journalFile};
        OperationLog log{journal, clock};
        ImportEngine engine{filesystemProbe,          files, sidecars, linking, log, LinkType::Symbolic,
                            Verification::ByStructure};
        ProcProcessProbe processProbe{std::vector<std::string>{}};
        JsonManifestParser manifestParser{};
        FilesystemScanner catalog{manifestParser, filesystemProbe, nothingWasImported};
        ImportService importing{engine,  processProbe, filesystemProbe,   catalog, files, sidecars,
                                linking, log,          LinkType::Symbolic};
        EntryClassifier classifier{linkService, filesystemProbe};
        LibarchiveArchiveReader packedArchives{};
        PosixFolderArchiveReader folderArchives{};
        ArchiveOrFolderReader archives{folderArchives, packedArchives};
        ArchiveInstallService installing{archives, manifestParser, filesystemProbe, files, log, importing};

        [[nodiscard]] std::vector<DestinationEntry> Entries(const Disk& disk) const
        {
            return classifier.Resolve(disk.Profile().destinations, {disk.LibraryRoot()});
        }

        [[nodiscard]] WhatInstallingDid Install(const Disk& disk,
                                                const std::string& version,
                                                const bool discard = false,
                                                const std::filesystem::path& replaces = {}) const
        {
            return InstallThrough(installing, disk, version, discard, replaces);
        }

        [[nodiscard]] WhatInstallingDid InstallThrough(const ArchiveInstallService& service,
                                                       const Disk& disk,
                                                       const std::string& version,
                                                       const bool discard = false,
                                                       const std::filesystem::path& replaces = {}) const
        {
            return service.Install(
                disk.Profile(), Entries(disk),
                ArchiveInstallRequest{.archive = disk.Archive(version),
                                      .category = disk.Category(),
                                      .packages = {ChosenPackage{.root = kPackage,
                                                                 .discardTheEarlierQuarantinedCopy = discard,
                                                                 .replaces = replaces}}});
        }
    };

    [[nodiscard]] std::filesystem::path QuarantinedCopy(const Disk& disk)
    {
        return QuarantineFolderInside(disk.LibraryRoot()) / PathFromUtf8(kPackage);
    }

    [[nodiscard]] QStringList SnapshotOf(const std::filesystem::path& root)
    {
        QStringList lines;

        for (const std::filesystem::directory_entry& item : std::filesystem::recursive_directory_iterator(root))
        {
            const QString name = AsText(item.path().lexically_relative(root));

            if (item.is_directory())
            {
                lines << name + QStringLiteral("/");
                continue;
            }

            lines << QStringLiteral("%1 | %2 | %3 | %4")
                         .arg(name)
                         .arg(item.file_size())
                         .arg(Shown(ContentsOfFile(item.path())))
                         .arg(item.last_write_time().time_since_epoch().count());
        }

        lines.sort();

        return lines;
    }

    [[nodiscard]] std::filesystem::path TheDroppedFolder(const Disk& disk)
    {
        return disk.Root() / "dropped" / PathFromUtf8(kPackage);
    }

    void MakeTheDroppedFolder(const Disk& disk, const std::string& version, const std::size_t sceneryBytes)
    {
        const std::filesystem::path folder = TheDroppedFolder(disk);

        WriteFile(folder / "manifest.json", R"({"title": "Pacote", "package_version": ")" + version + R"("})");
        WriteFile(folder / "layout.json", R"({"content": []})");
        WriteFile(folder / "scenery" / "world" / "a.bgl", std::string(sceneryBytes, 'b'));
        std::filesystem::create_directories(folder / "empty");
    }

    [[nodiscard]] ArchiveInstallRequest InstallingFrom(const Disk& disk, const std::filesystem::path& source)
    {
        return ArchiveInstallRequest{
            .archive = source, .category = disk.Category(), .packages = {ChosenPackage{.root = kPackage}}};
    }

    void CollectPathsBelow(const TreeNode& node, std::vector<std::filesystem::path>& into)
    {
        into.push_back(node.path);

        for (const TreeNode& child : node.children)
        {
            CollectPathsBelow(child, into);
        }
    }

    void TheFirstCopyIsInstalledAndLinked(const Disk& disk, const Adapters& adapters)
    {
        QVERIFY(disk.MakeTheArchive("1.0.0", 2048));
        QVERIFY(std::filesystem::create_directories(disk.Category()));
        QVERIFY(std::filesystem::create_directories(disk.Destination()));

        const WhatInstallingDid first = adapters.Install(disk, "1.0.0");

        QCOMPARE(first.stopped, FileResult::Completed);
        QCOMPARE(first.packages.size(), std::size_t{1});
        QCOMPARE(first.packages.front().result, FileResult::Completed);

        QVERIFY(adapters.linking
                    .Enable(Addon{.folderPath = disk.Package(), .manifest = Manifest{}}, disk.Destination(),
                            LinkType::Symbolic)
                    .Succeeded());
    }
}

void PosixArchiveInstallOnRealDiskTest::ANewPackageLandsWithTheSameRelativePathsAndSizesAndNoStagingIsLeft()
{
    const Disk disk;
    QVERIFY(disk.MakeTheArchive("1.0.0", 2048));
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const std::uintmax_t archiveBytes = std::filesystem::file_size(disk.Archive("1.0.0"));

    const WhatIsInTheArchive found =
        adapters.installing.Inspect(disk.Profile(), disk.Archive("1.0.0"), disk.Category());

    QCOMPARE(found.failure, ArchiveFailure::None);
    QCOMPARE(found.refusal, NothingToInstall::None);
    QCOMPARE(found.room, FileResult::Completed);
    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(Shown(FolderNameOf(found.packages.front().package).Text()), Shown(kPackage));
    QCOMPARE(found.packages.front().versionInTheArchive, std::string{"1.0.0"});
    QCOMPARE(found.packages.front().admission.result, FileResult::Completed);

    const WhatInstallingDid did = adapters.Install(disk, "1.0.0");

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QCOMPARE(did.packages.front().landed, disk.Package());
    QVERIFY(!did.packages.front().replaced);
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("1.0.0")));
    QVERIFY(std::filesystem::exists(disk.Package() / "scenery" / "world" / "a.bgl"));
    QCOMPARE(std::filesystem::file_size(disk.Package() / "scenery" / "world" / "a.bgl"), std::uintmax_t{2048});
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QCOMPARE(std::filesystem::file_size(disk.Archive("1.0.0")), archiveBytes);
}

void PosixArchiveInstallOnRealDiskTest::ANewerCopyReplacesTheOldOneThroughTheQuarantineAndTheLinkFollowsIt()
{
    const Disk disk;
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    TheFirstCopyIsInstalledAndLinked(disk, adapters);
    QVERIFY(!QTest::currentTestFailed());
    QVERIFY(disk.MakeTheArchive("2.0.0", 3000));

    const WhatIsInTheArchive found =
        adapters.installing.Inspect(disk.Profile(), disk.Archive("2.0.0"), disk.Category());

    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(found.packages.front().versionInTheArchive, std::string{"2.0.0"});
    QCOMPARE(found.packages.front().admission.result, FileResult::Completed);
    QCOMPARE(found.packages.front().admission.occupantVersion, std::string{"1.0.0"});
    QCOMPARE(found.packages.front().admission.occupant, disk.Package());

    const WhatInstallingDid did = adapters.Install(disk, "2.0.0", false, found.packages.front().admission.occupant);

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(did.packages.front().replaced);
    QCOMPARE(did.packages.front().landed, disk.Package());
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("2.0.0")));
    QVERIFY(ContentsOfFile(QuarantinedCopy(disk) / "manifest.json").find("1.0.0") != std::string::npos);
    QCOMPARE(std::filesystem::file_size(QuarantinedCopy(disk) / "scenery" / "world" / "a.bgl"), std::uintmax_t{2048});
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));

    const std::filesystem::path link = disk.Destination() / PathFromUtf8(kPackage);

    QVERIFY(adapters.filesystemProbe.IsReparsePoint(link));
    QCOMPARE(NormalizeReparseTarget(adapters.linkService.ReadLinkTarget(link).value()), disk.Package());
    QVERIFY(ContentsOfFile(link / "manifest.json").find("2.0.0") != std::string::npos);
    QCOMPARE(std::filesystem::file_size(link / "scenery" / "world" / "a.bgl"), std::uintmax_t{3000});
}

void PosixArchiveInstallOnRealDiskTest::AThirdCopyNeedsTheDiscardFlagWhileTheQuarantineHoldsTheSecond()
{
    const Disk disk;
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    TheFirstCopyIsInstalledAndLinked(disk, adapters);
    QVERIFY(!QTest::currentTestFailed());
    QVERIFY(disk.MakeTheArchive("2.0.0", 3000));
    QCOMPARE(adapters.Install(disk, "2.0.0", false, disk.Package()).packages.front().result, FileResult::Completed);
    QVERIFY(disk.MakeTheArchive("3.0.0", 4000));

    const WhatIsInTheArchive found =
        adapters.installing.Inspect(disk.Profile(), disk.Archive("3.0.0"), disk.Category());

    QCOMPARE(found.packages.size(), std::size_t{1});
    QVERIFY(found.packages.front().admission.theQuarantineHoldsAnEarlierCopy);

    const WhatInstallingDid refused = adapters.Install(disk, "3.0.0", false, disk.Package());

    QCOMPARE(refused.stopped, FileResult::Completed);
    QCOMPARE(refused.packages.size(), std::size_t{1});
    QCOMPARE(refused.packages.front().result, FileResult::TheQuarantineIsOccupied);
    QVERIFY(!refused.packages.front().replaced);
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("2.0.0")));
    QVERIFY(ContentsOfFile(QuarantinedCopy(disk) / "manifest.json").find("1.0.0") != std::string::npos);
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));

    const WhatInstallingDid discarding = adapters.Install(disk, "3.0.0", true, disk.Package());

    QCOMPARE(discarding.packages.size(), std::size_t{1});
    QCOMPARE(discarding.packages.front().result, FileResult::Completed);
    QVERIFY(discarding.packages.front().replaced);
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("3.0.0")));
    QVERIFY(ContentsOfFile(QuarantinedCopy(disk) / "manifest.json").find("2.0.0") != std::string::npos);
    QCOMPARE(std::filesystem::file_size(QuarantinedCopy(disk) / "scenery" / "world" / "a.bgl"), std::uintmax_t{3000});
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QVERIFY2(!std::filesystem::exists(SwapSlotFor(QuarantinedCopy(disk))),
             "the earlier copy set aside for the swap is gone once it was discarded");

    const std::filesystem::path link = disk.Destination() / PathFromUtf8(kPackage);

    QCOMPARE(NormalizeReparseTarget(adapters.linkService.ReadLinkTarget(link).value()), disk.Package());
    QVERIFY(ContentsOfFile(link / "manifest.json").find("3.0.0") != std::string::npos);
}

void PosixArchiveInstallOnRealDiskTest::
    AKilledInstallLeavesADiscardOnlyStagingThatIsNotAnAddonAndTheNextInstallRemovesIt()
{
    const Disk disk;
    QVERIFY(disk.MakeTheArchive("1.0.0", 2048));
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const std::filesystem::path staging = InstallStagingIn(disk.Category());
    WriteFile(staging / PathFromUtf8(kPackage) / "manifest.json", R"({"title": "half", "package_version": "0.0.1"})");
    WriteFile(staging / PathFromUtf8(kPackage) / "scenery" / "half.bgl", "half of it");

    const std::vector<StagingLeftover> leftovers = adapters.importing.Leftovers(disk.Profile());

    QCOMPARE(leftovers.size(), std::size_t{1});
    QCOMPARE(leftovers.front().staging, staging);
    QVERIFY(!leftovers.front().CanBeResumed());

    std::vector<std::filesystem::path> shown;
    for (const TreeNode& library : LibraryTreesOf(adapters.catalog, disk.Profile()))
    {
        CollectPathsBelow(library, shown);
    }

    QVERIFY(std::ranges::none_of(shown,
                                 [&staging](const std::filesystem::path& path)
                                 {
                                     return PathIsInside(path, staging);
                                 }));

    const WhatInstallingDid did = adapters.Install(disk, "1.0.0");

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(!std::filesystem::exists(staging));
    QVERIFY(adapters.importing.Leftovers(disk.Profile()).empty());
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("1.0.0")));
    QVERIFY(!std::filesystem::exists(disk.Package() / "scenery" / "half.bgl"));

    const std::vector<OperationRecord> history = adapters.journal.Read();

    QVERIFY(std::ranges::any_of(history,
                                [](const OperationRecord& record)
                                {
                                    return record.kind == OperationKind::DiscardStaging;
                                }));
}

void PosixArchiveInstallOnRealDiskTest::ReadOnlyFilesLeftInTheStagingDoNotKeepItAfterAnInstall()
{
    const Disk disk;
    QVERIFY(disk.MakeTheArchive("1.0.0", 2048, true));
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const ReadOnlyOnceExtracted readOnly(adapters.archives, {});
    const ArchiveInstallService installing{
        readOnly, adapters.manifestParser, adapters.filesystemProbe, adapters.files, adapters.log, adapters.importing};

    const WhatInstallingDid did = adapters.InstallThrough(installing, disk, "1.0.0");

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(!did.theStagingStayed);
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QVERIFY(adapters.importing.Leftovers(disk.Profile()).empty());
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("1.0.0")));

    MakeEverythingWritable(disk.Package());
}

void PosixArchiveInstallOnRealDiskTest::ReadOnlyFilesDoNotKeepTheStagingAfterAFailedVerification()
{
    const Disk disk;
    QVERIFY(disk.MakeTheArchive("1.0.0", 2048));
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const ReadOnlyOnceExtracted readOnly(adapters.archives, PathFromUtf8(kPackage) / "scenery" / "world" / "a.bgl");
    const ArchiveInstallService installing{
        readOnly, adapters.manifestParser, adapters.filesystemProbe, adapters.files, adapters.log, adapters.importing};

    const WhatInstallingDid did = adapters.InstallThrough(installing, disk, "1.0.0");

    QCOMPARE(did.stopped, FileResult::VerificationFailed);
    QVERIFY(did.packages.empty());
    QVERIFY(!did.theStagingStayed);
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QVERIFY(!std::filesystem::exists(disk.Package()));
    QVERIFY(adapters.importing.Leftovers(disk.Profile()).empty());
}

void PosixArchiveInstallOnRealDiskTest::TheSameFolderWrittenInTwoCasesIsInstalledFromARealZip()
{
    const Disk disk;
    const std::filesystem::path zip = disk.Root() / "downloads" / "two-cases.zip";
    WriteFile(zip,
              StoredZipOf({{"Pkg/manifest.json", R"({"title": "Pkg", "package_version": "1.0.0"})"},
                           {"Pkg/a.txt", "alpha"},
                           {"pkg/b.txt", "bravo!"}}));
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");

    const WhatIsInTheArchive found = adapters.installing.Inspect(disk.Profile(), zip, disk.Category());

    QCOMPARE(found.failure, ArchiveFailure::None);
    QCOMPARE(found.refusal, NothingToInstall::None);
    QCOMPARE(found.packages.size(), std::size_t{1});

    const WhatInstallingDid did = adapters.installing.Install(
        disk.Profile(), adapters.Entries(disk),
        ArchiveInstallRequest{.archive = zip, .category = disk.Category(), .packages = {ChosenPackage{.root = "Pkg"}}});

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QVERIFY(!did.theStagingStayed);
    QCOMPARE(std::filesystem::file_size(disk.Category() / "Pkg" / "a.txt"), std::uintmax_t{5});
    QCOMPARE(std::filesystem::file_size(disk.Category() / "Pkg" / "b.txt"), std::uintmax_t{6});
    QVERIFY(std::filesystem::exists(disk.Category() / "Pkg" / "manifest.json"));
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
}

void PosixArchiveInstallOnRealDiskTest::ASecondLooseArchiveWithTheSameFolderDoesNotReplaceTheFirst()
{
    const Disk disk;
    QVERIFY(disk.MakeTheArchive("1.0.0", 2048));
    QVERIFY(disk.MakeTheArchive("2.0.0", 3000));
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");

    const WhatIsInTheArchive offeredFirst =
        adapters.installing.Inspect(disk.Profile(), disk.Archive("1.0.0"), disk.Category());
    const WhatIsInTheArchive offeredSecond =
        adapters.installing.Inspect(disk.Profile(), disk.Archive("2.0.0"), disk.Category());

    QVERIFY(offeredFirst.packages.front().admission.occupant.empty());
    QVERIFY(offeredSecond.packages.front().admission.occupant.empty());

    const WhatInstallingDid first = adapters.Install(disk, "1.0.0");
    const WhatInstallingDid second = adapters.Install(disk, "2.0.0");

    QCOMPARE(first.packages.front().result, FileResult::Completed);
    QCOMPARE(second.stopped, FileResult::Completed);
    QCOMPARE(second.packages.size(), std::size_t{1});
    QCOMPARE(second.packages.front().result, FileResult::TheDiskDisagreesWithTheScan);
    QVERIFY(!second.packages.front().replaced);
    QCOMPARE(TreeOf(disk.Package()), TreeOf(disk.Source("1.0.0")));
    QVERIFY(ContentsOfFile(disk.Package() / "manifest.json").find("1.0.0") != std::string::npos);
    QVERIFY(!std::filesystem::exists(QuarantineFolderInside(disk.LibraryRoot())));
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
}

void PosixArchiveInstallOnRealDiskTest::ADroppedFolderInstallsItsPackageAndStaysExactlyAsItWas()
{
    const Disk disk;
    MakeTheDroppedFolder(disk, "1.0.0", 2048);
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const QStringList before = SnapshotOf(TheDroppedFolder(disk));

    const WhatIsInTheArchive found =
        adapters.installing.Inspect(disk.Profile(), TheDroppedFolder(disk), disk.Category());

    QCOMPARE(found.failure, ArchiveFailure::None);
    QCOMPARE(found.refusal, NothingToInstall::None);
    QVERIFY(found.theSourceIsAFolder);
    QCOMPARE(found.room, FileResult::Completed);
    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(Shown(FolderNameOf(found.packages.front().package).Text()), Shown(kPackage));
    QCOMPARE(found.packages.front().versionInTheArchive, std::string{"1.0.0"});
    QCOMPARE(found.packages.front().admission.result, FileResult::Completed);
    QCOMPARE(SnapshotOf(TheDroppedFolder(disk)), before);

    const WhatInstallingDid did = adapters.installing.Install(disk.Profile(), adapters.Entries(disk),
                                                              InstallingFrom(disk, TheDroppedFolder(disk)));

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QCOMPARE(did.packages.front().landed, disk.Package());
    QVERIFY(!did.packages.front().replaced);
    QCOMPARE(TreeOf(disk.Package()), TreeOf(TheDroppedFolder(disk)));
    QCOMPARE(ContentsOfFile(disk.Package() / "scenery" / "world" / "a.bgl"), std::string(2048, 'b'));
    QCOMPARE(ContentsOfFile(disk.Package() / "manifest.json"),
             ContentsOfFile(TheDroppedFolder(disk) / "manifest.json"));
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QCOMPARE(SnapshotOf(TheDroppedFolder(disk)), before);
}

void PosixArchiveInstallOnRealDiskTest::ADroppedFolderHoldingTwoPackagesInstallsTheOneChosen()
{
    const Disk disk;
    const std::filesystem::path envelope = disk.Root() / "dropped" / "Envelope";
    WriteFile(envelope / "alpha" / "manifest.json", R"({"title": "Alpha", "package_version": "1.0.0"})");
    WriteFile(envelope / "alpha" / "scenery" / "a.bgl", std::string(100, 'a'));
    WriteFile(envelope / "beta" / "manifest.json", R"({"title": "Beta", "package_version": "2.0.0"})");
    WriteFile(envelope / "beta" / "scenery" / "b.bgl", std::string(200, 'b'));
    WriteFile(envelope / "readme.txt", "read me");
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const QStringList before = SnapshotOf(envelope);

    const WhatIsInTheArchive found = adapters.installing.Inspect(disk.Profile(), envelope, disk.Category());

    QCOMPARE(found.packages.size(), std::size_t{2});
    QCOMPARE(EntryPathOf(found.packages[0].package), std::string{"Envelope/alpha"});
    QCOMPARE(EntryPathOf(found.packages[1].package), std::string{"Envelope/beta"});

    const WhatInstallingDid did = adapters.installing.Install(
        disk.Profile(), adapters.Entries(disk),
        ArchiveInstallRequest{
            .archive = envelope, .category = disk.Category(), .packages = {ChosenPackage{.root = "Envelope/beta"}}});

    QCOMPARE(did.stopped, FileResult::Completed);
    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::Completed);
    QCOMPARE(did.packages.front().landed, disk.Category() / "beta");
    QCOMPARE(ContentsOfFile(disk.Category() / "beta" / "scenery" / "b.bgl"), std::string(200, 'b'));
    QVERIFY(!std::filesystem::exists(disk.Category() / "alpha"));
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QCOMPARE(SnapshotOf(envelope), before);
}

void PosixArchiveInstallOnRealDiskTest::ADroppedFolderWithASymlinkInsideIsRefusedBeforeAnyWrite()
{
    const Disk disk;
    MakeTheDroppedFolder(disk, "1.0.0", 512);
    const std::filesystem::path elsewhere = disk.Root() / "elsewhere";
    WriteFile(elsewhere / "secret.txt", "not part of the addon");
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    PosixLinkService links;
    QCOMPARE(links.CreateLink(TheDroppedFolder(disk) / "scenery" / "shortcut", elsewhere, LinkType::Symbolic),
             LinkFailure::None);

    const WhatIsInTheArchive found =
        adapters.installing.Inspect(disk.Profile(), TheDroppedFolder(disk), disk.Category());

    QCOMPARE(found.refusal, NothingToInstall::AnEntryIsNotSafe);
    QVERIFY(found.packages.empty());

    const WhatInstallingDid did = adapters.installing.Install(disk.Profile(), adapters.Entries(disk),
                                                              InstallingFrom(disk, TheDroppedFolder(disk)));

    QCOMPARE(did.refusal, NothingToInstall::AnEntryIsNotSafe);
    QVERIFY(did.packages.empty());
    QVERIFY(std::filesystem::is_empty(disk.Category()));
    QVERIFY(std::filesystem::exists(elsewhere / "secret.txt"));
    QVERIFY(std::filesystem::exists(TheDroppedFolder(disk) / "manifest.json"));
}

void PosixArchiveInstallOnRealDiskTest::ADroppedFolderThatIsTheInstalledPackageIsRefusedAndTheInstalledOneStaysIntact()
{
    const Disk disk;
    MakeTheDroppedFolder(disk, "1.0.0", 1024);
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    QCOMPARE(adapters.installing
                 .Install(disk.Profile(), adapters.Entries(disk), InstallingFrom(disk, TheDroppedFolder(disk)))
                 .packages.front()
                 .result,
             FileResult::Completed);
    const QStringList installed = SnapshotOf(disk.Package());

    const WhatIsInTheArchive found = adapters.installing.Inspect(disk.Profile(), disk.Package(), disk.Category());

    QCOMPARE(found.packages.size(), std::size_t{1});
    QCOMPARE(found.packages.front().admission.occupant, disk.Package());

    ArchiveInstallRequest request = InstallingFrom(disk, disk.Package());
    request.packages.front().replaces = disk.Package();

    const WhatInstallingDid did = adapters.installing.Install(disk.Profile(), adapters.Entries(disk), request);

    QCOMPARE(did.packages.size(), std::size_t{1});
    QCOMPARE(did.packages.front().result, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QCOMPARE(SnapshotOf(disk.Package()), installed);
    QVERIFY(!std::filesystem::exists(QuarantinedCopy(disk)));
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
}

void PosixArchiveInstallOnRealDiskTest::ADroppedLibraryIsRefusedBeforeAnyWrite()
{
    const Disk disk;
    MakeTheDroppedFolder(disk, "1.0.0", 256);
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    WriteFile(disk.Category() / "other" / "manifest.json", R"({"title": "Other"})");
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const QStringList before = SnapshotOf(disk.LibraryRoot());

    const WhatInstallingDid did =
        adapters.installing.Install(disk.Profile(), adapters.Entries(disk),
                                    ArchiveInstallRequest{.archive = disk.LibraryRoot(),
                                                          .category = disk.Category(),
                                                          .packages = {ChosenPackage{.root = "Library/Utils/other"}}});

    QCOMPARE(did.stopped, FileResult::TheSourceIsInsideWhatWouldBeRemoved);
    QCOMPARE(SnapshotOf(disk.LibraryRoot()), before);
}

void PosixArchiveInstallOnRealDiskTest::ACancelledCopyOfADroppedFolderLeavesNeitherStagingNorPackage()
{
    const Disk disk;
    MakeTheDroppedFolder(disk, "1.0.0", 8 * 1024 * 1024);
    QVERIFY(std::filesystem::create_directories(disk.Category()));
    const Adapters adapters(disk.Root() / "journal" / "operations.jsonl");
    const QStringList before = SnapshotOf(TheDroppedFolder(disk));
    std::uintmax_t highest = 0;

    const WhatInstallingDid did = adapters.installing.Install(disk.Profile(), adapters.Entries(disk),
                                                              InstallingFrom(disk, TheDroppedFolder(disk)),
                                                              [&highest](const CopyProgress& progress)
                                                              {
                                                                  highest = std::max(highest, progress.copiedBytes);

                                                                  return progress.copiedBytes < 4096;
                                                              });

    QCOMPARE(did.stopped, FileResult::Cancelled);
    QVERIFY(highest > 0);
    QVERIFY(!std::filesystem::exists(disk.Package()));
    QVERIFY(!std::filesystem::exists(InstallStagingIn(disk.Category())));
    QCOMPARE(SnapshotOf(TheDroppedFolder(disk)), before);
}

QTEST_GUILESS_MAIN(PosixArchiveInstallOnRealDiskTest)

#include "tst_posix_archive_install_on_real_disk.moc"
