#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>
#include <iterator>

#include "infrastructure/journal/JsonlOperationJournal.h"

namespace
{
    class JsonlOperationJournalTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EachRecordBecomesOneLineWithEveryFieldOfTheOperation();
        static void AppendingNeverRewritesWhatWasAlreadyThere();
        static void TheRepairKindsHaveTheirOwnStableNames();
        static void AnImportRecordCarriesItsResultAndNoLinkFailure();
        static void EveryKindAndEveryReasonSurvivesTheRoundTrip();
        static void ReadingSkipsALineWrittenByANewerVersion();
        static void AnOutcomeThisVersionCannotReadIsNotASuccess();
        static void AnAbsentJournalReadsAsNoHistoryAtAll();
    };
}

namespace
{
    struct Storage
    {
        QTemporaryDir directory;

        [[nodiscard]] std::filesystem::path File() const
        {
            return std::filesystem::path(directory.path().toStdWString()) / "journal" / "journal.jsonl";
        }
    };

    QStringList LinesOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        const std::string content{std::istreambuf_iterator(stream), std::istreambuf_iterator<char>()};

        return QString::fromStdString(content).split('\n', Qt::SkipEmptyParts);
    }

    constexpr auto kMoment = std::chrono::seconds{1'769'000'000};
    constexpr auto kSource = R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)";
    constexpr auto kTarget = R"(E:\Flight Simulator 2024\Community\pmdg-aircraft-77w)";

    OperationRecord Record(const OperationKind kind, const LinkFailure failure)
    {
        return OperationRecord::OfLink(std::chrono::system_clock::time_point{kMoment}, kind,
                                       AddonId{.libraryId = "library-1", .folderName = "pmdg-aircraft-77w"}, kSource,
                                       kTarget, failure);
    }

    OperationRecord Record(const OperationKind kind, const FileResult result)
    {
        return OperationRecord::OfImport(std::chrono::system_clock::time_point{kMoment}, kind,
                                         AddonId{.libraryId = "library-1", .folderName = "pmdg-aircraft-77w"}, kSource,
                                         kTarget, result);
    }
}

void JsonlOperationJournalTest::EachRecordBecomesOneLineWithEveryFieldOfTheOperation()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());
    journal.Append(Record(OperationKind::EnableAddon, LinkFailure::DestinationHoldsRealFolder));

    const QStringList lines = LinesOf(storage.File());
    QCOMPARE(lines.size(), 1);

    const QJsonObject written = QJsonDocument::fromJson(lines.front().toUtf8()).object();
    QCOMPARE(written.value("timestamp").toString(), QStringLiteral("2026-01-21T12:53:20Z"));
    QCOMPARE(written.value("kind").toString(), QStringLiteral("enable"));
    QCOMPARE(written.value("libraryId").toString(), QStringLiteral("library-1"));
    QCOMPARE(written.value("addon").toString(), QStringLiteral("pmdg-aircraft-77w"));
    QCOMPARE(written.value("source").toString(), QStringLiteral(R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)"));
    QCOMPARE(written.value("target").toString(),
             QStringLiteral(R"(E:\Flight Simulator 2024\Community\pmdg-aircraft-77w)"));
    QCOMPARE(written.value("failure").toString(), QStringLiteral("destinationHoldsRealFolder"));
}

void JsonlOperationJournalTest::AppendingNeverRewritesWhatWasAlreadyThere()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());
    journal.Append(Record(OperationKind::EnableAddon, LinkFailure::None));
    journal.Append(Record(OperationKind::DisableAddon, LinkFailure::None));

    const QStringList lines = LinesOf(storage.File());
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines.front().contains(QStringLiteral("\"kind\":\"enable\"")));
    QVERIFY(lines.back().contains(QStringLiteral("\"kind\":\"disable\"")));
}

void JsonlOperationJournalTest::TheRepairKindsHaveTheirOwnStableNames()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());
    journal.Append(Record(OperationKind::RemoveBrokenLink, LinkFailure::None));
    journal.Append(Record(OperationKind::RepointLink, LinkFailure::None));

    const QStringList lines = LinesOf(storage.File());
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines[0].contains(QStringLiteral("\"kind\":\"removeBrokenLink\"")));
    QVERIFY(lines[1].contains(QStringLiteral("\"kind\":\"repointLink\"")));
}

