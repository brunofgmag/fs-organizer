#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "application/SceneryService.h"
#include "domain/support/PathUtils.h"
#include "tests/doubles/FakeClock.h"
#include "tests/doubles/FakeFilesystemProbe.h"
#include "tests/doubles/FakeSceneryCache.h"
#include "tests/doubles/FakeSceneryParser.h"
#include "tests/doubles/InMemoryFileSystem.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class SceneryServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheCodesOfAnAddonComeFromTheSceneryFolderAndNowhereElse();
        static void AFileThatDoesNotPassThePrefilterIsNeverReadWhole();
        static void TheSecondAskAnswersFromTheCacheWithoutOpeningAFile();
        static void SceneryChangedOnDiskIsReadAgainInsteadOfAnsweredFromTheCache();
        static void TheBatchAndTheSingleAskWriteToTheSameCache();
        static void TheBatchReportsProgressAndStopsWhereItIsToldTo();
        static void AnAddonWithNoSceneryFolderIsAnsweredWithoutReadingAnything();
        static void AFileChangedBelowTheFirstLevelOfTheSceneryFolderIsSeen();
        static void TheBatchToldToReadAgainReadsAgainInsteadOfAnsweringFromTheCache();
        static void ASceneryFileThatEndsEarlyIsCarriedAsARecordThatWasNotRead();
    };

    const std::filesystem::path kLibrary = PathFromUtf8("D:/Library/Sceneries");

    [[nodiscard]] AddonToRead Addon(const std::string& folderName)
    {
        return {.addon = {.libraryId = "library-1", .folderName = folderName},
                .folder = PathUnder(kLibrary, PathFromUtf8(folderName))};
    }

    [[nodiscard]] QStringList Listed(const std::vector<std::string>& codes)
    {
        QStringList text;
        for (const std::string& code : codes)
        {
            text << QString::fromStdString(code);
        }

        return text;
    }

    [[nodiscard]] QStringList CodesOf(const SceneryOfAnAddon& scenery)
    {
        QStringList codes;
        for (const SceneryCodes& file : scenery.files)
        {
            codes << Listed(file.codes);
        }

        return codes;
    }

    struct Reading
    {
        InMemoryFileSystem fileSystem{};
        FakeFilesystemProbe filesystemProbe{fileSystem};
        FakeSceneryParser parser{};
        FakeClock clock{};
        FakeSceneryCache cache{};

        [[nodiscard]] SceneryService Service()
        {
            return {filesystemProbe, parser, clock, cache};
        }

        void Touch(const std::filesystem::path& folder)
        {
            fileSystem.SetLastWriteTime(folder, clock.now);
        }
    };
}

void SceneryServiceTest::TheCodesOfAnAddonComeFromTheSceneryFolderAndNowhereElse()
{
    Reading reading;
    const std::filesystem::path addon = Addon("someone-airport-eham").folder;

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "APX.bgl", FakeSceneryParser::Carrying({"EHAM"}));
    reading.fileSystem.AddFileWithContents(addon / "scenery" / "world" / "deep.bgl",
                                           FakeSceneryParser::Carrying({"LPMA"}));
    reading.fileSystem.AddFileWithContents(addon / "Contrail" / "src" / "elsewhere.bgl",
                                           FakeSceneryParser::Carrying({"KJFK"}));
    reading.fileSystem.AddFileWithContents(addon / "scenery-old" / "older.bgl", FakeSceneryParser::Carrying({"KLAX"}));

    SceneryService service = reading.Service();
    const SceneryOfAnAddon scenery = service.SceneryOf(Addon("someone-airport-eham"));

    QCOMPARE(CodesOf(scenery).size(), 2);
    QVERIFY(CodesOf(scenery).contains(QStringLiteral("EHAM")));
    QVERIFY2(CodesOf(scenery).contains(QStringLiteral("LPMA")),
             "the scenery folder is walked whole, so a file below a subfolder of it still counts");
    QVERIFY2(!CodesOf(scenery).contains(QStringLiteral("KJFK")),
             "a bgl outside the scenery folder is not read, which is what makes the batch cheap");
    QVERIFY(!CodesOf(scenery).contains(QStringLiteral("KLAX")));
}

