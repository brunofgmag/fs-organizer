#include <QtTest/QtTest>

#include "infrastructure/catalog/JsonManifestParser.h"

class JsonManifestParserTest : public QObject
{
    Q_OBJECT

private slots:
    static void AManifestExposesTheFieldsTheTreeNeeds();
    static void AByteOrderMarkDoesNotHideTheManifest();
    static void FreeTextFieldsSurviveWhateverTheAuthorWrote();
    static void ContentThatIsNotAJsonObjectIsNotAManifest();
};

void JsonManifestParserTest::AManifestExposesTheFieldsTheTreeNeeds()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> manifest = parser.Parse(R"({
      "dependencies": [],
      "content_type": "MISC",
      "title": "MD11 Sound enhancement",
      "manufacturer": "Boeing",
      "creator": "aek sound",
      "package_version": "0.1",
      "minimum_game_version": "1.5.27",
      "total_package_size": "00000000000036803650"
    })");

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->title, std::string("MD11 Sound enhancement"));
    QCOMPARE(manifest->creator, std::string("aek sound"));
    QCOMPARE(manifest->manufacturer, std::string("Boeing"));
    QCOMPARE(manifest->contentType, std::string("MISC"));
    QCOMPARE(manifest->packageVersion, std::string("0.1"));
    QCOMPARE(manifest->minimumGameVersion, std::string("1.5.27"));
}

void JsonManifestParserTest::AByteOrderMarkDoesNotHideTheManifest()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> manifest =
        parser.Parse("\xEF\xBB\xBF{\"title\": \"Bijan Seasons\", \"content_type\": \"SCENERY\"}");

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->title, std::string("Bijan Seasons"));
    QCOMPARE(manifest->contentType, std::string("SCENERY"));
}

void JsonManifestParserTest::FreeTextFieldsSurviveWhateverTheAuthorWrote()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> lowercase = parser.Parse(R"({"content_type": "misc", "title": "AIGAIM AI Traffic"})");
    const std::optional<Manifest> spaced = parser.Parse(R"({"content_type": "Sound Set"})");
    const std::optional<Manifest> blank = parser.Parse(R"({"content_type": "", "creator": ""})");

    QVERIFY(lowercase.has_value());
    QVERIFY(spaced.has_value());
    QVERIFY(blank.has_value());
    QCOMPARE(lowercase->contentType, std::string("misc"));
    QCOMPARE(spaced->contentType, std::string("Sound Set"));
    QCOMPARE(blank->contentType, std::string());
    QCOMPARE(blank->creator, std::string());
    QCOMPARE(blank->title, std::string());
}

void JsonManifestParserTest::ContentThatIsNotAJsonObjectIsNotAManifest()
{
    const JsonManifestParser parser;

    QVERIFY(!parser.Parse("").has_value());
    QVERIFY(!parser.Parse("[]").has_value());
    QVERIFY(!parser.Parse("{\"title\": ").has_value());
}

QTEST_APPLESS_MAIN(JsonManifestParserTest)

#include "tst_json_manifest_parser.moc"
