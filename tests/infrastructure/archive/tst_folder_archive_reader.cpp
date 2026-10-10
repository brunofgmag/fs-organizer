#include <QtCore/QCryptographicHash>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <winioctl.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <string_view>
#include <system_error>
#include <thread>

#include "domain/importing/ImportPaths.h"
#include "domain/installing/PackagesInAnArchive.h"
#include "domain/support/PathUtils.h"
#include "infrastructure/archive/ArchiveOrFolderReader.h"
#include "infrastructure/archive/FolderArchiveReader.h"
#include "infrastructure/link/WindowsLinkService.h"
#include "support/PathText.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class FolderArchiveReaderTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AFolderThatIsThePackageIsListedUnderItsOwnName();
        static void AFolderWithTwoPackagesInsideListsBothUnderTheFolderName();
        static void ATrailingSeparatorDoesNotChangeTheListing();
        static void AFolderPastTheOldPathCeilingIsListedAndCopied();
        static void AJunctionInsideMakesTheListingCarryAnOtherEntryAndNeverEntersIt();
        static void AFolderThatIsItselfAJunctionCouldNotBeOpened();
        static void AFileAMissingPathAndADriveRootCouldNotBeOpened();
        static void AScanPastTheTimeLimitTookTooLong();
        static void AScanWithinTheTimeLimitSucceeds();
        static void TheManifestIsReadFromUnderTheFolderWithoutItsFirstSegment();
        static void TheReadStopsAtTheCapAndAMissingEntryCouldNotBeOpened();
        static void AnEntryThatEscapesTheFolderCouldNotBeOpened();
        static void OnlyWhatWasListedIsCopiedByteForByteAndTheSourceStaysTheSame();
        static void TheProgressReachesTheTotalOfTheListedFiles();
        static void StoppingInTheMiddleOfABigFileCancelsAndLeavesTheSourceAlone();
        static void ACopyIntoAFolderThatIsNotAnEmptyStagingFolderWritesNothing();
        static void AListedEntryOutsideTheFolderIsNotCopied();
        static void AListedEntryUnderAnotherFolderNameIsNotCopiedFromThisOne();
        static void AFileThatVanishedAfterTheListingCouldNotBeExtracted();
        static void AFolderIsHandedToTheFolderReaderAndAFileToTheArchiveReader();
        static void AJunctionFolderIsHandedToTheFolderReader();
        static void EveryOperationFollowsTheKindOfTheSource();
        static void ANameWithALoneSurrogateInsideIsListedAsOtherAndNothingThrows();
        static void AFolderNamedWithALoneSurrogateCouldNotBeOpened();
        static void ACopyBlockedByAnOplockIsGivenUpOnceTheIdleLimitPasses();
        static void CancellingWhileACopyIsBlockedReturnsCancelledWithoutWaitingForTheOwner();
        static void ARealCopyWithAShortIdleLimitStillEndsInNoneWithTheSameTree();
        static void NobodyListeningStillCopiesTheFolder();
        static void TheProgressIsOnlyEverReportedOnTheCallingThread();
        static void AFileSwappedForALinkAfterTheListingIsNotFollowed();
    };
}

namespace
{
    constexpr std::size_t kBigFileBytes = 16 * 1024 * 1024;

