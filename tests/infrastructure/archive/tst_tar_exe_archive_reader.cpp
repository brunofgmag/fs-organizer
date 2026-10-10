#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <fstream>
#include <string_view>
#include <system_error>

#include "domain/importing/ImportPaths.h"
#include "infrastructure/archive/TarExeArchiveReader.h"
#include "support/PathText.h"
#include "tests/support/DeepPaths.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    constexpr int kTarTimeoutMs = 60000;
    constexpr std::size_t kBigFileBytes = 300000;
    constexpr std::chrono::milliseconds kShortLimit{300};

    [[nodiscard]] std::string Utf8Of(const std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    const std::string kPackage = Utf8Of(u8"[RR WL] pacote ação");
    const std::string kManifest = Utf8Of(u8"{\"title\":\"Pacote ação\"}");
    const std::string kSmall = "hello";
    const std::string kBig =
        std::string(kBigFileBytes / 10, '\x01') + std::string(kBigFileBytes - kBigFileBytes / 10, 'z');

    [[nodiscard]] QString Described(const ArchiveEntry& entry)
    {
        return QStringLiteral("%1 | %2 | %3")
            .arg(QString::fromUtf8(entry.path.data(), static_cast<qsizetype>(entry.path.size())))
            .arg(static_cast<int>(entry.kind))
            .arg(entry.bytes);
    }

    [[nodiscard]] QStringList DescribedAll(std::vector<ArchiveEntry> entries)
    {
        std::ranges::sort(entries, {}, &ArchiveEntry::path);

        QStringList lines;
        for (const ArchiveEntry& entry : entries)
        {
            lines << Described(entry);
        }

        return lines;
    }

    [[nodiscard]] QStringList Expected()
    {
        return DescribedAll({
            {.path = kPackage, .bytes = 0, .kind = ArchiveEntryKind::Folder},
            {.path = kPackage + "/empty", .bytes = 0, .kind = ArchiveEntryKind::Folder},
            {.path = kPackage + "/leia me.txt", .bytes = kSmall.size(), .kind = ArchiveEntryKind::File},
            {.path = kPackage + "/manifest.json", .bytes = kManifest.size(), .kind = ArchiveEntryKind::File},
            {.path = kPackage + "/sub", .bytes = 0, .kind = ArchiveEntryKind::Folder},
            {.path = kPackage + "/sub/big file.bin", .bytes = kBig.size(), .kind = ArchiveEntryKind::File},
        });
    }

    void WriteFile(const std::filesystem::path& file, const std::string& content)
    {
        std::filesystem::create_directories(file.parent_path());

        std::ofstream stream(file, std::ios::binary);
        stream << content;
    }

    [[nodiscard]] int RunTar(const QStringList& arguments, const std::filesystem::path& from)
    {
        QProcess process;
        process.setWorkingDirectory(AsText(from));
        process.start(AsText(SystemTarTool()), arguments);
        process.closeWriteChannel();
        process.waitForFinished(kTarTimeoutMs);

        return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
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

    struct Source
    {
        QTemporaryDir directory;
        bool packed = false;

        Source()
        {
            const std::filesystem::path package = Tree() / PathFromUtf8(kPackage);
            WriteFile(package / "manifest.json", kManifest);
            WriteFile(package / "leia me.txt", kSmall);
            WriteFile(package / "sub" / "big file.bin", kBig);
            std::filesystem::create_directories(package / "empty");

            packed =
                RunTar({QStringLiteral("-a"), QStringLiteral("-cf"), AsText(Zip()), QStringLiteral("-C"),
                        AsText(Tree()), QString::fromUtf8(kPackage.data(), static_cast<qsizetype>(kPackage.size()))},
                       Root())
                == 0;
        }

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
        }

        [[nodiscard]] std::filesystem::path Tree() const
        {
            return Root() / "tree";
        }

        [[nodiscard]] std::filesystem::path Zip() const
        {
            return Root() / "pack.zip";
        }

        [[nodiscard]] std::filesystem::path Staging() const
        {
            return StagingPathFor(Root() / "target");
        }

        [[nodiscard]] std::filesystem::path EmptyStaging() const
        {
            const std::filesystem::path staging = Staging();
            std::filesystem::create_directories(staging);

            return staging;
        }

        [[nodiscard]] std::filesystem::path Spinner(const int turns, const std::string& announcement = {}) const
        {
            const std::filesystem::path script = Root() / "spin.cmd";
            const std::string announce = announcement.empty() ? std::string() : "echo " + announcement + "\r\n";
            WriteFile(script,
                      "@echo off\r\n" + announce + "for /L %%i in (1,1," + std::to_string(turns) + ") do @rem\r\n"
                          + "echo done> \"" + AsText(SpinnerFinished()).toStdString() + "\"\r\n");

            return script;
        }

        [[nodiscard]] std::filesystem::path SpinnerFinished() const
        {
            return Root() / "spinner-finished.marker";
        }

        [[nodiscard]] std::filesystem::path Script(const std::string& body) const
        {
            const std::filesystem::path script = Root() / "tool.cmd";
            WriteFile(script, "@echo off\r\n" + body);

            return script;
        }
    };

    constexpr int kSpinTurns = 40000000;
    constexpr int kWriteTurns = 150000;
    constexpr std::chrono::milliseconds kShortIdle{500};
    constexpr qint64 kIdleKillBoundMs = 20000;

    const std::string kRanACommand = "ran-a-command";

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

    [[nodiscard]] std::filesystem::path ZipOfAFolderNamed(const Source& source, const std::string& folder)
    {
        const std::filesystem::path tree = source.Root() / "odd-tree";
        WriteFile(tree / PathFromUtf8(folder) / "manifest.json", kManifest);

        const std::filesystem::path zip = source.Root() / "odd.zip";
        const int made =
            RunTar({QStringLiteral("-a"), QStringLiteral("-cf"), AsText(zip), QStringLiteral("-C"), AsText(tree),
                    QStringLiteral("--"), QString::fromUtf8(folder.data(), static_cast<qsizetype>(folder.size()))},
                   source.Root());

        return made == 0 ? zip : std::filesystem::path{};
    }

    void AppendLittleEndian(std::string& into, const std::uint32_t value, const int width)
    {
        for (int shift = 0; shift < width * 8; shift += 8)
        {
            into.push_back(static_cast<char>((value >> shift) & 0xFFU));
        }
    }

    [[nodiscard]] std::uint32_t Crc32Of(const std::string& content)
    {
        std::uint32_t crc = 0xFFFFFFFFU;
        for (const char letter : content)
        {
            crc ^= static_cast<unsigned char>(letter);
            for (int bit = 0; bit < 8; ++bit)
            {
                crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
            }
        }

        return ~crc;
    }

    struct Member
    {
        std::string name{};
        std::string content{};
    };

    [[nodiscard]] std::string ZipOf(const std::vector<Member>& members)
    {
        std::string local;
        std::string central;
        for (const Member& member : members)
        {
            const std::string& name = member.name;
            const std::string& content = member.content;
            const std::uint32_t crc = Crc32Of(content);
            const auto size = static_cast<std::uint32_t>(content.size());
            const auto offset = static_cast<std::uint32_t>(local.size());

            AppendLittleEndian(local, 0x04034B50U, 4);
            AppendLittleEndian(local, 20, 2);
            AppendLittleEndian(local, 0, 2);
            AppendLittleEndian(local, 0, 2);
            AppendLittleEndian(local, 0, 2);
            AppendLittleEndian(local, 0x5821, 2);
            AppendLittleEndian(local, crc, 4);
            AppendLittleEndian(local, size, 4);
            AppendLittleEndian(local, size, 4);
            AppendLittleEndian(local, static_cast<std::uint32_t>(name.size()), 2);
            AppendLittleEndian(local, 0, 2);
            local += name;
            local += content;

            AppendLittleEndian(central, 0x02014B50U, 4);
            AppendLittleEndian(central, 20, 2);
            AppendLittleEndian(central, 20, 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0x5821, 2);
            AppendLittleEndian(central, crc, 4);
            AppendLittleEndian(central, size, 4);
            AppendLittleEndian(central, size, 4);
            AppendLittleEndian(central, static_cast<std::uint32_t>(name.size()), 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0, 2);
            AppendLittleEndian(central, 0, 4);
            AppendLittleEndian(central, offset, 4);
            central += name;
        }

        std::string end;
        AppendLittleEndian(end, 0x06054B50U, 4);
        AppendLittleEndian(end, 0, 2);
        AppendLittleEndian(end, 0, 2);
        AppendLittleEndian(end, static_cast<std::uint32_t>(members.size()), 2);
        AppendLittleEndian(end, static_cast<std::uint32_t>(members.size()), 2);
        AppendLittleEndian(end, static_cast<std::uint32_t>(central.size()), 4);
        AppendLittleEndian(end, static_cast<std::uint32_t>(local.size()), 4);
        AppendLittleEndian(end, 0, 2);

        return local + central + end;
    }

    [[nodiscard]] std::string
    ZipWithTheSameNameTwice(const std::string& name, const std::string& first, const std::string& second)
    {
        return ZipOf({{.name = name, .content = first}, {.name = name, .content = second}});
    }

    [[nodiscard]] QString PrintedByTar(const std::filesystem::path& inside)
    {
        QString printed = QStringLiteral("\\\\?\\") + AsText(inside);
        printed.replace(u'\\', QStringLiteral("\\\\"));

        return printed;
    }

    [[nodiscard]] std::filesystem::path StagingInsideAFolderNamed(const Source& source, const std::string& folder)
    {
        const std::filesystem::path staging = StagingPathFor(source.Root() / PathFromUtf8(folder) / "target");
        std::filesystem::create_directories(staging);

        return staging;
    }

    constexpr int kHugeArchive = 50000;
    constexpr qint64 kSlowestAcceptableMs = 5000;

    struct TimedAnswer
    {
        bool asks = false;
        qint64 milliseconds = 0;
    };

    [[nodiscard]] QStringList EntryNames(const QString& folder, const int count)
    {
        QStringList names;
        names.reserve(count);
        for (int at = 0; at < count; ++at)
        {
            names << QStringLiteral("scenery/%1-%2/file-%2.bgl").arg(folder).arg(at);
        }

        return names;
    }

    [[nodiscard]] QString OneLinePerName(const QStringList& names, const QString& reason)
    {
        QStringList lines;
        lines.reserve(names.size());
        for (const QString& name : names)
        {
            lines << QStringLiteral("tar: '%1': %2").arg(name, reason);
        }

        return lines.join(QStringLiteral("\r\n"));
    }

    [[nodiscard]] QString RepeatedLine(const QString& line, const qsizetype times)
    {
        return QStringList(times, line).join(QStringLiteral("\r\n"));
    }

    [[nodiscard]] TimedAnswer TimedAsk(const QString& message, const QStringList& names)
    {
        QElapsedTimer clock;
        clock.start();
        const bool asks = AsksForAPassword(message, names);

        return {.asks = asks, .milliseconds = clock.elapsed()};
    }

    [[nodiscard]] QString SlowReport(const TimedAnswer& answer)
    {
        return QStringLiteral("took %1 ms").arg(answer.milliseconds);
    }

    [[nodiscard]] std::function<bool(const CopyProgress&)> Recording(std::vector<CopyProgress>& seen)
    {
        return [&seen](const CopyProgress& progress)
        {
            seen.push_back(progress);

            return true;
        };
    }

    class TarExeArchiveReaderTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheListingOfAZipCarriesPathsSizesAndKindsWithoutTrailingSlashes();
        static void TheFourRealLinesBecomeEntriesWhateverTheMonthIsCalled();
        static void ARarFolderWithoutATrailingSlashIsStillAFolder();
        static void ALeadingDotSlashAndBackslashesAreNormalisedAndTheRootEntryIsDropped();
        static void ALinkLineIsOtherAndKeepsOnlyItsOwnName();
        static void ALinkWhoseOwnNameHasAnArrowKeepsItWhole();
        static void TheDateColumnsCanBeWrittenAnyWayAtAll();
        static void NamesAndDetailsOfDifferentCountsAreRefused();
        static void ALineThatDoesNotParseMakesTheWholeListingFail();
        static void AnEmptyPrintoutIsAnEmptyListing();
        static void TheBracketedAccentedEntryIsReadByItsLiteralName();
        static void MostCutsTheBytesAndAnEntryBiggerThanMostIsNotReadToTheEnd();
        static void AnEntryThatIsNotThereCouldNotBeOpened();
        static void AFolderThatStartsWithAHyphenIsReadByItsName();
        static void AFolderNamedLikeAnOptionIsReadByItsNameAndRunsNothing();
        static void ExtractionReproducesTheTreeAndProgressEndsAtTheTotal();
        static void ProgressNeverGoesBackwardsAndStartsBelowTheTotal();
        static void NobodyListeningStillExtracts();
        static void AnEntryRepeatedInTheArchiveNeverOverwritesTheFirstCopy();
        static void AnEntryBiggerThanMostIsNotWaitedForToTheEnd();
        static void CancellingFromTheFirstProgressCallLeavesNothingHoldingTheFolder();
        static void TheProgressCallbackIsCalledEvenWhenNoLineArrives();
        static void AFolderThatIsNotEmptyIsRefusedAndKeepsItsContent();
        static void AFolderThatIsNotAStagingPathIsRefusedAndStaysEmpty();
        static void AFolderThatIsNotThereIsRefusedAndIsNotCreated();
        static void AFileGivenAsTheFolderIsRefused();
        static void ATextFileIsNotAnArchive();
        static void AMissingArchiveCouldNotBeOpened();
        static void AToolThatDoesNotExistIsMissingFromEveryMethod();
        static void AToolThatRunsPastTheLimitIsKilledAndTookTooLong();
        static void APasswordProtectedZipListsButNeedsAPasswordToReadOrExtract();
        static void AMemberNamedPasswordInAnEncryptedZipStillNeedsAPassword();
        static void AMemberNamedPasswordIsNotAskedForAPasswordWhenItsExtractionFails();
        static void AnEntryNamedPasswordThatIsNotThereCouldNotBeOpened();
        static void AnExtractionThatReadsAndWritesNothingIsKilledAndTookTooLong();
        static void AnExtractionThatKeepsWorkingIsNeverKilledForTakingLong();
        static void ARealExtractionWithAShortIdleLimitStillEndsInNone();
        static void AnArchiveWithAnEncryptedHeaderNeedsAPasswordFromEveryMethod();
        static void ADamagedArchiveNamedPasswordIsStillCouldNotOpen();
        static void AnEmptyStagingFolderPastTheOldPathCeilingIsAcceptedByTheGuard();
        static void AStagingFolderPastTheOldPathCeilingIsNotReportedAsAMissingTool();
        static void AToolThatExistsButCannotStartIsNotReportedAsMissing();
        static void AMemberNamedPasswordThatFailsInAFolderNamedEncryptedCouldNotBeExtracted();
        static void AHugeFailureThatNeverMentionsAPasswordIsAnsweredNoQuickly();
        static void AHugeFailureCitingNamesWithTheWordForAnotherReasonIsAnsweredNoQuickly();
        static void AHugeRealRequestCitingInnocentNamesIsFoundQuickly();
        static void AHugeListOfInnocentNamesIsNotScannedAgainstLongRequestLines();
        static void ARealRequestAfterManyInnocentLinesIsStillFound();
        static void ANameWithTheWordCitedForAnotherReasonDoesNotAsk();
        static void ARealRequestCitingANameWithTheWordStillAsks();
        static void ARealRequestAmongTheLinesCheckedIsFound();
        static void ARealRequestPastTheLinesCheckedBecauseOfNamesIsNotSeen();
    };
}

