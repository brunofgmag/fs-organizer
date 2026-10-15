#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <locale.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

#include "domain/importing/ImportPaths.h"
#include "domain/installing/PackagesInAnArchive.h"
#include "infrastructure/archive/LibarchiveArchiveReader.h"
#include "support/PathText.h"
#include "tests/support/ArchiveBuilders.h"
#include "tests/support/EnumPrinting.h"

namespace
{
    enum class Format : int
    {
        Zip = 0,
        SevenZip = 1,
        Rar5 = 2,
        Rar4 = 3,
    };

    class LibarchiveArchiveReaderTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheListingOfEveryFormatCarriesPathsSizesAndKindsWithoutTrailingSlashes_data();
        static void TheListingOfEveryFormatCarriesPathsSizesAndKindsWithoutTrailingSlashes();
        static void ExtractionOfEveryFormatReproducesTheTreeByteForByte_data();
        static void ExtractionOfEveryFormatReproducesTheTreeByteForByte();
        static void ProgressNeverGoesBackwardsStartsBelowTheTotalAndEndsAtIt();
        static void NobodyListeningStillExtracts();
        static void CancellingFromTheFirstProgressCallExtractsNothing();
        static void CancellingFromALaterProgressCallStopsTheExtraction();
        static void NamesAreReadAsUtf8WhateverTheLocaleOfTheProcessIs();
        static void ABackslashInAZipNameIsASeparatorAndALeadingDotSlashIsDropped();
        static void ALinkEntryIsOtherAndKeepsItsOwnName();
        static void AHardLinkEntryIsOtherAndRefusedOnExtraction();
        static void ANameThatIsNotUtf8IsListedAsOtherInsteadOfBeingGuessed();
        static void AnUnsafeNameIsListedAsWrittenSoThatTheDomainRefusesTheWholeArchive();
        static void AnEntryThatClimbsOutOfTheFolderIsRefusedAndWritesNothingOutside();
        static void AnAbsoluteEntryIsRefusedAndWritesNothingOutside();
        static void AFileWrittenThroughALinkEntryIsRefusedAndTheTargetStaysUntouched();
        static void AnEntryRepeatedInTheArchiveNeverOverwritesTheFirstCopy();
        static void TheManifestIsReadByItsLiteralNameAndMostCutsTheBytes();
        static void AnEntryThatIsNotThereOrIsAFolderCouldNotBeOpened();
        static void AFolderThatIsNotEmptyIsRefusedAndKeepsItsContent();
        static void AFolderThatIsNotAStagingPathIsRefusedAndStaysEmpty();
        static void AFolderThatIsNotThereIsRefusedAndIsNotCreated();
        static void AFileGivenAsTheFolderIsRefused();
        static void ATextFileIsNotAnArchive();
        static void AMissingArchiveCouldNotBeOpened();
        static void ATruncatedArchiveCouldNotBeOpenedOrExtracted();
        static void ACorruptedMemberCouldNotBeExtractedAndIsNotTakenForAPassword();
        static void AnEntryThatLiesAboutItsSizeCouldNotBeExtractedOrRead();
        static void AnEncryptedZipListsButNeedsAPasswordToReadOrExtract();
        static void AnEncryptedSevenZipListsButNeedsAPasswordToReadOrExtract();
        static void AnArchiveWithAnEncryptedHeaderNeedsAPasswordFromEveryMethod();
        static void AMemberNamedPasswordIsNotAskedForAPasswordWhenItIsNotEncrypted();
        static void AListingPastTheTimeLimitTookTooLong();
        static void AReadPastTheTimeLimitTookTooLong();
        static void AnExtractionPastTheIdleLimitTookTooLong();
        static void AnEmptyStagingFolderInAFolderWithAccentsIsAccepted();
    };
}

namespace
{
    constexpr std::size_t kBigFileBytes = 300000;