    [[nodiscard]] std::string Utf8Of(const std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    const std::string kPackage = Utf8Of(u8"[RR WL] pacote ação");
    const std::string kLayout = R"({"content": []})";
    const std::string kManifest = Utf8Of(u8"{\"title\":\"Pacote ação\",\"package_version\":\"1.2.3\"}");

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

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
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
            const QString name = AsText(item.path().lexically_relative(root));
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

    [[nodiscard]] std::uintmax_t BytesOfTheFiles(const std::vector<ArchiveEntry>& entries)
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
            return directory.path().toStdWString();
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

    [[nodiscard]] std::vector<ArchiveEntry> ListedBy(const FolderArchiveReader& reader,
                                                     const std::filesystem::path& folder)
    {
        const ArchiveListing listing = reader.EntriesOf(folder);

        return listing.failure == ArchiveFailure::None ? listing.entries : std::vector<ArchiveEntry>{};
    }

    constexpr std::chrono::milliseconds kValve{15000};
    constexpr std::chrono::milliseconds kShortIdle{600};
    constexpr qint64 kBlockedBoundMs = 6000;
    constexpr qint64 kCancelAfterMs = 500;
    constexpr int kLetTheWorkerEndMs = 300;

    class HeldOplock
    {
    public:
        HeldOplock(const std::filesystem::path& file, const std::chrono::milliseconds valve)
            : handle_(CreateFileW(file.c_str(),
                                  GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED,
                                  nullptr)),
              event_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
        {
            overlapped_.hEvent = event_;
            input_.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION;
            input_.StructureLength = sizeof(input_);
            input_.RequestedOplockLevel =
                OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE | OPLOCK_LEVEL_CACHE_HANDLE;
            input_.Flags = REQUEST_OPLOCK_INPUT_FLAG_REQUEST;
            output_.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION;
            output_.StructureLength = sizeof(output_);

            if (handle_ == INVALID_HANDLE_VALUE)
            {
                error_ = GetLastError();
            }
            else
            {
                const BOOL asked = DeviceIoControl(handle_, FSCTL_REQUEST_OPLOCK, &input_, sizeof(input_), &output_,
                                                   sizeof(output_), nullptr, &overlapped_);
                error_ = GetLastError();
                held_ = asked == FALSE && error_ == ERROR_IO_PENDING;
            }

            valve_ = std::jthread(
                [this, valve](const std::stop_token& stop)
                {
                    const auto until = std::chrono::steady_clock::now() + valve;
                    while (!stop.stop_requested() && std::chrono::steady_clock::now() < until)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds{20});
                    }

                    Release();
                });
        }

        HeldOplock(const HeldOplock&) = delete;
        HeldOplock& operator=(const HeldOplock&) = delete;

        ~HeldOplock()
        {
            valve_.request_stop();
            valve_.join();
            Release();
        }

        [[nodiscard]] bool Held() const
        {
            return held_;
        }

        [[nodiscard]] QString Explained() const
        {
            return QStringLiteral("the oplock was not granted, Windows error %1").arg(error_);
        }

        void Release()
        {
            const std::scoped_lock lock(guard_);
            if (released_)
            {
                return;
            }

            released_ = true;
            if (handle_ != INVALID_HANDLE_VALUE)
            {
                CloseHandle(handle_);
                if (held_)
                {
                    WaitForSingleObject(event_, 1000);
                }
            }
            CloseHandle(event_);
        }

    private:
        std::mutex guard_{};
        HANDLE handle_;
        HANDLE event_;
        OVERLAPPED overlapped_{};
        REQUEST_OPLOCK_INPUT_BUFFER input_{};
        REQUEST_OPLOCK_OUTPUT_BUFFER output_{};
        bool held_ = false;
        bool released_ = false;
        DWORD error_ = 0;
        std::jthread valve_{};
    };

    [[nodiscard]] bool MakeAnEmptyFile(const std::filesystem::path& file)
    {
        const HANDLE handle =
            CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        CloseHandle(handle);

        return true;
    }

    constexpr std::wstring_view kLoneHigh = L"\xD800";
    constexpr std::wstring_view kLoneLow = L"\xDC00";
}

void FolderArchiveReaderTest::AFolderThatIsThePackageIsListedUnderItsOwnName()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;

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

