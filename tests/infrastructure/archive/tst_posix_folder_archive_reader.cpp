#include <QtCore/QCryptographicHash>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <string_view>
#include <system_error>
#include <thread>

#include "domain/importing/ImportPaths.h"
#include "domain/installing/PackagesInAnArchive.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/archive/ArchiveOrFolderReader.h"
#include "infrastructure/archive/PosixFolderArchiveReader.h"
#include "support/PathText.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class PosixFolderArchiveReaderTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AFolderThatIsThePackageIsListedUnderItsOwnName();
        static void AFolderWithTwoPackagesInsideListsBothUnderTheFolderName();
        static void ATrailingSeparatorDoesNotChangeTheListing();
        static void ARelativePathIsListedUnderTheNameOfTheFolder();
        static void AFolderInsideAFolderWithAccentsIsListedAndCopied();
        static void ASymlinkInsideMakesTheListingCarryAnOtherEntryAndNeverEntersIt();
        static void ASymlinkToAFileInsideIsAnOtherEntryToo();
        static void ASpecialFileInsideIsAnOtherEntry();
        static void AFolderThatIsItselfASymlinkCouldNotBeOpened();
        static void AFileAMissingPathAndTheRootOfTheFilesystemCouldNotBeOpened();
        static void AScanPastTheTimeLimitTookTooLong();
        static void AScanWithinTheTimeLimitSucceeds();
        static void TheManifestIsReadFromUnderTheFolderWithoutItsFirstSegment();
        static void TheReadStopsAtTheCapAndAMissingEntryCouldNotBeOpened();
        static void AnEntryThatEscapesTheFolderCouldNotBeOpened();
        static void AnEntryThatIsALinkCouldNotBeOpened();
        static void OnlyWhatWasListedIsCopiedByteForByteAndTheSourceStaysTheSame();
        static void TheCopyKeepsTheWriteTimeAndTheExecutableBit();
        static void TheProgressReachesTheTotalOfTheListedFiles();
        static void StoppingInTheMiddleOfABigFileCancelsAndLeavesTheSourceAlone();
        static void ACopyIntoAFolderThatIsNotAnEmptyStagingFolderWritesNothing();
        static void AListedEntryOutsideTheFolderIsNotCopied();
        static void AListedEntryUnderAnotherFolderNameIsNotCopiedFromThisOne();
        static void AFileThatVanishedAfterTheListingCouldNotBeExtracted();
        static void AFileSwappedForALinkAfterTheListingIsNotFollowed();
        static void AFileSwappedForAPipeAfterTheListingIsRefusedWithoutWaiting();
        static void AFolderIsHandedToTheFolderReaderAndAFileToTheArchiveReader();
        static void AFolderThatIsALinkToAFolderIsHandedToTheFolderReader();
        static void EveryOperationFollowsTheKindOfTheSource();
        static void ANameThatIsNotUtf8InsideIsListedAsOtherAndNothingThrows();
        static void AFolderNamedWithBytesThatAreNotUtf8CouldNotBeOpened();
        static void ARealCopyWithAShortIdleLimitStillEndsInNoneWithTheSameTree();
        static void NobodyListeningStillCopiesTheFolder();
        static void TheProgressIsOnlyEverReportedOnTheCallingThread();
    };
}

namespace
{
    [[nodiscard]] std::string Utf8Of(const std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    const std::string kPackage = Utf8Of(u8"[RR WL] pacote ação");
    const std::string kLayout = R"({"content": []})";
    const std::string kManifest = Utf8Of(u8"{\"title\":\"Pacote ação\",\"package_version\":\"1.2.3\"}");
    constexpr std::size_t kBigFileBytes = 16 * 1024 * 1024;
    constexpr std::chrono::milliseconds kBlockedBound{6000};

    const std::function<bool(const CopyProgress&)> kKeepGoing = [](const CopyProgress&)
    {
        return true;
    };

    void WriteFile(const std::filesystem::path& file, const std::string& content)
    {
        std::filesystem::create_directories(file.parent_path());

        std::ofstream stream(file, std::ios::binary);
        stream << content;
    }

    [[nodiscard]] std::string ContentsOfFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] QString Shown(const std::string& text)
    {
        return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }

