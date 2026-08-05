#include <QtTest/QtTest>

#include "infrastructure/catalog/JsonManifestParser.h"

namespace
{
    class JsonManifestParserTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AManifestExposesTheFieldsTheTreeNeeds();
        static void AByteOrderMarkDoesNotHideTheManifest();
        static void FreeTextFieldsSurviveWhateverTheAuthorWrote();
        static void ContentThatIsNotAJsonObjectIsNotAManifest();
        static void EveryDeclaredDependencyComesOutWithItsNameAndItsVersion();
        static void ADependencyNameWithSpacesIsKeptWhole();
        static void ADependencyBlockThatIsNotAListDoesNotRejectTheManifest();
        static void ADependencyEntryWithoutANameIsNotADependency();
        static void AnEmptyDependencyBlockLeavesTheAddonWithoutDependencies();
        static void NothingElseFromTheBlockBecomesAField();
    };
}

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

void JsonManifestParserTest::EveryDeclaredDependencyComesOutWithItsNameAndItsVersion()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> manifest = parser.Parse(R"({
      "dependencies": [
        {"name": "fs-base-propdefs", "package_version": "0.1.2"},
        {"name": "fs-base-ui", "package_version": "0.1.10"},
        {"name": "asobo-vcockpits-core", "package_version": "0.1.12"}
      ],
      "title": "Sim Rate Selector",
      "package_version": "1.2.3"
    })");

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->dependencies.size(), std::size_t(3));
    QCOMPARE(manifest->dependencies[0].name, std::string("fs-base-propdefs"));
    QCOMPARE(manifest->dependencies[0].declaredVersion, std::string("0.1.2"));
    QCOMPARE(manifest->dependencies[1].name, std::string("fs-base-ui"));
    QCOMPARE(manifest->dependencies[2].name, std::string("asobo-vcockpits-core"));
    QCOMPARE(manifest->dependencies[2].declaredVersion, std::string("0.1.12"));
}

void JsonManifestParserTest::ADependencyNameWithSpacesIsKeptWhole()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> manifest =
        parser.Parse(R"({"dependencies": [{"name": "as a346 light mod", "package_version": "1.0"}]})");

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->dependencies.size(), std::size_t(1));
    QCOMPARE(manifest->dependencies[0].name, std::string("as a346 light mod"));
}

void JsonManifestParserTest::ADependencyBlockThatIsNotAListDoesNotRejectTheManifest()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> text = parser.Parse(R"({"dependencies": "fs-base-ui", "title": "Bijan Seasons"})");
    const std::optional<Manifest> object = parser.Parse(R"({"dependencies": {"name": "fs-base-ui"}, "title": "Nine"})");

    QVERIFY(text.has_value());
    QCOMPARE(text->title, std::string("Bijan Seasons"));
    QVERIFY(text->dependencies.empty());
    QVERIFY(object.has_value());
    QCOMPARE(object->title, std::string("Nine"));
    QVERIFY(object->dependencies.empty());
}

void JsonManifestParserTest::ADependencyEntryWithoutANameIsNotADependency()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> manifest = parser.Parse(R"({
      "dependencies": ["fs-base-ui", {"package_version": "0.1.10"}, {"name": "", "package_version": "1.0"},
                       {"name": "fs-base-propdefs"}],
      "title": "Airport"
    })");

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->title, std::string("Airport"));
    QCOMPARE(manifest->dependencies.size(), std::size_t(1));
    QCOMPARE(manifest->dependencies[0].name, std::string("fs-base-propdefs"));
    QCOMPARE(manifest->dependencies[0].declaredVersion, std::string());
}

void JsonManifestParserTest::AnEmptyDependencyBlockLeavesTheAddonWithoutDependencies()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> declared = parser.Parse(R"({"dependencies": [], "title": "Bijan Seasons"})");
    const std::optional<Manifest> absent = parser.Parse(R"({"title": "Bijan Seasons"})");

    QVERIFY(declared.has_value());
    QVERIFY(absent.has_value());
    QVERIFY(declared->dependencies.empty());
    QVERIFY(absent->dependencies.empty());
}

void JsonManifestParserTest::NothingElseFromTheBlockBecomesAField()
{
    const JsonManifestParser parser;

    const std::optional<Manifest> manifest = parser.Parse(R"({
      "dependencies": [{"name": "fs-base-ui", "package_version": "0.1.10", "optional": true, "title": "User Interface"}],
      "title": "Sim Rate Selector"
    })");

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->title, std::string("Sim Rate Selector"));
    QCOMPARE(manifest->dependencies.size(), std::size_t(1));
    QCOMPARE(manifest->dependencies[0].name, std::string("fs-base-ui"));
    QCOMPARE(manifest->dependencies[0].declaredVersion, std::string("0.1.10"));
}

QTEST_APPLESS_MAIN(JsonManifestParserTest)

#include "tst_json_manifest_parser.moc"