void TarExeArchiveReaderTest::TheListingOfAZipCarriesPathsSizesAndKindsWithoutTrailingSlashes()
{
    const Source source;
    QVERIFY(source.packed);

    const ArchiveListing listing = TarExeArchiveReader().EntriesOf(source.Zip());

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries), Expected());
}

void TarExeArchiveReaderTest::TheFourRealLinesBecomeEntriesWhateverTheMonthIsCalled()
{
    const QString names = QString::fromUtf8(u8"tlc-bgco/ContentInfo/tlc-bgco/Thumbnail.jpg\r\n"
                                            u8"[RR WL] pacote ação/sub/big file.bin\r\n"
                                            u8"[RR WL] pacote ação/sub/\r\n"
                                            u8"tlc-bgco/scenery/mycompany/TEXTURE\r\n");
    const QString details = QString::fromUtf8(
        u8"-rw-r--r--  0 0      0       13520 set 21  2020 tlc-bgco/ContentInfo/tlc-bgco/Thumbnail.jpg\r\n"
        u8"-rw-rw-rw-  0 0      0      300000 out 09 01:32 [RR WL] pacote ação/sub/big file.bin\r\n"
        u8"drwxrwxrwx  0 0      0           0 out 09 01:32 [RR WL] pacote ação/sub/\r\n"
        u8"drwxr-xr-x  0 0      0           0 dez 11  2020 tlc-bgco/scenery/mycompany/TEXTURE\r\n");

    const ArchiveListing listing = ListingFrom(names, details);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(
        DescribedAll(listing.entries),
        DescribedAll({
            {.path = "tlc-bgco/ContentInfo/tlc-bgco/Thumbnail.jpg", .bytes = 13520, .kind = ArchiveEntryKind::File},
            {.path = kPackage + "/sub/big file.bin", .bytes = 300000, .kind = ArchiveEntryKind::File},
            {.path = kPackage + "/sub", .bytes = 0, .kind = ArchiveEntryKind::Folder},
            {.path = "tlc-bgco/scenery/mycompany/TEXTURE", .bytes = 0, .kind = ArchiveEntryKind::Folder},
        }));
}