    [[nodiscard]] QStringList SnapshotOf(const std::filesystem::path& root)
    {
        QStringList lines;

        for (const std::filesystem::directory_entry& item : std::filesystem::recursive_directory_iterator(root))
        {
            const QString name = Shown(AsUtf8(item.path().lexically_relative(root)));
            if (item.is_directory())
            {
                lines << name + QStringLiteral("/");
                continue;
            }

            const std::string content = ContentsOfFile(item.path());
            const QByteArray digest = QCryptographicHash::hash(
                QByteArrayView(content.data(), static_cast<qsizetype>(content.size())), QCryptographicHash::Sha256);

            lines << QStringLiteral("%1 | %2 | %3 | %4")
                         .arg(name)
                         .arg(content.size())
                         .arg(QString::fromLatin1(digest.toHex()))
                         .arg(item.last_write_time().time_since_epoch().count());
        }

        lines.sort();

        return lines;
    }

    [[nodiscard]] QStringList PathsOf(const ArchiveListing& listing)
    {
        QStringList paths;

        for (const ArchiveEntry& entry : listing.entries)
        {
            paths << QStringLiteral("%1 | %2 | %3")
                         .arg(Shown(entry.path))
                         .arg(static_cast<int>(entry.kind))
                         .arg(entry.bytes);
        }

        paths.sort();

        return paths;
    }

    [[nodiscard]] QString Line(const std::string& path, const ArchiveEntryKind kind, const std::uintmax_t bytes)
    {
        return QStringLiteral("%1 | %2 | %3").arg(Shown(path)).arg(static_cast<int>(kind)).arg(bytes);
    }

    void MakeThePackage(const std::filesystem::path& folder)
    {
        WriteFile(folder / "manifest.json", kManifest);
        WriteFile(folder / "layout.json", kLayout);
        WriteFile(folder / "scenery" / "world" / "a.bgl", std::string(2048, 'b'));
        std::filesystem::create_directories(folder / "empty");
    }

    [[nodiscard]] std::uintmax_t BytesOfTheListedFiles(const std::vector<ArchiveEntry>& entries)
    {
        std::uintmax_t total = 0;

        for (const ArchiveEntry& entry : entries)
        {
            total += entry.kind == ArchiveEntryKind::File ? entry.bytes : 0;
        }

        return total;
    }

    [[nodiscard]] std::filesystem::path MakeStaging(const std::filesystem::path& under)
    {
        const std::filesystem::path staging = InstallStagingIn(under);
        std::filesystem::create_directories(staging);

        return staging;
    }