void JsonlOperationJournalTest::AnImportRecordCarriesItsResultAndNoLinkFailure()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());
    journal.Append(Record(OperationKind::ImportRemoveSource, FileResult::CouldNotRemoveSource));

    const QStringList lines = LinesOf(storage.File());
    QCOMPARE(lines.size(), 1);

    const QJsonObject written = QJsonDocument::fromJson(lines.front().toUtf8()).object();
    QCOMPARE(written.value("kind").toString(), QStringLiteral("importRemoveSource"));
    QCOMPARE(written.value("result").toString(), QStringLiteral("couldNotRemoveSource"));
    QVERIFY(!written.contains(QStringLiteral("failure")));
}

void JsonlOperationJournalTest::EveryKindAndEveryReasonSurvivesTheRoundTrip()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());

    std::vector<OperationRecord> written;
    for (const OperationKind kind : kAllOperationKinds)
    {
        if (CarriesAFileReason(kind))
        {
            for (const FileResult result : kAllFileResults)
            {
                written.push_back(Record(kind, result));
                journal.Append(written.back());
            }

            continue;
        }

        for (const LinkFailure failure : kAllLinkFailures)
        {
            written.push_back(Record(kind, failure));
            journal.Append(written.back());
        }
    }

    const std::vector<OperationRecord> read = journal.Read();
    QCOMPARE(read.size(), written.size());

    for (std::size_t position = 0; position < written.size(); ++position)
    {
        QCOMPARE(read[position].kind, written[position].kind);
        QCOMPARE(read[position].timestamp, written[position].timestamp);
        QCOMPARE(read[position].addonId.libraryId, written[position].addonId.libraryId);
        QCOMPARE(read[position].addonId.folderName, written[position].addonId.folderName);
        QCOMPARE(read[position].source, written[position].source);
        QCOMPARE(read[position].target, written[position].target);
        QCOMPARE(read[position].outcome, written[position].outcome);
    }
}

void JsonlOperationJournalTest::ReadingSkipsALineWrittenByANewerVersion()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());
    journal.Append(Record(OperationKind::EnableAddon, LinkFailure::None));

    std::ofstream stream(storage.File(), std::ios::binary | std::ios::app);
    stream << R"({"kind":"teleportAddon","addon":"pmdg-aircraft-77w"})" << '\n';
    stream << "isto nao e json" << '\n';
    stream.close();

    const std::vector<OperationRecord> read = journal.Read();

    QCOMPARE(read.size(), std::size_t{1});
    QCOMPARE(read.front().kind, OperationKind::EnableAddon);
}

void JsonlOperationJournalTest::AnOutcomeThisVersionCannotReadIsNotASuccess()
{
    const Storage storage;

    JsonlOperationJournal journal(storage.File());
    journal.Append(Record(OperationKind::EnableAddon, LinkFailure::None));

    std::ofstream stream(storage.File(), std::ios::binary | std::ios::app);
    stream << R"({"kind":"importCopyToStaging","addon":"pmdg-aircraft-77w","result":"theDiskCaughtFire"})" << '\n';
    stream << R"({"kind":"enable","addon":"pmdg-aircraft-77w","failure":"theCableWasUnplugged"})" << '\n';
    stream.close();

    const std::vector<OperationRecord> read = journal.Read();

    QCOMPARE(read.size(), std::size_t{3});
    QVERIFY(Succeeded(read.front().outcome));
    QVERIFY(!Succeeded(read[1].outcome));
    QVERIFY(!Succeeded(read[2].outcome));
    QCOMPARE(read[1].outcome, OperationOutcome{FileResult::TheOutcomeIsUnknown});
    QCOMPARE(read[2].outcome, OperationOutcome{LinkFailure::TheOutcomeIsUnknown});
}

void JsonlOperationJournalTest::AnAbsentJournalReadsAsNoHistoryAtAll()
{
    const Storage storage;

    QVERIFY(JsonlOperationJournal(storage.File()).Read().empty());
}

QTEST_APPLESS_MAIN(JsonlOperationJournalTest)

#include "tst_jsonl_operation_journal.moc"
