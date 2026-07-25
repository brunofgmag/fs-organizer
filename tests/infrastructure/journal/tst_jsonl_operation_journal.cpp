#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <fstream>
#include <iterator>

#include "infrastructure/journal/JsonlOperationJournal.h"

class JsonlOperationJournalTest : public QObject
{
    Q_OBJECT

private slots:
    static void EachRecordBecomesOneLineWithEveryFieldOfTheOperation();
    static void AppendingNeverRewritesWhatWasAlreadyThere();
    static void TheRepairKindsHaveTheirOwnStableNames();
};

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

    OperationRecord Record(const OperationKind kind, const LinkFailure failure)
    {
        OperationRecord record;
        record.timestamp = std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000}};
        record.kind = kind;
        record.addonId = AddonId{"library-1", "pmdg-aircraft-77w"};
        record.source = R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)";
        record.target = R"(E:\Flight Simulator 2024\Community\pmdg-aircraft-77w)";
        record.failure = failure;

        return record;
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
    QCOMPARE(written.value("source").toString(),
             QStringLiteral(R"(D:\MSFS 2024\Aircrafts\pmdg-aircraft-77w)"));
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

QTEST_APPLESS_MAIN(JsonlOperationJournalTest)

#include "tst_jsonl_operation_journal.moc"