    struct Disk
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
        }

        [[nodiscard]] std::filesystem::path Dropped() const
        {
            return Root() / "downloads" / PathFromUtf8(kPackage);
        }
    };

    struct ScriptedReader final : ArchiveReader
    {
        explicit ScriptedReader(QString name) : name(std::move(name))
        {
        }

        [[nodiscard]] ArchiveListing EntriesOf(const std::filesystem::path&) const override
        {
            calls.push_back(name + ":list");

            return {};
        }

        [[nodiscard]] ArchiveText
        ContentsOf(const std::filesystem::path&, const std::string&, std::size_t) const override
        {
            calls.push_back(name + ":read");

            return {};
        }

        [[nodiscard]] ArchiveFailure ExtractAll(const std::filesystem::path&,
                                                const std::vector<ArchiveEntry>&,
                                                const std::filesystem::path&,
                                                const std::function<bool(const CopyProgress&)>&) const override
        {
            calls.push_back(name + ":extract");

            return ArchiveFailure::None;
        }

        QString name;
        mutable QStringList calls{};
    };

    [[nodiscard]] std::vector<ArchiveEntry> ListedBy(const PosixFolderArchiveReader& reader,
                                                     const std::filesystem::path& folder)
    {
        const ArchiveListing listing = reader.EntriesOf(folder);

        return listing.failure == ArchiveFailure::None ? listing.entries : std::vector<ArchiveEntry>{};
    }

    class InDirectory
    {
    public:
        explicit InDirectory(const std::filesystem::path& directory) : before_(std::filesystem::current_path())
        {
            std::filesystem::current_path(directory);
        }

        InDirectory(const InDirectory&) = delete;
        InDirectory& operator=(const InDirectory&) = delete;

        ~InDirectory()
        {
            std::error_code error;
            std::filesystem::current_path(before_, error);
        }

    private:
        std::filesystem::path before_;
    };

    const std::string kBytesThatAreNotUtf8 = "bad\xFF\xFE";

    class RemovedAtTheEnd
    {
    public:
        explicit RemovedAtTheEnd(std::filesystem::path path) : path_(std::move(path))
        {
        }

        RemovedAtTheEnd(const RemovedAtTheEnd&) = delete;
        RemovedAtTheEnd& operator=(const RemovedAtTheEnd&) = delete;

        ~RemovedAtTheEnd()
        {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

    private:
        std::filesystem::path path_;
    };
}

void PosixFolderArchiveReaderTest::AFolderThatIsThePackageIsListedUnderItsOwnName()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(PathsOf(listing),
             (QStringList{Line(kPackage, ArchiveEntryKind::Folder, 0),
                          Line(kPackage + "/empty", ArchiveEntryKind::Folder, 0),
                          Line(kPackage + "/layout.json", ArchiveEntryKind::File, kLayout.size()),
                          Line(kPackage + "/manifest.json", ArchiveEntryKind::File, kManifest.size()),
                          Line(kPackage + "/scenery", ArchiveEntryKind::Folder, 0),
                          Line(kPackage + "/scenery/world", ArchiveEntryKind::Folder, 0),
                          Line(kPackage + "/scenery/world/a.bgl", ArchiveEntryKind::File, 2048)}));

    const WhatAnArchiveHolds holds = PackagesIn(listing.entries);

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{1});
    QCOMPARE(Shown(FolderNameOf(holds.packages.front()).Text()), Shown(kPackage));
    QCOMPARE(holds.packages.front().manifest, kPackage + "/manifest.json");
}

void PosixFolderArchiveReaderTest::AFolderWithTwoPackagesInsideListsBothUnderTheFolderName()
{
    const Disk disk;
    const std::filesystem::path envelope = disk.Root() / "Envelope";
    MakeThePackage(envelope / "alpha");
    MakeThePackage(envelope / "deeper" / "beta");
    WriteFile(envelope / "readme.txt", "read me");
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(envelope);

    QCOMPARE(listing.failure, ArchiveFailure::None);

    const WhatAnArchiveHolds holds = PackagesIn(listing.entries);

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{2});
    QCOMPARE(EntryPathOf(holds.packages[0]), std::string{"Envelope/alpha"});
    QCOMPARE(EntryPathOf(holds.packages[1]), std::string{"Envelope/deeper/beta"});
}

void PosixFolderArchiveReaderTest::ATrailingSeparatorDoesNotChangeTheListing()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;

    const ArchiveListing plain = reader.EntriesOf(disk.Dropped());
    const ArchiveListing slashed = reader.EntriesOf(disk.Dropped() / "");

    QCOMPARE(slashed.failure, ArchiveFailure::None);
    QCOMPARE(PathsOf(slashed), PathsOf(plain));
    QVERIFY(!slashed.entries.empty());
}