    [[nodiscard]] std::string Utf8Of(const std::u8string_view text)
    {
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    const std::string kPackage = Utf8Of(u8"[RR WL] pacote ação");
    const std::string kManifest = Utf8Of(u8"{\"title\":\"Pacote ação\"}");
    const std::string kSmall = "hello";
    const std::string kBig =
        std::string(kBigFileBytes / 10, '\x01') + std::string(kBigFileBytes - kBigFileBytes / 10, 'z');

    [[nodiscard]] std::vector<ArchiveItem> PackageItems()
    {
        return {
            {.name = kPackage, .folder = true},
            {.name = kPackage + "/empty", .folder = true},
            {.name = kPackage + "/leia me.txt", .content = kSmall},
            {.name = kPackage + "/manifest.json", .content = kManifest},
            {.name = kPackage + "/sub", .folder = true},
            {.name = kPackage + "/sub/big file.bin", .content = kBig},
        };
    }

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

    [[nodiscard]] std::string BytesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);

        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] std::filesystem::path Fixture(const std::string& name)
    {
        return std::filesystem::path(FSORG_FIXTURES_DIR) / name;
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

    [[nodiscard]] std::function<bool(const CopyProgress&)> Recording(std::vector<CopyProgress>& seen)
    {
        return [&seen](const CopyProgress& progress)
        {
            seen.push_back(progress);

            return true;
        };
    }

    struct Source
    {
        QTemporaryDir directory;

        Source()
        {
            WriteFile(Archive(Format::Zip), ZipOf(PackageItems()));
            WriteFile(Archive(Format::Rar5), Rar5Of(PackageItems()));
            WriteFile(Archive(Format::Rar4), Rar4Of(PackageItems()));
        }

        [[nodiscard]] std::filesystem::path Root() const
        {
            return AsPath(directory.path());
        }

        [[nodiscard]] std::filesystem::path Archive(const Format format) const
        {
            switch (format)
            {
            case Format::Zip: return Root() / "pack.zip";
            case Format::SevenZip: return Fixture("archive-package.7z");
            case Format::Rar5: return Root() / "pack5.rar";
            case Format::Rar4: return Root() / "pack4.rar";
            }

            return {};
        }

        [[nodiscard]] std::filesystem::path Zip() const
        {
            return Archive(Format::Zip);
        }

        [[nodiscard]] std::filesystem::path ZipOfItems(const std::vector<ArchiveItem>& items,
                                                       const std::string& name = "odd.zip") const
        {
            const std::filesystem::path zip = Root() / name;
            WriteFile(zip, ZipOf(items));

            return zip;
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
    };

    [[nodiscard]] QStringList TreeOfThePackage()
    {
        return {AsText(PathFromUtf8(kPackage)) + QStringLiteral("/"),
                AsText(PathFromUtf8(kPackage + "/empty")) + QStringLiteral("/"),
                AsText(PathFromUtf8(kPackage + "/leia me.txt")) + QStringLiteral(" | 5"),
                AsText(PathFromUtf8(kPackage + "/manifest.json")) + QStringLiteral(" | %1").arg(kManifest.size()),
                AsText(PathFromUtf8(kPackage + "/sub")) + QStringLiteral("/"),
                AsText(PathFromUtf8(kPackage + "/sub/big file.bin")) + QStringLiteral(" | %1").arg(kBig.size())};
    }

    class WithTheLocale
    {
    public:
        explicit WithTheLocale(const char* name) : before_(setlocale(LC_ALL, nullptr))
        {
            setlocale(LC_ALL, name);
        }

        WithTheLocale(const WithTheLocale&) = delete;
        WithTheLocale& operator=(const WithTheLocale&) = delete;

        ~WithTheLocale()
        {
            setlocale(LC_ALL, before_.c_str());
        }

    private:
        std::string before_;
    };

    void AddEveryFormat()
    {
        QTest::addColumn<int>("format");
        QTest::newRow("zip") << static_cast<int>(Format::Zip);
        QTest::newRow("7z") << static_cast<int>(Format::SevenZip);
        QTest::newRow("rar5") << static_cast<int>(Format::Rar5);
        QTest::newRow("rar4") << static_cast<int>(Format::Rar4);
    }
}

void LibarchiveArchiveReaderTest::TheListingOfEveryFormatCarriesPathsSizesAndKindsWithoutTrailingSlashes_data()
{
    AddEveryFormat();
}

void LibarchiveArchiveReaderTest::TheListingOfEveryFormatCarriesPathsSizesAndKindsWithoutTrailingSlashes()
{
    QFETCH(int, format);
    const Source source;

    const ArchiveListing listing = LibarchiveArchiveReader().EntriesOf(source.Archive(static_cast<Format>(format)));

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries), Expected());
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::None);
}