void TarExeArchiveReaderTest::ARarFolderWithoutATrailingSlashIsStillAFolder()
{
    const ArchiveListing listing = ListingFrom(
        QStringLiteral("tlc-bgco/scenery/mycompany/TEXTURE\r\n"
                       "tlc-bgco/scenery/mycompany/TEXTURE/a b.dds\r\n"),
        QStringLiteral(
            "drwxr-xr-x  0 0      0           0 dez 11  2020 tlc-bgco/scenery/mycompany/TEXTURE\r\n"
            "-rw-r--r--  0 0      0         512 dez 11  2020 tlc-bgco/scenery/mycompany/TEXTURE/a b.dds\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(listing.entries.size(), std::size_t{2});
    QCOMPARE(listing.entries.front().path, std::string("tlc-bgco/scenery/mycompany/TEXTURE"));
    QCOMPARE(listing.entries.front().kind, ArchiveEntryKind::Folder);
    QCOMPARE(listing.entries.back().path, std::string("tlc-bgco/scenery/mycompany/TEXTURE/a b.dds"));
    QCOMPARE(listing.entries.back().kind, ArchiveEntryKind::File);
    QCOMPARE(listing.entries.back().bytes, std::uintmax_t{512});
}

void TarExeArchiveReaderTest::ALeadingDotSlashAndBackslashesAreNormalisedAndTheRootEntryIsDropped()
{
    const ArchiveListing listing =
        ListingFrom(QStringLiteral("./\r\n"
                                   "./a/b.txt\r\n"
                                   "c\\d.txt\r\n"),
                    QStringLiteral("drwxr-xr-x  0 0      0           0 out 09 01:32 ./\r\n"
                                   "-rw-r--r--  0 0      0          12 out 09 01:32 ./a/b.txt\r\n"
                                   "-rw-r--r--  0 0      0          14 out 09 01:32 c\\d.txt\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries),
             DescribedAll({
                 {.path = "a/b.txt", .bytes = 12, .kind = ArchiveEntryKind::File},
                 {.path = "c/d.txt", .bytes = 14, .kind = ArchiveEntryKind::File},
             }));
}

void TarExeArchiveReaderTest::ALinkLineIsOtherAndKeepsOnlyItsOwnName()
{
    const ArchiveListing listing =
        ListingFrom(QStringLiteral("pasta/atalho\r\n"),
                    QStringLiteral("lrwxrwxrwx  0 0      0           0 out 09 01:32 pasta/atalho -> ../alvo.txt\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(listing.entries.size(), std::size_t{1});
    QCOMPARE(listing.entries.front().path, std::string("pasta/atalho"));
    QCOMPARE(listing.entries.front().kind, ArchiveEntryKind::Other);
}

void TarExeArchiveReaderTest::ALinkWhoseOwnNameHasAnArrowKeepsItWhole()
{
    const ArchiveListing listing = ListingFrom(
        QStringLiteral("pasta/a -> b.txt\r\n"
                       "pasta/atalho x -> y\r\n"),
        QStringLiteral("-rw-r--r--  0 0      0          12 out 09 01:32 pasta/a -> b.txt\r\n"
                       "lrwxrwxrwx  0 0      0           0 out 09 01:32 pasta/atalho x -> y -> ../alvo.txt\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(listing.entries.size(), std::size_t{2});
    QCOMPARE(listing.entries.front().path, std::string("pasta/a -> b.txt"));
    QCOMPARE(listing.entries.front().kind, ArchiveEntryKind::File);
    QCOMPARE(listing.entries.back().path, std::string("pasta/atalho x -> y"));
    QCOMPARE(listing.entries.back().kind, ArchiveEntryKind::Other);
}

void TarExeArchiveReaderTest::TheDateColumnsCanBeWrittenAnyWayAtAll()
{
    const ArchiveListing listing =
        ListingFrom(QStringLiteral("um dois.jpg\r\n"
                                   "tres.txt\r\n"
                                   "quatro/\r\n"
                                   "cinco\r\n"),
                    QStringLiteral("-rw-r--r--  0 0      0       13520 21 setembro 2020 um dois.jpg\r\n"
                                   "-rw-r--r--  0 0      0         512 2020-09-21T01:32:00 tres.txt\r\n"
                                   "drwxr-xr-x  0 0      0           0 sept. 21  2020 quatro/\r\n"
                                   "-rw-r--r--  0 0      0           7\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries),
             DescribedAll({
                 {.path = "um dois.jpg", .bytes = 13520, .kind = ArchiveEntryKind::File},
                 {.path = "tres.txt", .bytes = 512, .kind = ArchiveEntryKind::File},
                 {.path = "quatro", .bytes = 0, .kind = ArchiveEntryKind::Folder},
                 {.path = "cinco", .bytes = 7, .kind = ArchiveEntryKind::File},
             }));
}

void TarExeArchiveReaderTest::NamesAndDetailsOfDifferentCountsAreRefused()
{
    const QString oneLine = QStringLiteral("-rw-r--r--  0 0      0          12 out 09 01:32 a.txt\r\n");
    const QString twoLines = oneLine + QStringLiteral("-rw-r--r--  0 0      0          14 out 09 01:32 b.txt\r\n");

    const ArchiveListing moreDetails = ListingFrom(QStringLiteral("a.txt\r\n"), twoLines);
    const ArchiveListing moreNames = ListingFrom(QStringLiteral("a.txt\r\nb.txt\r\n"), oneLine);

    QCOMPARE(moreDetails.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(moreDetails.entries.empty());
    QCOMPARE(moreNames.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(moreNames.entries.empty());
}

void TarExeArchiveReaderTest::ALineThatDoesNotParseMakesTheWholeListingFail()
{
    const ArchiveListing listing =
        ListingFrom(QStringLiteral("fine.txt\r\n"
                                   "this is not a listing\r\n"
                                   "also fine.txt\r\n"),
                    QStringLiteral("-rw-r--r--  0 0      0          12 out 09 01:32 fine.txt\r\n"
                                   "this is not a listing\r\n"
                                   "-rw-r--r--  0 0      0          12 out 09 01:32 also fine.txt\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(listing.entries.empty());
}

void TarExeArchiveReaderTest::AnEmptyPrintoutIsAnEmptyListing()
{
    const ArchiveListing listing = ListingFrom(QStringLiteral("\r\n"), QStringLiteral("\r\n"));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QVERIFY(listing.entries.empty());
}

void TarExeArchiveReaderTest::TheBracketedAccentedEntryIsReadByItsLiteralName()
{
    const Source source;
    QVERIFY(source.packed);

    const ArchiveText text = TarExeArchiveReader().ContentsOf(source.Zip(), kPackage + "/manifest.json", 1000);

    QCOMPARE(text.failure, ArchiveFailure::None);
    QCOMPARE(text.text, kManifest);
}

void TarExeArchiveReaderTest::MostCutsTheBytesAndAnEntryBiggerThanMostIsNotReadToTheEnd()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;

    const ArchiveText cut = reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 5);
    const ArchiveText head = reader.ContentsOf(source.Zip(), kPackage + "/sub/big file.bin", 10);
    const ArchiveText whole = reader.ContentsOf(source.Zip(), kPackage + "/sub/big file.bin", kBig.size() + 100);

    QCOMPARE(cut.failure, ArchiveFailure::None);
    QCOMPARE(cut.text, kManifest.substr(0, 5));
    QCOMPARE(head.failure, ArchiveFailure::None);
    QCOMPARE(head.text, kBig.substr(0, 10));
    QCOMPARE(whole.failure, ArchiveFailure::None);
    QVERIFY(whole.text == kBig);
}

void TarExeArchiveReaderTest::AnEntryThatIsNotThereCouldNotBeOpened()
{
    const Source source;
    QVERIFY(source.packed);

    const ArchiveText text = TarExeArchiveReader().ContentsOf(source.Zip(), kPackage + "/nao existe.txt", 1000);

    QCOMPARE(text.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(text.text.empty());
}

void TarExeArchiveReaderTest::AFolderThatStartsWithAHyphenIsReadByItsName()
{
    const Source source;
    const std::string folder = "-pacote";
    const std::filesystem::path zip = ZipOfAFolderNamed(source, folder);
    QVERIFY(!zip.empty());

    const ArchiveText text = TarExeArchiveReader().ContentsOf(zip, folder + "/manifest.json", 1000);

    QCOMPARE(text.failure, ArchiveFailure::None);
    QCOMPARE(text.text, kManifest);
}

void TarExeArchiveReaderTest::AFolderNamedLikeAnOptionIsReadByItsNameAndRunsNothing()
{
    const Source source;
    const std::string folder =
        "--use-compress-program=powershell -NoProfile -c New-Item " + kRanACommand + " -ItemType Directory;#";
    const std::filesystem::path zip = ZipOfAFolderNamed(source, folder);
    QVERIFY(!zip.empty());
    const std::filesystem::path where = source.Root() / "where-the-tool-runs";
    std::filesystem::create_directories(where);
    const InDirectory inside(where);

    const ArchiveText text = TarExeArchiveReader().ContentsOf(zip, folder + "/manifest.json", 1000);

    QVERIFY2(!std::filesystem::exists(where / kRanACommand), "the name of a member ran as a command");
    QCOMPARE(text.failure, ArchiveFailure::None);
    QCOMPARE(text.text, kManifest);
}

void TarExeArchiveReaderTest::ExtractionReproducesTheTreeAndProgressEndsAtTheTotal()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    QCOMPARE(listing.failure, ArchiveFailure::None);
    const std::filesystem::path into = source.EmptyStaging();
    std::vector<CopyProgress> seen;

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), listing.entries, into, Recording(seen));

    QCOMPARE(failure, ArchiveFailure::None);
    QCOMPARE(TreeOf(into), TreeOf(source.Tree()));
    QVERIFY(!seen.empty());
    QCOMPARE(seen.back().totalBytes, std::uintmax_t{kSmall.size() + kManifest.size() + kBig.size()});
    QCOMPARE(seen.back().copiedBytes, seen.back().totalBytes);
}

void TarExeArchiveReaderTest::ProgressNeverGoesBackwardsAndStartsBelowTheTotal()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    std::vector<CopyProgress> seen;

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, source.EmptyStaging(), Recording(seen)),
             ArchiveFailure::None);

    QVERIFY(seen.size() >= 2);
    QVERIFY(seen.front().copiedBytes < seen.front().totalBytes);
    QVERIFY(std::ranges::is_sorted(seen, {}, &CopyProgress::copiedBytes));
}

void TarExeArchiveReaderTest::AnEntryRepeatedInTheArchiveNeverOverwritesTheFirstCopy()
{
    const Source source;
    const std::filesystem::path twice = source.Root() / "twice.zip";
    WriteFile(twice, ZipWithTheSameNameTwice("a.txt", "first", "second!!"));
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(TarExeArchiveReader().ExtractAll(twice, {}, into, {}), ArchiveFailure::None);

    QCOMPARE(TreeOf(into), QStringList{QStringLiteral("a.txt | 5")});
}

void TarExeArchiveReaderTest::AnEntryBiggerThanMostIsNotWaitedForToTheEnd()
{
    const Source source;
    const TarExeArchiveReader reader(source.Spinner(kSpinTurns, "0123456789"));

    const ArchiveText text = reader.ContentsOf(source.Zip(), "anything", 3);

    QCOMPARE(text.failure, ArchiveFailure::None);
    QCOMPARE(text.text, std::string("012"));
    QVERIFY2(!std::filesystem::exists(source.SpinnerFinished()), "the tool was waited for to the end");
}

void TarExeArchiveReaderTest::NobodyListeningStillExtracts()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::None);

    QCOMPARE(TreeOf(into), TreeOf(source.Tree()));
}

void TarExeArchiveReaderTest::CancellingFromTheFirstProgressCallLeavesNothingHoldingTheFolder()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();
    int calls = 0;

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), listing.entries, into,
                                                     [&calls](const CopyProgress&)
                                                     {
                                                         ++calls;

                                                         return false;
                                                     });

    QCOMPARE(failure, ArchiveFailure::Cancelled);
    QCOMPARE(calls, 1);
    std::error_code error;
    std::filesystem::remove_all(into, error);
    QVERIFY(!error);
    QVERIFY(!std::filesystem::exists(into));
}