void PosixFolderArchiveReaderTest::ARelativePathIsListedUnderTheNameOfTheFolder()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const InDirectory inside(disk.Root() / "downloads");
    const PosixFolderArchiveReader reader;

    const ArchiveListing relative = reader.EntriesOf(PathFromUtf8(kPackage));
    const ArchiveListing dotted = reader.EntriesOf(PathFromUtf8("./" + kPackage + "/../" + kPackage));

    QCOMPARE(relative.failure, ArchiveFailure::None);
    QCOMPARE(PathsOf(relative), PathsOf(reader.EntriesOf(disk.Dropped())));
    QCOMPARE(PathsOf(dotted), PathsOf(relative));
    QCOMPARE(reader.EntriesOf(".").failure, ArchiveFailure::None);
    QCOMPARE(PathsOf(reader.EntriesOf(PathFromUtf8(kPackage + "/."))), PathsOf(relative));
}

void PosixFolderArchiveReaderTest::AFolderInsideAFolderWithAccentsIsListedAndCopied()
{
    const Disk disk;
    const std::filesystem::path accented = disk.Root() / PathFromUtf8(Utf8Of(u8"Ação e música")) / "pacote";
    MakeThePackage(accented);
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, accented);
    const std::filesystem::path staging = MakeStaging(disk.Root() / PathFromUtf8(Utf8Of(u8"Ação e música")));

    QVERIFY(!listed.empty());
    QCOMPARE(reader.ExtractAll(accented, listed, staging, kKeepGoing), ArchiveFailure::None);
    QCOMPARE(SnapshotOf(staging / "pacote"), SnapshotOf(accented));
}

void PosixFolderArchiveReaderTest::ASymlinkInsideMakesTheListingCarryAnOtherEntryAndNeverEntersIt()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const std::filesystem::path elsewhere = disk.Root() / "elsewhere";
    WriteFile(elsewhere / "secret.txt", "not part of the folder");
    std::filesystem::create_directory_symlink(elsewhere, disk.Dropped() / "scenery" / "shortcut");
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QVERIFY(PathsOf(listing).contains(Line(kPackage + "/scenery/shortcut", ArchiveEntryKind::Other, 0)));
    QVERIFY(std::ranges::none_of(listing.entries,
                                 [](const ArchiveEntry& entry)
                                 {
                                     return entry.path.find("secret.txt") != std::string::npos;
                                 }));
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
    QVERIFY(std::filesystem::exists(elsewhere / "secret.txt"));
}

void PosixFolderArchiveReaderTest::ASymlinkToAFileInsideIsAnOtherEntryToo()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "outside.txt", "outside");
    std::filesystem::create_symlink(disk.Root() / "outside.txt", disk.Dropped() / "outside-link.txt");
    std::filesystem::create_symlink(disk.Root() / "missing.txt", disk.Dropped() / "dangling.txt");
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QVERIFY(PathsOf(listing).contains(Line(kPackage + "/outside-link.txt", ArchiveEntryKind::Other, 0)));
    QVERIFY(PathsOf(listing).contains(Line(kPackage + "/dangling.txt", ArchiveEntryKind::Other, 0)));
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
}

void PosixFolderArchiveReaderTest::ASpecialFileInsideIsAnOtherEntry()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    QCOMPARE(::mkfifo((disk.Dropped() / "pipe").c_str(), 0600), 0);
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QVERIFY(PathsOf(listing).contains(Line(kPackage + "/pipe", ArchiveEntryKind::Other, 0)));
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
}