void LibarchiveArchiveReaderTest::ExtractionOfEveryFormatReproducesTheTreeByteForByte_data()
{
    AddEveryFormat();
}

void LibarchiveArchiveReaderTest::ExtractionOfEveryFormatReproducesTheTreeByteForByte()
{
    QFETCH(int, format);
    const Source source;
    const std::filesystem::path archive = source.Archive(static_cast<Format>(format));
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(archive);
    QCOMPARE(listing.failure, ArchiveFailure::None);
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(reader.ExtractAll(archive, listing.entries, into, {}), ArchiveFailure::None);

    QCOMPARE(TreeOf(into), TreeOfThePackage());
    QCOMPARE(BytesOf(into / PathFromUtf8(kPackage + "/manifest.json")), kManifest);
    QCOMPARE(BytesOf(into / PathFromUtf8(kPackage + "/leia me.txt")), kSmall);
    QVERIFY(BytesOf(into / PathFromUtf8(kPackage + "/sub/big file.bin")) == kBig);
}

void LibarchiveArchiveReaderTest::ProgressNeverGoesBackwardsStartsBelowTheTotalAndEndsAtIt()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    std::vector<CopyProgress> seen;

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, source.EmptyStaging(), Recording(seen)),
             ArchiveFailure::None);

    QVERIFY(seen.size() >= 2);
    QCOMPARE(seen.front().copiedBytes, std::uintmax_t{0});
    QVERIFY(seen.front().copiedBytes < seen.front().totalBytes);
    QVERIFY(std::ranges::is_sorted(seen, {}, &CopyProgress::copiedBytes));
    QCOMPARE(seen.back().totalBytes, std::uintmax_t{kSmall.size() + kManifest.size() + kBig.size()});
    QCOMPARE(seen.back().copiedBytes, seen.back().totalBytes);
}

void LibarchiveArchiveReaderTest::NobodyListeningStillExtracts()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::None);

    QCOMPARE(TreeOf(into), TreeOfThePackage());
}

void LibarchiveArchiveReaderTest::CancellingFromTheFirstProgressCallExtractsNothing()
{
    const Source source;
    const LibarchiveArchiveReader reader;
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
    QVERIFY(TreeOf(into).isEmpty());
    std::error_code error;
    std::filesystem::remove_all(into, error);
    QVERIFY(!error);
}

void LibarchiveArchiveReaderTest::CancellingFromALaterProgressCallStopsTheExtraction()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();
    int calls = 0;

    const ArchiveFailure failure = reader.ExtractAll(source.Zip(), listing.entries, into,
                                                     [&calls](const CopyProgress&)
                                                     {
                                                         return ++calls < 2;
                                                     });

    QCOMPARE(failure, ArchiveFailure::Cancelled);
    QCOMPARE(calls, 2);
    QVERIFY(!std::filesystem::exists(into / PathFromUtf8(kPackage) / "sub" / "big file.bin")
            || std::filesystem::file_size(into / PathFromUtf8(kPackage) / "sub" / "big file.bin") <= kBig.size());
    std::error_code error;
    std::filesystem::remove_all(into, error);
    QVERIFY(!error);
}