void TarExeArchiveReaderTest::TheProgressCallbackIsCalledEvenWhenNoLineArrives()
{
    const Source source;
    const TarExeArchiveReader reader(source.Spinner(kSpinTurns));
    const std::filesystem::path into = source.EmptyStaging();
    int calls = 0;

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), {}, into,
                                                     [&calls](const CopyProgress&)
                                                     {
                                                         return ++calls < 3;
                                                     });

    QCOMPARE(failure, ArchiveFailure::Cancelled);
    QCOMPARE(calls, 3);
    QVERIFY2(!std::filesystem::exists(source.SpinnerFinished()), "the tool was waited for to the end");
    std::error_code error;
    std::filesystem::remove_all(into, error);
    QVERIFY(!error);
}

void TarExeArchiveReaderTest::AFolderThatIsNotEmptyIsRefusedAndKeepsItsContent()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();
    WriteFile(into / "ja estava aqui.txt", "mine");
    const QStringList before = TreeOf(into);

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::CouldNotExtract);

    QCOMPARE(TreeOf(into), before);
}

void TarExeArchiveReaderTest::AFolderThatIsNotAStagingPathIsRefusedAndStaysEmpty()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.Root() / "target";
    std::filesystem::create_directories(into);

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(TreeOf(into).isEmpty());
}