void PosixFolderArchiveReaderTest::AFolderThatIsItselfASymlinkCouldNotBeOpened()
{
    const Disk disk;
    const std::filesystem::path real = disk.Root() / "real";
    MakeThePackage(real / PathFromUtf8(kPackage));
    const std::filesystem::path linked = disk.Root() / "linked";
    std::filesystem::create_directory_symlink(real / PathFromUtf8(kPackage), linked);
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(linked);

    QCOMPARE(listing.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(listing.entries.empty());
    QVERIFY(std::filesystem::exists(real / PathFromUtf8(kPackage) / "manifest.json"));
}

void PosixFolderArchiveReaderTest::AFileAMissingPathAndTheRootOfTheFilesystemCouldNotBeOpened()
{
    const Disk disk;
    WriteFile(disk.Root() / "plain.txt", "a file");
    const PosixFolderArchiveReader reader;

    QCOMPARE(reader.EntriesOf(disk.Root() / "plain.txt").failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(disk.Root() / "missing").failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(disk.Root().root_path()).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(std::filesystem::path{}).failure, ArchiveFailure::CouldNotOpen);
}

void PosixFolderArchiveReaderTest::AScanPastTheTimeLimitTookTooLong()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader(std::chrono::milliseconds{0});

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::TookTooLong);
    QVERIFY(listing.entries.empty());
}

void PosixFolderArchiveReaderTest::AScanWithinTheTimeLimitSucceeds()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader(std::chrono::minutes{5});

    QCOMPARE(reader.EntriesOf(disk.Dropped()).failure, ArchiveFailure::None);
    QCOMPARE(PosixFolderArchiveReader{}.EntriesOf(disk.Dropped()).failure, ArchiveFailure::None);
}

void PosixFolderArchiveReaderTest::TheManifestIsReadFromUnderTheFolderWithoutItsFirstSegment()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;

    const ArchiveText text = reader.ContentsOf(disk.Dropped(), kPackage + "/manifest.json", 1024 * 1024);

    QCOMPARE(text.failure, ArchiveFailure::None);
    QCOMPARE(text.text, kManifest);

    const ArchiveText deeper = reader.ContentsOf(disk.Dropped(), kPackage + "/scenery/world/a.bgl", 1024);

    QCOMPARE(deeper.text, std::string(1024, 'b'));
}

void PosixFolderArchiveReaderTest::TheReadStopsAtTheCapAndAMissingEntryCouldNotBeOpened()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;

    const ArchiveText capped = reader.ContentsOf(disk.Dropped(), kPackage + "/manifest.json", 5);

    QCOMPARE(capped.failure, ArchiveFailure::None);
    QCOMPARE(capped.text, kManifest.substr(0, 5));
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/manifest.json", 0).text, std::string{});
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/nothing.json", 100).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/scenery", 100).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage, 100).failure, ArchiveFailure::CouldNotOpen);
}

void PosixFolderArchiveReaderTest::AnEntryThatEscapesTheFolderCouldNotBeOpened()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "downloads" / "outside.txt", "outside");
    const PosixFolderArchiveReader reader;

    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/../outside.txt", 100).failure,
             ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), AsUtf8(disk.Root() / "downloads" / "outside.txt"), 100).failure,
             ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "//etc/passwd", 100).failure, ArchiveFailure::CouldNotOpen);
}

void PosixFolderArchiveReaderTest::AnEntryThatIsALinkCouldNotBeOpened()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "outside.txt", "outside");
    std::filesystem::create_symlink(disk.Root() / "outside.txt", disk.Dropped() / "link.txt");
    const PosixFolderArchiveReader reader;

    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/link.txt", 100).failure, ArchiveFailure::CouldNotOpen);
}

void PosixFolderArchiveReaderTest::OnlyWhatWasListedIsCopiedByteForByteAndTheSourceStaysTheSame()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "not-listed.txt", "appeared later");
    const PosixFolderArchiveReader reader;
    std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    std::erase_if(listed,
                  [](const ArchiveEntry& entry)
                  {
                      return entry.path.ends_with("not-listed.txt") || entry.path.ends_with("layout.json");
                  });
    const QStringList before = SnapshotOf(disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing);

    QCOMPARE(failure, ArchiveFailure::None);
    QVERIFY(!std::filesystem::exists(staging / PathFromUtf8(kPackage) / "not-listed.txt"));
    QVERIFY(!std::filesystem::exists(staging / PathFromUtf8(kPackage) / "layout.json"));
    QVERIFY(std::filesystem::is_directory(staging / PathFromUtf8(kPackage) / "empty"));
    QCOMPARE(ContentsOfFile(staging / PathFromUtf8(kPackage) / "manifest.json"), kManifest);
    QCOMPARE(ContentsOfFile(staging / PathFromUtf8(kPackage) / "scenery" / "world" / "a.bgl"), std::string(2048, 'b'));
    QCOMPARE(SnapshotOf(disk.Dropped()), before);
}

