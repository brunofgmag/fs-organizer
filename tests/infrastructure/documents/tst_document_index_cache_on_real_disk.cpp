#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <fstream>
#include <optional>
#include <string>

#include "domain/support/PathUtils.h"
#include "infrastructure/documents/JsonDocumentIndexCache.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class DocumentIndexCacheOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnIndexWrittenToDiskComesBackWithEveryLineOfIt();
        static void AnAddonFolderWithAnAccentSurvivesTheRoundTrip();
        static void AFileNobodyWroteAnswersNothingInsteadOfAnEmptyIndex();
        static void AFileSomethingElseWroteAnswersNothingInsteadOfHalfAnIndex();
        static void WritingAgainReplacesTheAddonsInsteadOfAddingToThem();
    };

    struct AFolderNobodyElseUses
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path File() const
        {
            return std::filesystem::path(directory.path().toStdWString()) / L"document-index.json";
        }
    };

    [[nodiscard]] std::chrono::system_clock::time_point AMomentWithoutFractions()
    {
        return std::chrono::system_clock::time_point{std::chrono::milliseconds{1786000000000}};
    }

    [[nodiscard]] DocumentsOfAnAddon TheCrj()
    {
        return {.addon = {.libraryId = "library-1", .folderName = "aerosoft-crj"},
                .folder = PathFromUtf8("D:/MSFS 2024/Aircrafts/aerosoft-crj"),
                .itWasWalked = true,
                .documents = {PathFromUtf8("Documentation/Vol1_Aircraft Manual.pdf"),
                              PathFromUtf8("Documentation/Vol4_Normal Ops Checklist.pdf")},
                .airports = {}};
    }

    [[nodiscard]] DocumentsOfAnAddon Brussels()
    {
        return {.addon = {.libraryId = "library-1", .folderName = "aerosoft-airport-ebbr-brussels"},
                .folder = PathFromUtf8("D:/MSFS 2024/Sceneries/aerosoft-airport-ebbr-brussels"),
                .itWasWalked = true,
                .documents = {},
                .airports = {{.code = "EBBR",
                              .catalogued = true,
                              .entriesInTheCatalogue = 154,
                              .types = {{.type = "AOI",
                                         .charts = {{.name = "Operating information",
                                                     .revision = ChartRevision::InForce,
                                                     .pages = {PathFromUtf8("EBBR/53211.pdf")}},
                                                    {.name = "",
                                                     .revision = ChartRevision::Previous,
                                                     .pages = {PathFromUtf8("EBBR/53241.pdf")}}}}}}}};
    }
}

void DocumentIndexCacheOnRealDiskTest::AnIndexWrittenToDiskComesBackWithEveryLineOfIt()
{
    const AFolderNobodyElseUses folder;

    {
        JsonDocumentIndexCache writing(folder.File());
        writing.Keep({.readAt = AMomentWithoutFractions(), .addons = {TheCrj(), Brussels()}});
    }

    const JsonDocumentIndexCache reading(folder.File());
    const std::optional<RememberedDocuments> known = reading.Remember();

    QVERIFY(known.has_value());
    QCOMPARE(known->readAt, AMomentWithoutFractions());
    QCOMPARE(known->addons.size(), std::size_t{2});

    QCOMPARE(known->addons.front().addon.folderName, TheCrj().addon.folderName);
    QCOMPARE(known->addons.front().addon.libraryId, TheCrj().addon.libraryId);
    QCOMPARE(known->addons.front().folder, TheCrj().folder);
    QCOMPARE(known->addons.front().documents, TheCrj().documents);
    QVERIFY(known->addons.front().itWasWalked);

    const ChartsOfAnAirport& airport = known->addons.back().airports.front();

    QCOMPARE(airport.code, std::string{"EBBR"});
    QVERIFY(airport.catalogued);
    QCOMPARE(airport.entriesInTheCatalogue, std::size_t{154});
    QCOMPARE(airport.types.front().type, std::string{"AOI"});
    QCOMPARE(airport.types.front().charts.size(), std::size_t{2});
    QCOMPARE(airport.types.front().charts.front().name, std::string{"Operating information"});
    QCOMPARE(airport.types.front().charts.front().pages.front(), PathFromUtf8("EBBR/53211.pdf"));
    QCOMPARE(airport.types.front().charts.back().revision, ChartRevision::Previous);
}

void DocumentIndexCacheOnRealDiskTest::AnAddonFolderWithAnAccentSurvivesTheRoundTrip()
{
    const AFolderNobodyElseUses folder;
    DocumentsOfAnAddon accented = TheCrj();
    accented.addon.folderName = "aerosoft-aéroport-lfpg";
    accented.folder = PathFromUtf8("D:/MSFS 2024/Sceneries/aerosoft-aéroport-lfpg");
    accented.documents = {PathFromUtf8("Documentação/Manuel de vol.pdf")};

    {
        JsonDocumentIndexCache writing(folder.File());
        writing.Keep({.readAt = AMomentWithoutFractions(), .addons = {accented}});
    }

    const JsonDocumentIndexCache reading(folder.File());
    const std::optional<RememberedDocuments> known = reading.Remember();

    QVERIFY(known.has_value());
    QCOMPARE(known->addons.front().addon.folderName, accented.addon.folderName);
    QCOMPARE(known->addons.front().folder, accented.folder);
    QCOMPARE(known->addons.front().documents.front(), accented.documents.front());
}

void DocumentIndexCacheOnRealDiskTest::AFileNobodyWroteAnswersNothingInsteadOfAnEmptyIndex()
{
    const AFolderNobodyElseUses folder;
    const JsonDocumentIndexCache reading(folder.File());

    QVERIFY2(!reading.Remember().has_value(),
             "a first run of the life has no index, and an empty one would be an index saying the library carries no "
             "documentation");
}

void DocumentIndexCacheOnRealDiskTest::AFileSomethingElseWroteAnswersNothingInsteadOfHalfAnIndex()
{
    const AFolderNobodyElseUses folder;

    std::ofstream stream(folder.File(), std::ios::binary);
    stream << "this is not the index it was told to read";
    stream.close();

    const JsonDocumentIndexCache reading(folder.File());

    QVERIFY(!reading.Remember().has_value());
}

void DocumentIndexCacheOnRealDiskTest::WritingAgainReplacesTheAddonsInsteadOfAddingToThem()
{
    const AFolderNobodyElseUses folder;

    {
        JsonDocumentIndexCache writing(folder.File());
        writing.Keep({.readAt = AMomentWithoutFractions(), .addons = {TheCrj(), Brussels()}});
        writing.Keep({.readAt = AMomentWithoutFractions(), .addons = {Brussels()}});
    }

    const JsonDocumentIndexCache reading(folder.File());
    const std::optional<RememberedDocuments> known = reading.Remember();

    QVERIFY(known.has_value());
    QVERIFY2(known->addons.size() == std::size_t{1},
             "the addon the user deleted between two runs stops being in the index, because the whole index is "
             "written every time instead of one addon being added to what was there");
}

QTEST_APPLESS_MAIN(DocumentIndexCacheOnRealDiskTest)

#include "tst_document_index_cache_on_real_disk.moc"