void TarExeArchiveReaderTest::AFolderThatIsNotThereIsRefusedAndIsNotCreated()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, source.Staging(), {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(!std::filesystem::exists(source.Staging()));
}

void TarExeArchiveReaderTest::AFileGivenAsTheFolderIsRefused()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    WriteFile(source.Staging(), "");

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, source.Staging(), {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(std::filesystem::is_regular_file(source.Staging()));
}

void TarExeArchiveReaderTest::ATextFileIsNotAnArchive()
{
    const Source source;
    const std::filesystem::path text = source.Root() / "notes.txt";
    WriteFile(text, "just some words, nothing to unpack\n");
    const TarExeArchiveReader reader;

    QCOMPARE(reader.EntriesOf(text).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(text, "notes.txt", 100).failure, ArchiveFailure::CouldNotOpen);
}

void TarExeArchiveReaderTest::AMissingArchiveCouldNotBeOpened()
{
    const Source source;

    const ArchiveListing listing = TarExeArchiveReader().EntriesOf(source.Root() / "never-made.zip");

    QCOMPARE(listing.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(listing.entries.empty());
}

void TarExeArchiveReaderTest::AToolThatDoesNotExistIsMissingFromEveryMethod()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader(source.Root() / "no-such-tar.exe");

    QCOMPARE(reader.EntriesOf(source.Zip()).failure, ArchiveFailure::TheToolIsMissing);
    QCOMPARE(reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 10).failure,
             ArchiveFailure::TheToolIsMissing);
    QCOMPARE(reader.ExtractAll(source.Zip(), {}, source.EmptyStaging(), {}), ArchiveFailure::TheToolIsMissing);
}