void PosixFolderArchiveReaderTest::TheCopyKeepsTheWriteTimeAndTheExecutableBit()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const std::filesystem::path script = disk.Dropped() / "run.sh";
    WriteFile(script, "#!/bin/sh");
    std::filesystem::permissions(script, std::filesystem::perms::owner_all | std::filesystem::perms::group_read,
                                 std::filesystem::perm_options::replace);
    const std::filesystem::file_time_type moved =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 90);
    std::filesystem::last_write_time(disk.Dropped() / "manifest.json", moved);
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::None);

    const std::filesystem::path copy = staging / PathFromUtf8(kPackage);
    QCOMPARE(std::filesystem::last_write_time(copy / "manifest.json"),
             std::filesystem::last_write_time(disk.Dropped() / "manifest.json"));
    QCOMPARE(std::filesystem::status(copy / "run.sh").permissions(), std::filesystem::status(script).permissions());
}

void PosixFolderArchiveReaderTest::TheProgressReachesTheTotalOfTheListedFiles()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "big.bin", std::string(3 * 1024 * 1024, 'q'));
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    std::vector<CopyProgress> seen;

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging,
                                                     [&seen](const CopyProgress& progress)
                                                     {
                                                         seen.push_back(progress);

                                                         return true;
                                                     });

    QCOMPARE(failure, ArchiveFailure::None);
    QVERIFY(!seen.empty());
    QCOMPARE(seen.back().copiedBytes, BytesOfTheListedFiles(listed));
    QCOMPARE(seen.back().totalBytes, BytesOfTheListedFiles(listed));
    QVERIFY(std::ranges::all_of(seen,
                                [&listed](const CopyProgress& progress)
                                {
                                    return progress.totalBytes == BytesOfTheListedFiles(listed);
                                }));
    QVERIFY(std::ranges::is_sorted(seen, {}, &CopyProgress::copiedBytes));
    QVERIFY(seen.size() > 3);
}

void PosixFolderArchiveReaderTest::StoppingInTheMiddleOfABigFileCancelsAndLeavesTheSourceAlone()
{
    const Disk disk;
    WriteFile(disk.Dropped() / "manifest.json", kManifest);
    WriteFile(disk.Dropped() / "big.bin", std::string(kBigFileBytes, 'q'));
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const QStringList before = SnapshotOf(disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    const std::uintmax_t manifestBytes = kManifest.size();
    std::uintmax_t highest = 0;

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging,
                                                     [&highest, manifestBytes](const CopyProgress& progress)
                                                     {
                                                         highest = std::max(highest, progress.copiedBytes);

                                                         return progress.copiedBytes <= manifestBytes;
                                                     });

    QCOMPARE(failure, ArchiveFailure::Cancelled);
    QVERIFY(highest > manifestBytes);
    QVERIFY(highest < manifestBytes + kBigFileBytes);
    QVERIFY(!std::filesystem::exists(staging / disk.Dropped().filename() / "big.bin"));
    QCOMPARE(SnapshotOf(disk.Dropped()), before);
}

void PosixFolderArchiveReaderTest::ACopyIntoAFolderThatIsNotAnEmptyStagingFolderWritesNothing()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());

    const std::filesystem::path notStaging = disk.Root() / "plain";
    std::filesystem::create_directories(notStaging);

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, notStaging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(std::filesystem::is_empty(notStaging));

    const std::filesystem::path occupied = MakeStaging(disk.Root());
    WriteFile(occupied / "leftover.txt", "left");

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, occupied, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(!std::filesystem::exists(occupied / PathFromUtf8(kPackage)));

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, InstallStagingIn(disk.Root() / "absent"), kKeepGoing),
             ArchiveFailure::CouldNotExtract);
    QVERIFY(!std::filesystem::exists(disk.Root() / "absent"));
}