void SceneryServiceTest::AFileThatDoesNotPassThePrefilterIsNeverReadWhole()
{
    Reading reading;
    const std::filesystem::path addon = Addon("someone-models").folder;

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "not-a-bgl.txt", FakeSceneryParser::Carrying({"EHAM"}));
    reading.fileSystem.AddFileWithContents(addon / "scenery" / "models.bgl", "no signature here");

    SceneryService service = reading.Service();
    const SceneryOfAnAddon scenery = service.SceneryOf(Addon("someone-models"));

    QVERIFY(scenery.files.empty());
    QCOMPARE(reading.parser.prefiltered, std::size_t{1});
    QCOMPARE(reading.parser.parsed, std::size_t{0});
}

void SceneryServiceTest::TheSecondAskAnswersFromTheCacheWithoutOpeningAFile()
{
    Reading reading;
    const std::filesystem::path addon = Addon("someone-airport-eham").folder;

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "APX.bgl", FakeSceneryParser::Carrying({"EHAM"}));
    reading.Touch(addon);
    reading.Touch(addon / "scenery");

    SceneryService service = reading.Service();
    QCOMPARE(CodesOf(service.SceneryOf(Addon("someone-airport-eham"))), QStringList({"EHAM"}));

    const std::size_t opened = reading.parser.prefiltered;

    QCOMPARE(CodesOf(service.SceneryOf(Addon("someone-airport-eham"))), QStringList({"EHAM"}));
    QCOMPARE(reading.parser.prefiltered, opened);
    QCOMPARE(reading.cache.kept, std::size_t{1});
}

void SceneryServiceTest::SceneryChangedOnDiskIsReadAgainInsteadOfAnsweredFromTheCache()
{
    Reading reading;
    const std::filesystem::path addon = Addon("someone-airport-eham").folder;

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "APX.bgl", FakeSceneryParser::Carrying({"EHAM"}));
    reading.Touch(addon);
    reading.Touch(addon / "scenery");

    SceneryService service = reading.Service();
    QCOMPARE(CodesOf(service.SceneryOf(Addon("someone-airport-eham"))), QStringList({"EHAM"}));

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "APX.bgl", FakeSceneryParser::Carrying({"LPMA"}));
    reading.fileSystem.SetLastWriteTime(addon / "scenery", reading.clock.now + std::chrono::hours{1});

    QCOMPARE(CodesOf(service.SceneryOf(Addon("someone-airport-eham"))), QStringList({"LPMA"}));
    QCOMPARE(reading.cache.kept, std::size_t{2});
}

void SceneryServiceTest::TheBatchAndTheSingleAskWriteToTheSameCache()
{
    Reading reading;

    for (const std::string& name : {std::string("one-eham"), std::string("another-eham")})
    {
        reading.fileSystem.AddFileWithContents(Addon(name).folder / "scenery" / "APX.bgl",
                                               FakeSceneryParser::Carrying({"EHAM"}));
        reading.Touch(Addon(name).folder);
        reading.Touch(Addon(name).folder / "scenery");
    }

    SceneryService service = reading.Service();
    static_cast<void>(service.SceneryOf(Addon("one-eham")));

    const std::size_t openedByTheSingleAsk = reading.parser.prefiltered;

    const std::vector<SceneryOfAnAddon> scenery = service.SceneryOfEach({Addon("one-eham"), Addon("another-eham")}, {});

    QCOMPARE(scenery.size(), std::size_t{2});
    QVERIFY2(reading.parser.prefiltered == openedByTheSingleAsk + 1,
             "the batch reuses what the ask at the enable gesture already wrote, because both write the same cache");
    QCOMPARE(reading.cache.known.size(), std::size_t{2});
}

void SceneryServiceTest::TheBatchReportsProgressAndStopsWhereItIsToldTo()
{
    Reading reading;

    for (const std::string& name : {std::string("one"), std::string("two"), std::string("three")})
    {
        reading.fileSystem.AddFileWithContents(Addon(name).folder / "scenery" / "APX.bgl",
                                               FakeSceneryParser::Carrying({"EHAM"}));
    }

    std::vector<std::size_t> reported;

    SceneryService service = reading.Service();
    const std::vector<SceneryOfAnAddon> scenery =
        service.SceneryOfEach({Addon("one"), Addon("two"), Addon("three")},
                              [&reported](const std::size_t read, const std::size_t outOf)
                              {
                                  reported.push_back(outOf);

                                  return read < 2;
                              });

    QCOMPARE(scenery.size(), std::size_t{2});
    QCOMPARE(reported.size(), std::size_t{2});
    QVERIFY2(reported.front() == std::size_t{3},
             "the total is known before the walk, so the bar is a bar and not a spinner");
}