void LibarchiveArchiveReaderTest::NamesAreReadAsUtf8WhateverTheLocaleOfTheProcessIs()
{
    const Source source;
    const WithTheLocale plain("C");

    const ArchiveListing zip = LibarchiveArchiveReader().EntriesOf(source.Archive(Format::Zip));
    const ArchiveListing rar4 = LibarchiveArchiveReader().EntriesOf(source.Archive(Format::Rar4));
    const ArchiveListing sevenZip = LibarchiveArchiveReader().EntriesOf(source.Archive(Format::SevenZip));
    const ArchiveText manifest =
        LibarchiveArchiveReader().ContentsOf(source.Archive(Format::Zip), kPackage + "/manifest.json", 1000);

    QCOMPARE(DescribedAll(zip.entries), Expected());
    QCOMPARE(DescribedAll(rar4.entries), Expected());
    QCOMPARE(DescribedAll(sevenZip.entries), Expected());
    QCOMPARE(manifest.text, kManifest);
    QCOMPARE(QString::fromLatin1(setlocale(LC_ALL, nullptr)), QStringLiteral("C"));
}

void LibarchiveArchiveReaderTest::ABackslashInAZipNameIsASeparatorAndALeadingDotSlashIsDropped()
{
    const Source source;
    const std::filesystem::path zip = source.ZipOfItems({
        {.name = "pkg\\sub\\manifest.json", .content = "x"},
        {.name = "./pkg/other.txt", .content = "y"},
        {.name = "./", .folder = true},
    });

    const ArchiveListing listing = LibarchiveArchiveReader().EntriesOf(zip);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries),
             DescribedAll({{.path = "pkg/other.txt", .bytes = 1, .kind = ArchiveEntryKind::File},
                           {.path = "pkg/sub/manifest.json", .bytes = 1, .kind = ArchiveEntryKind::File}}));
}

void LibarchiveArchiveReaderTest::ALinkEntryIsOtherAndKeepsItsOwnName()
{
    const Source source;
    const std::filesystem::path zip = source.ZipOfItems({
        {.name = "pkg/manifest.json", .content = "{}"},
        {.name = "pkg/link", .content = "../outside", .unixMode = 0120777},
    });

    const ArchiveListing listing = LibarchiveArchiveReader().EntriesOf(zip);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries),
             DescribedAll({{.path = "pkg/link", .bytes = 0, .kind = ArchiveEntryKind::Other},
                           {.path = "pkg/manifest.json", .bytes = 2, .kind = ArchiveEntryKind::File}}));
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
}

void LibarchiveArchiveReaderTest::AHardLinkEntryIsOtherAndRefusedOnExtraction()
{
    constexpr std::uint32_t kHardLink = 4;
    const Source source;
    const std::filesystem::path rar = source.Root() / "linked.rar";
    WriteFile(rar,
              Rar5Of({
                  {.name = "pkg/manifest.json", .content = "{}"},
                  {.name = "pkg/copy.json", .redirection = kHardLink, .redirectedTo = "pkg/manifest.json"},
              }));
    const LibarchiveArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(rar);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries),
             DescribedAll({{.path = "pkg/copy.json", .bytes = 0, .kind = ArchiveEntryKind::Other},
                           {.path = "pkg/manifest.json", .bytes = 2, .kind = ArchiveEntryKind::File}}));
    QCOMPARE(reader.ExtractAll(rar, listing.entries, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);
}

