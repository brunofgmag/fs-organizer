#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/scenery/JsonSceneryCache.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class JsonSceneryCacheOnRealDiskTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryFolderKeptByThreadsAtOnceIsRememberedByAFreshCache();
        static void EveryFolderSurvivesThreadsWritingTheFileWhileOthersKeep();
        static void ReadingWhileOtherThreadsKeepAndWriteNeverSeesAHalfWrittenEntry();
    };

    constexpr std::size_t kThreads = 8;
    constexpr std::size_t kFoldersPerThread = 2000;
    constexpr std::size_t kNeverWritesBeforeTheEnd = 0;
    constexpr std::size_t kWriteEvery = 500;

    [[nodiscard]] std::filesystem::path FolderOf(const std::size_t thread, const std::size_t number)
    {
        return PathUnder(PathFromUtf8("D:/Library/Sceneries"),
                         PathFromUtf8("thread-" + std::to_string(thread) + "/addon-" + std::to_string(number)));
    }

    [[nodiscard]] RememberedScenery SceneryOf(const std::size_t thread, const std::size_t number)
    {
        const std::string code = "T" + std::to_string(thread) + "N" + std::to_string(number);

        return {.readAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{1'700'000'000'000 + number}},
                .files = {SceneryCodes{.reading = SceneryReading::Read, .codes = {code}}}};
    }

    void KeepAndWrite(JsonSceneryCache& cache, const std::size_t thread, const std::size_t writeEvery)
    {
        for (std::size_t number = 0; number < kFoldersPerThread; ++number)
        {
            cache.Keep(FolderOf(thread, number), SceneryOf(thread, number));

            if (writeEvery != 0 && number % writeEvery == 0)
            {
                cache.WriteWhatIsKept();
            }
        }

        cache.WriteWhatIsKept();
    }

    void KeepAndWriteFromEveryThread(JsonSceneryCache& cache, const std::size_t writeEvery)
    {
        std::vector<std::thread> threads;
        threads.reserve(kThreads);

        for (std::size_t thread = 0; thread < kThreads; ++thread)
        {
            threads.emplace_back(KeepAndWrite, std::ref(cache), thread, writeEvery);
        }

        for (std::thread& running : threads)
        {
            running.join();
        }
    }

    [[nodiscard]] std::size_t HowManyAreMissingFrom(const std::filesystem::path& file)
    {
        const JsonSceneryCache fresh(file);
        std::size_t missing = 0;

        for (std::size_t thread = 0; thread < kThreads; ++thread)
        {
            for (std::size_t number = 0; number < kFoldersPerThread; ++number)
            {
                const std::optional<RememberedScenery> remembered = fresh.Remember(FolderOf(thread, number));

                if (!remembered.has_value() || remembered->files.size() != 1
                    || remembered->files.front().codes != SceneryOf(thread, number).files.front().codes)
                {
                    ++missing;
                }
            }
        }

        return missing;
    }
}

void JsonSceneryCacheOnRealDiskTest::EveryFolderKeptByThreadsAtOnceIsRememberedByAFreshCache()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::filesystem::path file = std::filesystem::path(directory.path().toStdString()) / "scenery-cache.json";

    {
        JsonSceneryCache cache(file);

        KeepAndWriteFromEveryThread(cache, kNeverWritesBeforeTheEnd);
    }

    QCOMPARE(HowManyAreMissingFrom(file), std::size_t{0});
}

void JsonSceneryCacheOnRealDiskTest::EveryFolderSurvivesThreadsWritingTheFileWhileOthersKeep()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::filesystem::path file = std::filesystem::path(directory.path().toStdString()) / "scenery-cache.json";

    {
        JsonSceneryCache cache(file);

        KeepAndWriteFromEveryThread(cache, kWriteEvery);
    }

    QCOMPARE(HowManyAreMissingFrom(file), std::size_t{0});
}

void JsonSceneryCacheOnRealDiskTest::ReadingWhileOtherThreadsKeepAndWriteNeverSeesAHalfWrittenEntry()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::filesystem::path file = std::filesystem::path(directory.path().toStdString()) / "scenery-cache.json";

    JsonSceneryCache cache(file);
    std::atomic<bool> writing = true;
    std::atomic<std::size_t> torn = 0;

    std::thread reader(
        [&]
        {
            while (writing)
            {
                for (std::size_t number = 0; number < kFoldersPerThread; number += 7)
                {
                    const std::optional<RememberedScenery> remembered = cache.Remember(FolderOf(0, number));

                    if (remembered.has_value() && remembered->files.size() != 1)
                    {
                        ++torn;
                    }
                }
            }
        });

    KeepAndWriteFromEveryThread(cache, kNeverWritesBeforeTheEnd);

    writing = false;
    reader.join();

    QCOMPARE(torn.load(), std::size_t{0});
    QVERIFY(cache.Remember(FolderOf(kThreads - 1, kFoldersPerThread - 1)).has_value());
}

QTEST_APPLESS_MAIN(JsonSceneryCacheOnRealDiskTest)

#include "tst_json_scenery_cache_on_real_disk.moc"