void FolderArchiveReaderTest::AFolderWithTwoPackagesInsideListsBothUnderTheFolderName()
{
    const Disk disk;
    const std::filesystem::path envelope = disk.Root() / "Envelope";
    MakeThePackage(envelope / "alpha");
    MakeThePackage(envelope / "deeper" / "beta");
    WriteFile(envelope / "readme.txt", "read me");
    const FolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(envelope);

    QCOMPARE(listing.failure, ArchiveFailure::None);

    const WhatAnArchiveHolds holds = PackagesIn(listing.entries);

    QCOMPARE(holds.refusal, NothingToInstall::None);
    QCOMPARE(holds.packages.size(), std::size_t{2});
    QCOMPARE(EntryPathOf(holds.packages[0]), std::string{"Envelope/alpha"});
    QCOMPARE(EntryPathOf(holds.packages[1]), std::string{"Envelope/deeper/beta"});
}

void FolderArchiveReaderTest::ATrailingSeparatorDoesNotChangeTheListing()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;

    const ArchiveListing plain = reader.EntriesOf(disk.Dropped());
    const ArchiveListing slashed = reader.EntriesOf(disk.Dropped() / "");

    QCOMPARE(slashed.failure, ArchiveFailure::None);
    QCOMPARE(PathsOf(slashed), PathsOf(plain));
    QVERIFY(!slashed.entries.empty());
}

void FolderArchiveReaderTest::AFolderPastTheOldPathCeilingIsListedAndCopied()
{
    const Disk disk;
    const std::filesystem::path package = disk.Root() / "Deep";
    const std::filesystem::path deepFolder = FolderPastTheCeiling(package / "scenery", "world");
    WriteFilePastTheCeiling(deepFolder / "a.bgl", std::string(300, 'd'));
    WriteFile(package / "manifest.json", kManifest);
    const FolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(package);

    QCOMPARE(listing.failure, ArchiveFailure::None);

    const auto deepFile = std::ranges::find_if(listing.entries,
                                               [](const ArchiveEntry& entry)
                                               {
                                                   return entry.path.ends_with("/world/a.bgl");
                                               });

    QVERIFY(deepFile != listing.entries.end());
    QVERIFY(deepFile->path.size() > kOldPathCeiling - 60);
    QCOMPARE(deepFile->bytes, std::uintmax_t{300});

    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(package, listing.entries, staging, kKeepGoing), ArchiveFailure::None);
    QCOMPARE(ContentsOfFile(BeyondTheCeiling(staging / PathFromUtf8(deepFile->path))), std::string(300, 'd'));
}

void FolderArchiveReaderTest::AJunctionInsideMakesTheListingCarryAnOtherEntryAndNeverEntersIt()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const std::filesystem::path elsewhere = disk.Root() / "elsewhere";
    WriteFile(elsewhere / "secret.txt", "not part of the folder");
    WindowsLinkService links;
    QCOMPARE(links.CreateLink(disk.Dropped() / "scenery" / "shortcut", elsewhere, LinkType::Junction),
             LinkFailure::None);
    const FolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::None);

    const QStringList paths = PathsOf(listing);

    QVERIFY(paths.contains(Line(kPackage + "/scenery/shortcut", ArchiveEntryKind::Other, 0)));
    QVERIFY(std::ranges::none_of(listing.entries,
                                 [](const ArchiveEntry& entry)
                                 {
                                     return entry.path.find("secret.txt") != std::string::npos;
                                 }));
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
    QVERIFY(std::filesystem::exists(elsewhere / "secret.txt"));
}