void TarExeArchiveReaderTest::AToolThatRunsPastTheLimitIsKilledAndTookTooLong()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader(source.Spinner(kSpinTurns), kShortLimit);

    QCOMPARE(reader.EntriesOf(source.Zip()).failure, ArchiveFailure::TookTooLong);
    QCOMPARE(reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 10).failure, ArchiveFailure::TookTooLong);
}

void TarExeArchiveReaderTest::APasswordProtectedZipListsButNeedsAPasswordToReadOrExtract()
{
    const Source source;
    const std::filesystem::path locked = source.Root() / "locked.zip";
    const int made = RunTar({QStringLiteral("-a"), QStringLiteral("-cf"), AsText(locked), QStringLiteral("--options"),
                             QStringLiteral("zip:encryption=zipcrypt"), QStringLiteral("--passphrase"),
                             QStringLiteral("secret"), QStringLiteral("-C"), AsText(source.Tree()),
                             QString::fromUtf8(kPackage.data(), static_cast<qsizetype>(kPackage.size()))},
                            source.Root());
    if (made != 0 || !std::filesystem::exists(locked))
    {
        QSKIP(qPrintable(QStringLiteral("this tar.exe could not create an encrypted zip, exit code %1").arg(made)));
    }
    const TarExeArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(locked);
    const ArchiveText text = reader.ContentsOf(locked, kPackage + "/manifest.json", 1000);
    const ArchiveFailure extracted = reader.ExtractAll(locked, listing.entries, source.EmptyStaging(), {});

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries), Expected());
    QCOMPARE(text.failure, ArchiveFailure::NeedsAPassword);
    QCOMPARE(extracted, ArchiveFailure::NeedsAPassword);
}

void TarExeArchiveReaderTest::AMemberNamedPasswordInAnEncryptedZipStillNeedsAPassword()
{
    const Source source;
    const std::filesystem::path tree = source.Root() / "locked-tree";
    WriteFile(tree / "password.txt", kSmall);
    const std::filesystem::path locked = source.Root() / "locked-by-name.zip";
    const int made =
        RunTar({QStringLiteral("-a"), QStringLiteral("-cf"), AsText(locked), QStringLiteral("--options"),
                QStringLiteral("zip:encryption=zipcrypt"), QStringLiteral("--passphrase"), QStringLiteral("secret"),
                QStringLiteral("-C"), AsText(tree), QStringLiteral("password.txt")},
               source.Root());
    if (made != 0 || !std::filesystem::exists(locked))
    {
        QSKIP(qPrintable(QStringLiteral("this tar.exe could not create an encrypted zip, exit code %1").arg(made)));
    }
    const TarExeArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(locked);
    const ArchiveFailure extracted = reader.ExtractAll(locked, listing.entries, source.EmptyStaging(), {});

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(extracted, ArchiveFailure::NeedsAPassword);
}