void PosixFolderArchiveReaderTest::AListedEntryOutsideTheFolderIsNotCopied()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "downloads" / "sibling.txt", "next to the folder");
    const PosixFolderArchiveReader reader;
    std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    listed.push_back(ArchiveEntry{.path = "sibling.txt", .bytes = 18, .kind = ArchiveEntryKind::File});
    listed.push_back(ArchiveEntry{.path = kPackage + "/../sibling.txt", .bytes = 18, .kind = ArchiveEntryKind::File});
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(!std::filesystem::exists(staging / "sibling.txt"));
    QVERIFY(!std::filesystem::exists(disk.Root() / "sibling.txt"));
}

void PosixFolderArchiveReaderTest::AListedEntryUnderAnotherFolderNameIsNotCopiedFromThisOne()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;
    std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    listed.push_back(ArchiveEntry{.path = "Another/manifest.json", .bytes = 1, .kind = ArchiveEntryKind::File});
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(std::filesystem::is_empty(staging));
}

void PosixFolderArchiveReaderTest::AFileThatVanishedAfterTheListingCouldNotBeExtracted()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    std::filesystem::remove(disk.Dropped() / "layout.json");
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
}

void PosixFolderArchiveReaderTest::AFileSwappedForALinkAfterTheListingIsNotFollowed()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "outside.txt", "content from outside the folder");
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    std::filesystem::remove(disk.Dropped() / "layout.json");
    std::filesystem::create_symlink(disk.Root() / "outside.txt", disk.Dropped() / "layout.json");

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(!std::filesystem::exists(staging / PathFromUtf8(kPackage) / "layout.json"));
}

void PosixFolderArchiveReaderTest::AFileSwappedForAPipeAfterTheListingIsRefusedWithoutWaiting()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    std::filesystem::remove(disk.Dropped() / "layout.json");
    QCOMPARE(::mkfifo((disk.Dropped() / "layout.json").c_str(), 0600), 0);
    QElapsedTimer clock;
    clock.start();

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);

    QVERIFY(clock.elapsed() < kBlockedBound.count());
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/layout.json", 100).failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(clock.elapsed() < kBlockedBound.count());
}

void PosixFolderArchiveReaderTest::AFolderIsHandedToTheFolderReaderAndAFileToTheArchiveReader()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "pack.zip", "PK");
    const ScriptedReader folders("folders");
    const ScriptedReader archives("archives");
    const ArchiveOrFolderReader reader(folders, archives);

    static_cast<void>(reader.EntriesOf(disk.Dropped()));
    static_cast<void>(reader.EntriesOf(disk.Root() / "pack.zip"));
    static_cast<void>(reader.EntriesOf(disk.Root() / "missing.zip"));

    QCOMPARE(folders.calls, (QStringList{"folders:list"}));
    QCOMPARE(archives.calls, (QStringList{"archives:list", "archives:list"}));
}

void PosixFolderArchiveReaderTest::AFolderThatIsALinkToAFolderIsHandedToTheFolderReader()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    std::filesystem::create_directory_symlink(disk.Dropped(), disk.Root() / "linked");
    const ScriptedReader folders("folders");
    const ScriptedReader archives("archives");
    const ArchiveOrFolderReader reader(folders, archives);

    static_cast<void>(reader.EntriesOf(disk.Root() / "linked"));

    QCOMPARE(folders.calls, (QStringList{"folders:list"}));
    QVERIFY(archives.calls.isEmpty());
}