void FolderArchiveReaderTest::AFolderThatIsItselfAJunctionCouldNotBeOpened()
{
    const Disk disk;
    const std::filesystem::path real = disk.Root() / "real";
    MakeThePackage(real / PathFromUtf8(kPackage));
    const std::filesystem::path linked = disk.Root() / "linked";
    WindowsLinkService links;
    QCOMPARE(links.CreateLink(linked, real / PathFromUtf8(kPackage), LinkType::Junction), LinkFailure::None);
    const FolderArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(linked);

    QCOMPARE(listing.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(listing.entries.empty());
    QVERIFY(std::filesystem::exists(real / PathFromUtf8(kPackage) / "manifest.json"));
}

void FolderArchiveReaderTest::AFileAMissingPathAndADriveRootCouldNotBeOpened()
{
    const Disk disk;
    WriteFile(disk.Root() / "plain.txt", "a file");
    const FolderArchiveReader reader;

    QCOMPARE(reader.EntriesOf(disk.Root() / "plain.txt").failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(disk.Root() / "missing").failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(disk.Root().root_path()).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(std::filesystem::path{}).failure, ArchiveFailure::CouldNotOpen);
}

void FolderArchiveReaderTest::AScanPastTheTimeLimitTookTooLong()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader(std::chrono::milliseconds{0});

    const ArchiveListing listing = reader.EntriesOf(disk.Dropped());

    QCOMPARE(listing.failure, ArchiveFailure::TookTooLong);
    QVERIFY(listing.entries.empty());
}

void FolderArchiveReaderTest::AScanWithinTheTimeLimitSucceeds()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader(std::chrono::minutes{5});

    QCOMPARE(reader.EntriesOf(disk.Dropped()).failure, ArchiveFailure::None);
    QCOMPARE(FolderArchiveReader{}.EntriesOf(disk.Dropped()).failure, ArchiveFailure::None);
}

void FolderArchiveReaderTest::TheManifestIsReadFromUnderTheFolderWithoutItsFirstSegment()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;

    const ArchiveText text = reader.ContentsOf(disk.Dropped(), kPackage + "/manifest.json", 1024 * 1024);

    QCOMPARE(text.failure, ArchiveFailure::None);
    QCOMPARE(text.text, kManifest);

    const ArchiveText deeper = reader.ContentsOf(disk.Dropped(), kPackage + "/scenery/world/a.bgl", 1024);

    QCOMPARE(deeper.text, std::string(1024, 'b'));
}

void FolderArchiveReaderTest::TheReadStopsAtTheCapAndAMissingEntryCouldNotBeOpened()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;

    const ArchiveText capped = reader.ContentsOf(disk.Dropped(), kPackage + "/manifest.json", 5);

    QCOMPARE(capped.failure, ArchiveFailure::None);
    QCOMPARE(capped.text, kManifest.substr(0, 5));
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/nothing.json", 100).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/scenery", 100).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage, 100).failure, ArchiveFailure::CouldNotOpen);
}

void FolderArchiveReaderTest::AnEntryThatEscapesTheFolderCouldNotBeOpened()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "downloads" / "outside.txt", "outside");
    const FolderArchiveReader reader;

    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/../outside.txt", 100).failure,
             ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(disk.Dropped(), kPackage + "/C:/Windows/win.ini", 100).failure,
             ArchiveFailure::CouldNotOpen);
}

void FolderArchiveReaderTest::OnlyWhatWasListedIsCopiedByteForByteAndTheSourceStaysTheSame()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "not-listed.txt", "appeared later");
    const FolderArchiveReader reader;
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

void FolderArchiveReaderTest::TheProgressReachesTheTotalOfTheListedFiles()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "big.bin", std::string(3 * 1024 * 1024, 'q'));
    const FolderArchiveReader reader;
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
    QCOMPARE(seen.back().copiedBytes, BytesOfTheFiles(listed));
    QCOMPARE(seen.back().totalBytes, BytesOfTheFiles(listed));
    QVERIFY(std::ranges::all_of(seen,
                                [&listed](const CopyProgress& progress)
                                {
                                    return progress.totalBytes == BytesOfTheFiles(listed);
                                }));
    QVERIFY(std::ranges::is_sorted(seen, {}, &CopyProgress::copiedBytes));
}