void TarExeArchiveReaderTest::AMemberNamedPasswordIsNotAskedForAPasswordWhenItsExtractionFails()
{
    const Source source;
    const std::filesystem::path into = source.EmptyStaging();
    const std::filesystem::path tool = source.Script("echo x ok.txt 1>&2\r\n"
                                                     "echo x password.txt: Can't create '"
                                                     + PrintedByTar(into / "password.txt").toStdString()
                                                     + "': No such file or directory 1>&2\r\n"
                                                       "echo x encrypt.dat 1>&2\r\n"
                                                       "echo tar.exe: Error exit delayed from previous errors 1>&2\r\n"
                                                       "exit /b 1\r\n");
    const TarExeArchiveReader reader(tool);
    const std::vector<ArchiveEntry> listed{
        {.path = "ok.txt", .bytes = 1, .kind = ArchiveEntryKind::File},
        {.path = "password.txt", .bytes = 1, .kind = ArchiveEntryKind::File},
        {.path = "encrypt.dat", .bytes = 1, .kind = ArchiveEntryKind::File},
    };

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), listed, into, {});

    QCOMPARE(failure, ArchiveFailure::CouldNotExtract);
}

void TarExeArchiveReaderTest::AnEntryNamedPasswordThatIsNotThereCouldNotBeOpened()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;

    const ArchiveText plain = reader.ContentsOf(source.Zip(), "password.txt", 1000);
    const ArchiveText bracketed = reader.ContentsOf(source.Zip(), "[password].txt", 1000);
    const ArchiveText inAFolder = reader.ContentsOf(source.Zip(), "encrypted/passphrase", 1000);

    QCOMPARE(plain.failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(bracketed.failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(inAFolder.failure, ArchiveFailure::CouldNotOpen);
}

void TarExeArchiveReaderTest::AnExtractionThatReadsAndWritesNothingIsKilledAndTookTooLong()
{
    const Source source;
    const TarExeArchiveReader reader(source.Spinner(kSpinTurns), kTarTimeLimit, kShortIdle);
    const std::filesystem::path into = source.EmptyStaging();
    QElapsedTimer clock;
    clock.start();

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), {}, into, {});

    QCOMPARE(failure, ArchiveFailure::TookTooLong);
    QVERIFY2(clock.elapsed() < kIdleKillBoundMs, "the idle tool was not stopped soon after its limit");
    QVERIFY2(!std::filesystem::exists(source.SpinnerFinished()), "the tool was waited for to the end");
    std::error_code error;
    std::filesystem::remove_all(into, error);
    QVERIFY(!error);
}

void TarExeArchiveReaderTest::AnExtractionThatKeepsWorkingIsNeverKilledForTakingLong()
{
    constexpr std::chrono::milliseconds idleLimit{800};
    const Source source;
    const std::filesystem::path tool = source.Script("for /L %%i in (1,1," + std::to_string(kWriteTurns)
                                                     + ") do @echo x file%%i 1>&2\r\n"
                                                       "exit /b 0\r\n");
    const TarExeArchiveReader reader(tool, kTarTimeLimit, idleLimit);
    QElapsedTimer clock;
    clock.start();

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), {}, source.EmptyStaging(), {});

    QCOMPARE(failure, ArchiveFailure::None);
    QVERIFY2(clock.elapsed() > idleLimit.count(), "the tool did not outlast the idle limit, so this proved nothing");
}

void TarExeArchiveReaderTest::ARealExtractionWithAShortIdleLimitStillEndsInNone()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader(SystemTarTool(), kTarTimeLimit, kShortIdle);
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), listing.entries, into, {});

    QCOMPARE(failure, ArchiveFailure::None);
    QCOMPARE(TreeOf(into), TreeOf(source.Tree()));
}

void TarExeArchiveReaderTest::AnArchiveWithAnEncryptedHeaderNeedsAPasswordFromEveryMethod()
{
    const Source source;
    const std::filesystem::path tool =
        source.Script("echo tar.exe: The archive header is encrypted, but currently not supported 1>&2\r\n"
                      "echo tar.exe: Error exit delayed from previous errors 1>&2\r\n"
                      "exit /b 1\r\n");
    const TarExeArchiveReader reader(tool);

    QCOMPARE(reader.EntriesOf(source.Zip()).failure, ArchiveFailure::NeedsAPassword);
    QCOMPARE(reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 10).failure, ArchiveFailure::NeedsAPassword);
}

void TarExeArchiveReaderTest::ADamagedArchiveNamedPasswordIsStillCouldNotOpen()
{
    const Source source;
    const std::filesystem::path damaged = source.Root() / "password.zip";
    WriteFile(damaged, "this is not an archive at all\n");
    const std::filesystem::path missing = source.Root() / "encrypted" / "passphrase.zip";
    const TarExeArchiveReader reader;

    QCOMPARE(reader.EntriesOf(damaged).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.EntriesOf(missing).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(damaged, "a.txt", 10).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(missing, "a.txt", 10).failure, ArchiveFailure::CouldNotOpen);
}

void TarExeArchiveReaderTest::AnEmptyStagingFolderPastTheOldPathCeilingIsAcceptedByTheGuard()
{
    const Source source;
    const std::filesystem::path deep = FolderPastTheCeiling(source.Root(), "target.fsorg-partial");
    QVERIFY(deep.wstring().size() > kOldPathCeiling);
    const TarExeArchiveReader reader(source.Script("exit /b 0\r\n"));

    QCOMPARE(reader.ExtractAll(source.Zip(), {}, deep, {}), ArchiveFailure::None);
}

void TarExeArchiveReaderTest::AStagingFolderPastTheOldPathCeilingIsNotReportedAsAMissingTool()
{
    const Source source;
    QVERIFY(source.packed);
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path deep = FolderPastTheCeiling(source.Root(), "target.fsorg-partial");

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), listing.entries, deep, {});

    QCOMPARE(failure, ArchiveFailure::CouldNotExtract);
}