void SceneryServiceTest::AnAddonWithNoSceneryFolderIsAnsweredWithoutReadingAnything()
{
    Reading reading;

    reading.fileSystem.AddFileWithContents(Addon("someone-aircraft").folder / "SimObjects" / "model.bgl",
                                           FakeSceneryParser::Carrying({"EHAM"}));

    SceneryService service = reading.Service();

    QVERIFY(service.SceneryOf(Addon("someone-aircraft")).files.empty());
    QCOMPARE(reading.parser.prefiltered, std::size_t{0});
    QCOMPARE(reading.filesystemProbe.TimesItReadSomethingEndingIn(".bgl"), std::size_t{0});
}

void SceneryServiceTest::AFileChangedBelowTheFirstLevelOfTheSceneryFolderIsSeen()
{
    Reading reading;
    const std::filesystem::path addon = Addon("someone-airport-eham").folder;

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "world" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"EHAM"}));
    reading.Touch(addon);
    reading.Touch(addon / "scenery");
    reading.Touch(addon / "scenery" / "world");

    SceneryService service = reading.Service();
    QCOMPARE(CodesOf(service.SceneryOf(Addon("someone-airport-eham"))), QStringList({"EHAM"}));

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "world" / "APX.bgl",
                                           FakeSceneryParser::Carrying({"LPMA"}));
    reading.fileSystem.SetLastWriteTime(addon / "scenery" / "world", reading.clock.now + std::chrono::hours{1});

    QVERIFY2(CodesOf(service.SceneryOf(Addon("someone-airport-eham"))) == QStringList({"LPMA"}),
             "writing a file stamps the folder holding it and none above, and most scenery files of a real library "
             "sit below the first level, so watching the scenery folder alone would miss almost all of them");
}

void SceneryServiceTest::TheBatchToldToReadAgainReadsAgainInsteadOfAnsweringFromTheCache()
{
    Reading reading;
    const std::filesystem::path addon = Addon("one-eham").folder;

    reading.fileSystem.AddFileWithContents(addon / "scenery" / "APX.bgl", FakeSceneryParser::Carrying({"EHAM"}));
    reading.Touch(addon);
    reading.Touch(addon / "scenery");

    SceneryService service = reading.Service();
    static_cast<void>(service.SceneryOfEach({Addon("one-eham")}, {}));

    const std::size_t opened = reading.parser.prefiltered;

    static_cast<void>(service.SceneryOfEach({Addon("one-eham")}, {}, SceneryFreshness::ReuseWhatIsKnown));
    QCOMPARE(reading.parser.prefiltered, opened);

    static_cast<void>(service.SceneryOfEach({Addon("one-eham")}, {}, SceneryFreshness::ReadAgain));

    QVERIFY2(reading.parser.prefiltered > opened,
             "opening the section reuses what is known, and the button that says it reads them again has to read "
             "them again, in the mould of the Remedir of the size screen");
}

void SceneryServiceTest::ASceneryFileThatEndsEarlyIsCarriedAsARecordThatWasNotRead()
{
    Reading reading;

    reading.fileSystem.AddFileWithContents(Addon("someone-airport-eham").folder / "scenery" / "APX.bgl",
                                           FakeSceneryParser::ThatEndsEarly());

    SceneryService service = reading.Service();
    const SceneryOfAnAddon scenery = service.SceneryOf(Addon("someone-airport-eham"));

    QCOMPARE(scenery.files.size(), std::size_t{1});
    QCOMPARE(scenery.files.front().reading, SceneryReading::ItEndsBeforeItSaysItDoes);
    QVERIFY2(AirportsOfEachAddon({scenery}).front().evidence == AirportEvidence::ARecordWasNotRead,
             "a truncated file is not an addon that carries no airport, and the diagnostics section counts the two "
             "apart, which is the whole point of the US-118.9");
}

QTEST_APPLESS_MAIN(SceneryServiceTest)

#include "tst_scenery_service.moc"