void FolderArchiveReaderTest::StoppingInTheMiddleOfABigFileCancelsAndLeavesTheSourceAlone()
{
    const Disk disk;
    WriteFile(disk.Dropped() / "manifest.json", kManifest);
    WriteFile(disk.Dropped() / "big.bin", std::string(kBigFileBytes, 'q'));
    const FolderArchiveReader reader;
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

    const std::filesystem::path landed = staging / disk.Dropped().filename() / "big.bin";
    std::error_code error;
    const std::uintmax_t landedBytes =
        std::filesystem::exists(landed, error) ? std::filesystem::file_size(landed, error) : 0;

    QVERIFY(landedBytes < kBigFileBytes);
    QCOMPARE(SnapshotOf(disk.Dropped()), before);
}

void FolderArchiveReaderTest::ACopyIntoAFolderThatIsNotAnEmptyStagingFolderWritesNothing()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;
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

void FolderArchiveReaderTest::AListedEntryOutsideTheFolderIsNotCopied()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "downloads" / "sibling.txt", "next to the folder");
    const FolderArchiveReader reader;
    std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    listed.push_back(ArchiveEntry{.path = "sibling.txt", .bytes = 18, .kind = ArchiveEntryKind::File});
    listed.push_back(ArchiveEntry{.path = kPackage + "/../sibling.txt", .bytes = 18, .kind = ArchiveEntryKind::File});
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(!std::filesystem::exists(staging / "sibling.txt"));
    QVERIFY(!std::filesystem::exists(disk.Root() / "sibling.txt"));
}

void FolderArchiveReaderTest::AListedEntryUnderAnotherFolderNameIsNotCopiedFromThisOne()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;
    std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    listed.push_back(ArchiveEntry{.path = "Another/manifest.json", .bytes = 1, .kind = ArchiveEntryKind::File});
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(std::filesystem::is_empty(staging));
}

void FolderArchiveReaderTest::AFileThatVanishedAfterTheListingCouldNotBeExtracted()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    std::filesystem::remove(disk.Dropped() / "layout.json");
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
}

void FolderArchiveReaderTest::AFolderIsHandedToTheFolderReaderAndAFileToTheArchiveReader()
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

void FolderArchiveReaderTest::AJunctionFolderIsHandedToTheFolderReader()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WindowsLinkService links;
    QCOMPARE(links.CreateLink(disk.Root() / "linked", disk.Dropped(), LinkType::Junction), LinkFailure::None);
    const ScriptedReader folders("folders");
    const ScriptedReader archives("archives");
    const ArchiveOrFolderReader reader(folders, archives);

    static_cast<void>(reader.EntriesOf(disk.Root() / "linked"));

    QCOMPARE(folders.calls, (QStringList{"folders:list"}));
    QVERIFY(archives.calls.isEmpty());
}

void FolderArchiveReaderTest::EveryOperationFollowsTheKindOfTheSource()
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

void FolderArchiveReaderTest::ANameWithALoneSurrogateInsideIsListedAsOtherAndNothingThrows()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const std::filesystem::path oddFile = disk.Dropped() / (std::wstring(kLoneHigh) + L".txt");
    const std::filesystem::path oddFolder = disk.Dropped() / (std::wstring(kLoneLow) + L"dir");
    QVERIFY(MakeAnEmptyFile(oddFile));
    QVERIFY(CreateDirectoryW(oddFolder.c_str(), nullptr) != FALSE);
    QVERIFY(MakeAnEmptyFile(oddFolder / L"inside.txt"));
    const FolderArchiveReader reader;

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

void FolderArchiveReaderTest::AFolderNamedWithALoneSurrogateCouldNotBeOpened()
{
    const Disk disk;
    const std::filesystem::path odd = disk.Root() / (std::wstring(kLoneHigh) + L"pack");
    QVERIFY(CreateDirectoryW(odd.c_str(), nullptr) != FALSE);
    WriteFile(odd / "manifest.json", kManifest);
    const FolderArchiveReader reader;
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.EntriesOf(odd).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(odd, "pack/manifest.json", 1024).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ExtractAll(odd, {}, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(std::filesystem::is_empty(staging));
}

void FolderArchiveReaderTest::ACopyBlockedByAnOplockIsGivenUpOnceTheIdleLimitPasses()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader(kFolderTimeLimit, kShortIdle);
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    HeldOplock owner(disk.Dropped() / "manifest.json", kValve);
    QVERIFY2(owner.Held(), qPrintable(owner.Explained()));
    QElapsedTimer clock;
    clock.start();

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing);
    const qint64 waited = clock.elapsed();
    owner.Release();
    QTest::qSleep(kLetTheWorkerEndMs);

    QCOMPARE(failure, ArchiveFailure::TookTooLong);
    QVERIFY2(waited < kBlockedBoundMs, qPrintable(QStringLiteral("waited %1 ms for the owner").arg(waited)));
}