void TarExeArchiveReaderTest::AToolThatExistsButCannotStartIsNotReportedAsMissing()
{
    const Source source;
    QVERIFY(source.packed);
    const std::filesystem::path notATool = source.Root() / "not-a-tool.exe";
    std::filesystem::create_directories(notATool);
    const TarExeArchiveReader reader(notATool);

    QCOMPARE(reader.EntriesOf(source.Zip()).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 10).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ExtractAll(source.Zip(), {}, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);
}

void TarExeArchiveReaderTest::AMemberNamedPasswordThatFailsInAFolderNamedEncryptedCouldNotBeExtracted()
{
    const Source source;
    const std::filesystem::path zip = source.Root() / "collision.zip";
    WriteFile(zip,
              ZipOf({{.name = "password/readme.txt", .content = "file"},
                     {.name = "password/readme.txt/inner.txt", .content = "inner"}}));
    const TarExeArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(zip);
    QCOMPARE(listing.failure, ArchiveFailure::None);
    const std::filesystem::path into = StagingInsideAFolderNamed(source, "encrypted");

    const ArchiveFailure failure = reader.ExtractAll(zip, listing.entries, into, {});

    QCOMPARE(failure, ArchiveFailure::CouldNotExtract);
}

void TarExeArchiveReaderTest::AHugeFailureThatNeverMentionsAPasswordIsAnsweredNoQuickly()
{
    const QStringList names = EntryNames(QStringLiteral("pack"), kHugeArchive);
    const QString message = OneLinePerName(names, QStringLiteral("Can't create: Permission denied"));

    const TimedAnswer answer = TimedAsk(message, names);

    QVERIFY(!answer.asks);
    QVERIFY2(answer.milliseconds < kSlowestAcceptableMs, qPrintable(SlowReport(answer)));
}

void TarExeArchiveReaderTest::AHugeFailureCitingNamesWithTheWordForAnotherReasonIsAnsweredNoQuickly()
{
    const QStringList names = EntryNames(QStringLiteral("password"), kHugeArchive);
    const QString message = OneLinePerName(names, QStringLiteral("Can't create: Permission denied"));

    const TimedAnswer answer = TimedAsk(message, names);

    QVERIFY(!answer.asks);
    QVERIFY2(answer.milliseconds < kSlowestAcceptableMs, qPrintable(SlowReport(answer)));
}

void TarExeArchiveReaderTest::AHugeRealRequestCitingInnocentNamesIsFoundQuickly()
{
    const QStringList names = EntryNames(QStringLiteral("pack"), kHugeArchive);
    const QString message = OneLinePerName(names, QStringLiteral("Encrypted file is unsupported"));

    const TimedAnswer answer = TimedAsk(message, names);

    QVERIFY(answer.asks);
    QVERIFY2(answer.milliseconds < kSlowestAcceptableMs, qPrintable(SlowReport(answer)));
}

void TarExeArchiveReaderTest::AHugeListOfInnocentNamesIsNotScannedAgainstLongRequestLines()
{
    const QStringList names = EntryNames(QStringLiteral("pack"), kHugeArchive);
    const QString line = QStringLiteral("tar: Encrypted file is unsupported: ") + QString(6000, u'x');
    const QString message = RepeatedLine(line, kMostLinesChecked);

    const TimedAnswer answer = TimedAsk(message, names);

    QVERIFY(answer.asks);
    QVERIFY2(answer.milliseconds < kSlowestAcceptableMs, qPrintable(SlowReport(answer)));
}

void TarExeArchiveReaderTest::ARealRequestAfterManyInnocentLinesIsStillFound()
{
    const QString innocent = RepeatedLine(QStringLiteral("tar: pack/file.bgl: Can't create: Permission denied"), 10000);
    const QString message = innocent + QStringLiteral("\r\ntar: pack/last.bgl: Encrypted file is unsupported");

    QVERIFY(AsksForAPassword(message, {QStringLiteral("pack/file.bgl"), QStringLiteral("pack/last.bgl")}));
}

void TarExeArchiveReaderTest::ANameWithTheWordCitedForAnotherReasonDoesNotAsk()
{
    const QStringList names{QStringLiteral("password.txt")};

    QVERIFY(!AsksForAPassword(QStringLiteral("tar: Can't create 'C:\\out\\password.txt': Permission denied"), names));
    QVERIFY(!AsksForAPassword(QStringLiteral("tar: Can't create 'C:/out/password.txt': Permission denied"), names));
    QVERIFY(
        !AsksForAPassword(QStringLiteral("tar: Can't create 'C:\\\\out\\\\password.txt': Permission denied"), names));
}

void TarExeArchiveReaderTest::ARealRequestCitingANameWithTheWordStillAsks()
{
    const QStringList names{QStringLiteral("password.txt")};

    QVERIFY(AsksForAPassword(QStringLiteral("tar: 'C:\\out\\password.txt': Encrypted file is unsupported"), names));
}

void TarExeArchiveReaderTest::ARealRequestAmongTheLinesCheckedIsFound()
{
    const QStringList names{QStringLiteral("password.txt")};
    const QString because =
        RepeatedLine(QStringLiteral("tar: Can't create 'out/password.txt': Permission denied"), kMostLinesChecked - 1);

    QVERIFY(AsksForAPassword(because + QStringLiteral("\r\ntar: Encrypted file is unsupported"), names));
}

void TarExeArchiveReaderTest::ARealRequestPastTheLinesCheckedBecauseOfNamesIsNotSeen()
{
    const QStringList names{QStringLiteral("password.txt")};
    const QString because =
        RepeatedLine(QStringLiteral("tar: Can't create 'out/password.txt': Permission denied"), kMostLinesChecked);

    QVERIFY(!AsksForAPassword(because + QStringLiteral("\r\ntar: Encrypted file is unsupported"), names));
}

QTEST_GUILESS_MAIN(TarExeArchiveReaderTest)

#include "tst_tar_exe_archive_reader.moc"