void LibarchiveArchiveReaderTest::ANameThatIsNotUtf8IsListedAsOtherInsteadOfBeingGuessed()
{
    const Source source;
    const std::filesystem::path zip = source.ZipOfItems({
        {.name = "pkg/manifest.json", .content = "{}"},
        {.name = "pkg/bad\xFF\xFE.txt", .content = "x"},
    });

    const ArchiveListing listing = LibarchiveArchiveReader().EntriesOf(zip);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(PackagesIn(listing.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
}

void LibarchiveArchiveReaderTest::AnUnsafeNameIsListedAsWrittenSoThatTheDomainRefusesTheWholeArchive()
{
    const Source source;
    const std::filesystem::path climbing = source.ZipOfItems(
        {{.name = "pkg/manifest.json", .content = "{}"}, {.name = "../evil.txt", .content = "x"}}, "climb.zip");
    const std::filesystem::path absolute = source.ZipOfItems(
        {{.name = "pkg/manifest.json", .content = "{}"}, {.name = "/etc/evil.txt", .content = "x"}}, "abs.zip");

    const ArchiveListing climbs = LibarchiveArchiveReader().EntriesOf(climbing);
    const ArchiveListing absolutes = LibarchiveArchiveReader().EntriesOf(absolute);

    QCOMPARE(climbs.failure, ArchiveFailure::None);
    QVERIFY(std::ranges::any_of(climbs.entries,
                                [](const ArchiveEntry& entry)
                                {
                                    return entry.path == "../evil.txt";
                                }));
    QCOMPARE(PackagesIn(climbs.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
    QCOMPARE(absolutes.failure, ArchiveFailure::None);
    QVERIFY(std::ranges::any_of(absolutes.entries,
                                [](const ArchiveEntry& entry)
                                {
                                    return entry.path == "/etc/evil.txt";
                                }));
    QCOMPARE(PackagesIn(absolutes.entries).refusal, NothingToInstall::AnEntryIsNotSafe);
}

void LibarchiveArchiveReaderTest::AnEntryThatClimbsOutOfTheFolderIsRefusedAndWritesNothingOutside()
{
    const Source source;
    const std::filesystem::path zip = source.ZipOfItems(
        {{.name = "pkg/manifest.json", .content = "{}"}, {.name = "../evil.txt", .content = "x"}}, "climb.zip");
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(LibarchiveArchiveReader().ExtractAll(zip, {}, into, {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(!std::filesystem::exists(source.Root() / "evil.txt"));
    QVERIFY(!std::filesystem::exists(into.parent_path() / "evil.txt"));
}

void LibarchiveArchiveReaderTest::AnAbsoluteEntryIsRefusedAndWritesNothingOutside()
{
    const Source source;
    const QTemporaryDir elsewhere;
    const std::filesystem::path outside = AsPath(elsewhere.path()) / "abs.txt";
    const std::filesystem::path zip = source.ZipOfItems(
        {{.name = "pkg/manifest.json", .content = "{}"}, {.name = AsUtf8(outside), .content = "x"}}, "abs.zip");

    QCOMPARE(LibarchiveArchiveReader().ExtractAll(zip, {}, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(!std::filesystem::exists(outside));
}

void LibarchiveArchiveReaderTest::AFileWrittenThroughALinkEntryIsRefusedAndTheTargetStaysUntouched()
{
    const Source source;
    const std::filesystem::path outside = source.Root() / "outside";
    std::filesystem::create_directories(outside);
    const std::filesystem::path zip = source.ZipOfItems(
        {
            {.name = "pkg/link", .content = AsUtf8(outside), .unixMode = 0120777},
            {.name = "pkg/link/planted.txt", .content = "x"},
        },
        "through.zip");

    QCOMPARE(LibarchiveArchiveReader().ExtractAll(zip, {}, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(!std::filesystem::exists(outside / "planted.txt"));
    QVERIFY(std::filesystem::is_empty(outside));
}

void LibarchiveArchiveReaderTest::AnEntryRepeatedInTheArchiveNeverOverwritesTheFirstCopy()
{
    const Source source;
    const std::filesystem::path twice = source.ZipOfItems(
        {{.name = "a.txt", .content = "first"}, {.name = "a.txt", .content = "second!!"}}, "twice.zip");
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(LibarchiveArchiveReader().ExtractAll(twice, {}, into, {}), ArchiveFailure::None);

    QCOMPARE(TreeOf(into), QStringList{QStringLiteral("a.txt | 5")});
    QCOMPARE(BytesOf(into / "a.txt"), std::string("first"));
}

void LibarchiveArchiveReaderTest::TheManifestIsReadByItsLiteralNameAndMostCutsTheBytes()
{
    const Source source;
    const LibarchiveArchiveReader reader;

    const ArchiveText whole = reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 1000);
    const ArchiveText cut = reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 5);
    const ArchiveText head = reader.ContentsOf(source.Zip(), kPackage + "/sub/big file.bin", 10);
    const ArchiveText big = reader.ContentsOf(source.Zip(), kPackage + "/sub/big file.bin", kBig.size() + 100);
    const ArchiveText none = reader.ContentsOf(source.Zip(), kPackage + "/manifest.json", 0);

    QCOMPARE(whole.failure, ArchiveFailure::None);
    QCOMPARE(whole.text, kManifest);
    QCOMPARE(cut.failure, ArchiveFailure::None);
    QCOMPARE(cut.text, kManifest.substr(0, 5));
    QCOMPARE(head.failure, ArchiveFailure::None);
    QCOMPARE(head.text, kBig.substr(0, 10));
    QCOMPARE(big.failure, ArchiveFailure::None);
    QVERIFY(big.text == kBig);
    QCOMPARE(none.failure, ArchiveFailure::None);
    QVERIFY(none.text.empty());
}

void LibarchiveArchiveReaderTest::AnEntryThatIsNotThereOrIsAFolderCouldNotBeOpened()
{
    const Source source;
    const LibarchiveArchiveReader reader;

    const ArchiveText missing = reader.ContentsOf(source.Zip(), kPackage + "/nao existe.txt", 1000);
    const ArchiveText folder = reader.ContentsOf(source.Zip(), kPackage + "/sub", 1000);

    QCOMPARE(missing.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(missing.text.empty());
    QCOMPARE(folder.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(folder.text.empty());
}

void LibarchiveArchiveReaderTest::AFolderThatIsNotEmptyIsRefusedAndKeepsItsContent()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.EmptyStaging();
    WriteFile(into / "ja estava aqui.txt", "mine");
    const QStringList before = TreeOf(into);

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::CouldNotExtract);

    QCOMPARE(TreeOf(into), before);
}

void LibarchiveArchiveReaderTest::AFolderThatIsNotAStagingPathIsRefusedAndStaysEmpty()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    const std::filesystem::path into = source.Root() / "target";
    std::filesystem::create_directories(into);

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(TreeOf(into).isEmpty());
}

void LibarchiveArchiveReaderTest::AFolderThatIsNotThereIsRefusedAndIsNotCreated()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, source.Staging(), {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(!std::filesystem::exists(source.Staging()));
}

void LibarchiveArchiveReaderTest::AFileGivenAsTheFolderIsRefused()
{
    const Source source;
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());
    WriteFile(source.Staging(), "");

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, source.Staging(), {}), ArchiveFailure::CouldNotExtract);

    QVERIFY(std::filesystem::is_regular_file(source.Staging()));
}

void LibarchiveArchiveReaderTest::ATextFileIsNotAnArchive()
{
    const Source source;
    const std::filesystem::path text = source.Root() / "notes.txt";
    WriteFile(text, "just some words, nothing to unpack\n");
    const LibarchiveArchiveReader reader;

    QCOMPARE(reader.EntriesOf(text).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ContentsOf(text, "notes.txt", 100).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ExtractAll(text, {}, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);
}

void LibarchiveArchiveReaderTest::AMissingArchiveCouldNotBeOpened()
{
    const Source source;
    const LibarchiveArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(source.Root() / "never-made.zip");

    QCOMPARE(listing.failure, ArchiveFailure::CouldNotOpen);
    QVERIFY(listing.entries.empty());
    QCOMPARE(reader.ContentsOf(source.Root() / "never-made.zip", "a", 1).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ExtractAll(source.Root() / "never-made.zip", {}, source.EmptyStaging(), {}),
             ArchiveFailure::CouldNotExtract);
}

void LibarchiveArchiveReaderTest::ATruncatedArchiveCouldNotBeOpenedOrExtracted()
{
    const Source source;
    const std::string whole = BytesOf(source.Zip());
    const std::filesystem::path cut = source.Root() / "cut.zip";
    WriteFile(cut, whole.substr(0, whole.size() / 2));
    const LibarchiveArchiveReader reader;

    QCOMPARE(reader.EntriesOf(cut).failure, ArchiveFailure::CouldNotOpen);
    QCOMPARE(reader.ExtractAll(cut, {}, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);
}

void LibarchiveArchiveReaderTest::ACorruptedMemberCouldNotBeExtractedAndIsNotTakenForAPassword()
{
    const Source source;
    std::string zip = ZipOf({{.name = "password.txt", .content = "the content of the member"}});
    zip[30 + std::string("password.txt").size() + 3] ^= 0x55;
    const std::filesystem::path damaged = source.Root() / "damaged.zip";
    WriteFile(damaged, zip);
    const LibarchiveArchiveReader reader;

    QCOMPARE(reader.ExtractAll(damaged, {}, source.EmptyStaging(), {}), ArchiveFailure::CouldNotExtract);
    QCOMPARE(reader.ContentsOf(damaged, "password.txt", 100).failure, ArchiveFailure::CouldNotOpen);
}

void LibarchiveArchiveReaderTest::AnEntryThatLiesAboutItsSizeCouldNotBeExtractedOrRead()
{
    constexpr std::size_t kLocalSizeField = 22;
    constexpr std::size_t kLocalHeader = 30;
    const Source source;
    const std::string name = "a.txt";
    const std::string honest = ZipOf({{.name = name, .content = "12345678"}});
    std::string lying = honest;
    const std::size_t central = lying.find("PK\x01\x02");
    lying[kLocalSizeField] = 3;
    lying[central + 24] = 3;
    const std::filesystem::path zip = source.Root() / "lie.zip";
    WriteFile(zip, lying);
    QCOMPARE(honest.size(), lying.size());
    QVERIFY(central > kLocalHeader + name.size());
    const std::filesystem::path into = source.EmptyStaging();

    QCOMPARE(LibarchiveArchiveReader().ExtractAll(zip, {}, into, {}), ArchiveFailure::CouldNotExtract);
    QCOMPARE(LibarchiveArchiveReader().ContentsOf(zip, name, 100).failure, ArchiveFailure::CouldNotOpen);
}

void LibarchiveArchiveReaderTest::AnEncryptedZipListsButNeedsAPasswordToReadOrExtract()
{
    const Source source;
    const std::filesystem::path zip = source.ZipOfItems(
        {
            {.name = "pkg", .folder = true},
            {.name = "pkg/manifest.json", .content = "scrambled", .encrypted = true},
        },
        "locked.zip");
    const LibarchiveArchiveReader reader;
    const std::filesystem::path into = source.EmptyStaging();

    const ArchiveListing listing = reader.EntriesOf(zip);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(listing.entries.size(), std::size_t{2});
    QCOMPARE(reader.ContentsOf(zip, "pkg/manifest.json", 100).failure, ArchiveFailure::NeedsAPassword);
    QCOMPARE(reader.ExtractAll(zip, listing.entries, into, {}), ArchiveFailure::NeedsAPassword);
}

void LibarchiveArchiveReaderTest::AnEncryptedSevenZipListsButNeedsAPasswordToReadOrExtract()
{
    const Source source;
    const std::filesystem::path locked = Fixture("archive-encrypted-data.7z");
    const LibarchiveArchiveReader reader;

    const ArchiveListing listing = reader.EntriesOf(locked);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(DescribedAll(listing.entries), Expected());
    QCOMPARE(reader.ContentsOf(locked, kPackage + "/manifest.json", 100).failure, ArchiveFailure::NeedsAPassword);
    QCOMPARE(reader.ExtractAll(locked, listing.entries, source.EmptyStaging(), {}), ArchiveFailure::NeedsAPassword);
}

void LibarchiveArchiveReaderTest::AnArchiveWithAnEncryptedHeaderNeedsAPasswordFromEveryMethod()
{
    const Source source;
    const std::filesystem::path locked = Fixture("archive-encrypted-header.7z");
    const LibarchiveArchiveReader reader;

    QCOMPARE(reader.EntriesOf(locked).failure, ArchiveFailure::NeedsAPassword);
    QCOMPARE(reader.ContentsOf(locked, kPackage + "/manifest.json", 100).failure, ArchiveFailure::NeedsAPassword);
    QCOMPARE(reader.ExtractAll(locked, {}, source.EmptyStaging(), {}), ArchiveFailure::NeedsAPassword);
}

void LibarchiveArchiveReaderTest::AMemberNamedPasswordIsNotAskedForAPasswordWhenItIsNotEncrypted()
{
    const Source source;
    const std::filesystem::path zip = source.ZipOfItems(
        {{.name = "password/manifest.json", .content = "{}"}, {.name = "password/encrypted.txt", .content = "x"}},
        "words.zip");
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(zip);

    QCOMPARE(listing.failure, ArchiveFailure::None);
    QCOMPARE(reader.ContentsOf(zip, "password/manifest.json", 100).text, std::string("{}"));
    QCOMPARE(reader.ExtractAll(zip, listing.entries, source.EmptyStaging(), {}), ArchiveFailure::None);
}

void LibarchiveArchiveReaderTest::AListingPastTheTimeLimitTookTooLong()
{
    const Source source;

    const ArchiveListing listing = LibarchiveArchiveReader(std::chrono::milliseconds{0}).EntriesOf(source.Zip());

    QCOMPARE(listing.failure, ArchiveFailure::TookTooLong);
    QVERIFY(listing.entries.empty());
}

void LibarchiveArchiveReaderTest::AReadPastTheTimeLimitTookTooLong()
{
    const Source source;

    const ArchiveText text = LibarchiveArchiveReader(std::chrono::milliseconds{0})
                                 .ContentsOf(source.Zip(), kPackage + "/manifest.json", 100);

    QCOMPARE(text.failure, ArchiveFailure::TookTooLong);
    QVERIFY(text.text.empty());
}

void LibarchiveArchiveReaderTest::AnExtractionPastTheIdleLimitTookTooLong()
{
    const Source source;
    const ArchiveListing listing = LibarchiveArchiveReader().EntriesOf(source.Zip());
    const LibarchiveArchiveReader impatient(kLibarchiveTimeLimit, std::chrono::milliseconds{0});

    QCOMPARE(impatient.ExtractAll(source.Zip(), listing.entries, source.EmptyStaging(), {}),
             ArchiveFailure::TookTooLong);
}

void LibarchiveArchiveReaderTest::AnEmptyStagingFolderInAFolderWithAccentsIsAccepted()
{
    const Source source;
    const std::filesystem::path accented = source.Root() / PathFromUtf8(kPackage) / "alvo";
    const std::filesystem::path into = StagingPathFor(accented);
    std::filesystem::create_directories(into);
    const LibarchiveArchiveReader reader;
    const ArchiveListing listing = reader.EntriesOf(source.Zip());

    QCOMPARE(reader.ExtractAll(source.Zip(), listing.entries, into, {}), ArchiveFailure::None);

    QCOMPARE(TreeOf(into), TreeOfThePackage());
}

QTEST_APPLESS_MAIN(LibarchiveArchiveReaderTest)

#include "tst_libarchive_archive_reader.moc"