void FolderArchiveReaderTest::CancellingWhileACopyIsBlockedReturnsCancelledWithoutWaitingForTheOwner()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    HeldOplock owner(disk.Dropped() / "manifest.json", kValve);
    QVERIFY2(owner.Held(), qPrintable(owner.Explained()));
    QElapsedTimer clock;
    clock.start();

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging,
                                                     [&clock](const CopyProgress&)
                                                     {
                                                         return clock.elapsed() < kCancelAfterMs;
                                                     });
    const qint64 waited = clock.elapsed();
    owner.Release();
    QTest::qSleep(kLetTheWorkerEndMs);

    QCOMPARE(failure, ArchiveFailure::Cancelled);
    QVERIFY2(waited < kBlockedBoundMs, qPrintable(QStringLiteral("waited %1 ms for the owner").arg(waited)));
}

void FolderArchiveReaderTest::ARealCopyWithAShortIdleLimitStillEndsInNoneWithTheSameTree()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "big.bin", std::string(3 * 1024 * 1024, 'q'));
    const FolderArchiveReader reader(kFolderTimeLimit, std::chrono::milliseconds{300});
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());

    const ArchiveFailure failure = reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing);

    QCOMPARE(failure, ArchiveFailure::None);
    QCOMPARE(SnapshotOf(staging / PathFromUtf8(kPackage)), SnapshotOf(disk.Dropped()));
}

void FolderArchiveReaderTest::NobodyListeningStillCopiesTheFolder()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    const FolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, {}), ArchiveFailure::None);
    QCOMPARE(SnapshotOf(staging / PathFromUtf8(kPackage)), SnapshotOf(disk.Dropped()));
}

void FolderArchiveReaderTest::TheProgressIsOnlyEverReportedOnTheCallingThread()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Dropped() / "big.bin", std::string(3 * 1024 * 1024, 'q'));
    const FolderArchiveReader reader;
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

void FolderArchiveReaderTest::AFileSwappedForALinkAfterTheListingIsNotFollowed()
{
    const Disk disk;
    MakeThePackage(disk.Dropped());
    WriteFile(disk.Root() / "outside.txt", "content from outside the folder");
    const FolderArchiveReader reader;
    const std::vector<ArchiveEntry> listed = ListedBy(reader, disk.Dropped());
    const std::filesystem::path staging = MakeStaging(disk.Root());
    std::filesystem::remove(disk.Dropped() / "layout.json");

    const BOOL linked =
        CreateSymbolicLinkW((disk.Dropped() / "layout.json").c_str(), (disk.Root() / "outside.txt").c_str(),
                            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
    if (linked == FALSE)
    {
        QSKIP(qPrintable(QStringLiteral("no symbolic link here, Windows error %1: the guard was not exercised")
                             .arg(GetLastError())));
    }
    qInfo() << "a symbolic link was created: the guard is exercised";

    QCOMPARE(reader.ExtractAll(disk.Dropped(), listed, staging, kKeepGoing), ArchiveFailure::CouldNotExtract);
    QVERIFY(!std::filesystem::exists(staging / PathFromUtf8(kPackage) / "layout.json"));
}

QTEST_APPLESS_MAIN(FolderArchiveReaderTest)

#include "tst_folder_archive_reader.moc"