void PosixFolderArchiveReaderTest::EveryOperationFollowsTheKindOfTheSource()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "pack.zip", "PK");
    const ScriptedReader folders("folders");
    const ScriptedReader archives("archives");
    const ArchiveOrFolderReader reader(folders, archives);

    static_cast<void>(reader.ContentsOf(disk.Dropped(), "a", 1));
    static_cast<void>(reader.ContentsOf(disk.Root() / "pack.zip", "a", 1));
    static_cast<void>(reader.ExtractAll(disk.Dropped(), {}, disk.Root(), kKeepGoing));
    static_cast<void>(reader.ExtractAll(disk.Root() / "pack.zip", {}, disk.Root(), kKeepGoing));

    QCOMPARE(folders.calls, (QStringList{"folders:read", "folders:extract"}));
    QCOMPARE(archives.calls, (QStringList{"archives:read", "archives:extract"}));
}

void PosixFolderArchiveReaderTest::ANameThatIsNotUtf8InsideIsListedAsOtherAndNothingThrows()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const std::filesystem::path oddFile = disk.Dropped() / (kBytesThatAreNotUtf8 + ".txt");
    const std::filesystem::path oddFolder = disk.Dropped() / (kBytesThatAreNotUtf8 + "dir");
    const RemovedAtTheEnd cleanup(disk.Root());
    WriteFile(oddFile, "odd");
    WriteFile(oddFolder / "inside.txt", "odd");
    const PosixFolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(std::ranges::count(listing.entries, ArchiveEntryKind::Other, &ArchiveEntry::kind), std::ptrdiff_t{2});
    QVERIFY(std::ranges::none_of(listing.entries,
                                 [](const ArchiveEntry& entry)
                                 {
                                     return entry.path.find("inside.txt") != std::string::npos;
                                 }));
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);

    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listing.entries, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(std::filesystem::is_empty(staging));
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/manifest.json", 1024).text, kManifest);
}

void PosixFolderArchiveReaderTest::AFolderNamedWithBytesThatAreNotUtf8CouldNotBeOpened()
{
    const Disk disk;
    const std::filesystem::path odd = disk.Root() / (kBytesThatAreNotUtf8 + "pack");
    const RemovedAtTheEnd cleanup(disk.Root());
    WriteFile(odd / "manifest.json", kManifest);
    const PosixFolderArchiveReader reader;
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.EntriesOf(odd).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(odd, "pack/manifest.json", 1024).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ExtractAll(odd, {}, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(std::filesystem::is_empty(staging));
}

void PosixFolderArchiveReaderTest::ARealCopyWithAShortIdleLimitStillEndsInNoneWithTheSameTree()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "big.bin", std::string(3 * 1024 * 1024, 'q'));
    const PosixFolderArchiveReader reader(kPosixFolderTimeLimit, std::chrono::milliseconds{300});
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing);

    QCOMPARE(failure, ArchiveFailure::None);
    QCOMPARE(SnapshotOf(staging / PathFromUtf8(kPackage)), SnapshotOf(disk.Dropped()));
}

void PosixFolderArchiveReaderTest::NobodyListeningStillCopiesTheFolder()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, {}), ArchiveFailure::None);
    QCOMPARE(SnapshotOf(staging / PathFromUtf8(kPackage)), SnapshotOf(disk.Dropped()));
}

void PosixFolderArchiveReaderTest::TheProgressIsOnlyEverReportedOnTheCallingThread()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "big.bin", std::string(3 * 1024 * 1024, 'q'));
    const PosixFolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    const std::thread::id caller = std::this_thread::get_id();
    int calls = 0;
    bool elsewhere = false;

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging,
                                                     [&](const CopyProgress&)
                                                     {
                                                         ++calls;
                                                         elsewhere = elsewhere || std::this_thread::get_id() != caller;

                                                         return true;
                                                     });

    QCOMPARE(failure, ArchiveFailure::None);
    QVERIFY(calls > 0);
    QVERIFY(!elsewhere);
}

QTEST_APPLESS_MAIN(PosixFolderArchiveReaderTest)

#include "tst_posix_folder_archive_reader.moc"
