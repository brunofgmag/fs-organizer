#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include "infrastructure/platform/LinuxKnownFolders.h"
#include "support/PathText.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class LinuxKnownFoldersTest : public QObject
    {
        Q_OBJECT

    private slots:
        void init();
        void cleanup();
        void TheConfigDataAndCacheHomesFollowTheXdgVariables();
        void WithoutTheVariablesTheHomesAreTheUsualOnesUnderTheHomeFolder();
        void TheSettingsLiveInTheConfigHomeAndTheCacheableFilesInTheCacheHome();
        void EverythingTheUserBuiltLivesInTheDataHome();
        void NothingIsCreatedOnDiskJustByAskingWhereItWouldGo();

    private:
        QTemporaryDir root_;
        QByteArray home_{};
        QByteArray config_{};
        QByteArray data_{};
        QByteArray cache_{};
    };
}

void LinuxKnownFoldersTest::init()
{
    home_ = qgetenv("HOME");
    config_ = qgetenv("XDG_CONFIG_HOME");
    data_ = qgetenv("XDG_DATA_HOME");
    cache_ = qgetenv("XDG_CACHE_HOME");

    QVERIFY(root_.isValid());

    qputenv("HOME", root_.filePath(QStringLiteral("home")).toUtf8());
    qputenv("XDG_CONFIG_HOME", root_.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", root_.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_CACHE_HOME", root_.filePath(QStringLiteral("cache")).toUtf8());
}

void LinuxKnownFoldersTest::cleanup()
{
    const auto restore = [](const char* name, const QByteArray& value)
    {
        if (value.isNull())
        {
            qunsetenv(name);
            return;
        }

        qputenv(name, value);
    };

    restore("HOME", home_);
    restore("XDG_CONFIG_HOME", config_);
    restore("XDG_DATA_HOME", data_);
    restore("XDG_CACHE_HOME", cache_);
}

void LinuxKnownFoldersTest::TheConfigDataAndCacheHomesFollowTheXdgVariables()
{
    QCOMPARE(ConfigHomeFolder(), AsPath(root_.filePath(QStringLiteral("config"))));
    QCOMPARE(DataHomeFolder(), AsPath(root_.filePath(QStringLiteral("data"))));
    QCOMPARE(CacheHomeFolder(), AsPath(root_.filePath(QStringLiteral("cache"))));
    QCOMPARE(HomeFolder(), AsPath(root_.filePath(QStringLiteral("home"))));
}

void LinuxKnownFoldersTest::WithoutTheVariablesTheHomesAreTheUsualOnesUnderTheHomeFolder()
{
    qunsetenv("XDG_CONFIG_HOME");
    qunsetenv("XDG_DATA_HOME");
    qunsetenv("XDG_CACHE_HOME");

    const std::filesystem::path home = AsPath(root_.filePath(QStringLiteral("home")));

    QCOMPARE(ConfigHomeFolder(), home / ".config");
    QCOMPARE(DataHomeFolder(), home / ".local" / "share");
    QCOMPARE(CacheHomeFolder(), home / ".cache");
}

void LinuxKnownFoldersTest::TheSettingsLiveInTheConfigHomeAndTheCacheableFilesInTheCacheHome()
{
    const AppFolders folders = LinuxAppFolders();

    QCOMPARE(folders.settingsFile, ConfigHomeFolder() / "fs-organizer" / "settings.json");
    QCOMPARE(folders.sceneryCacheFile, CacheHomeFolder() / "fs-organizer" / "scenery-cache.json");
    QCOMPARE(folders.documentIndexFile, CacheHomeFolder() / "fs-organizer" / "document-index.json");
    QCOMPARE(folders.manualFolder, CacheHomeFolder() / "fs-organizer" / "manual");
}

void LinuxKnownFoldersTest::EverythingTheUserBuiltLivesInTheDataHome()
{
    const AppFolders folders = LinuxAppFolders();
    const std::filesystem::path data = DataHomeFolder() / "fs-organizer";

    QCOMPARE(folders.journalFile, data / "journal" / "operations.jsonl");
    QCOMPARE(folders.presetsFolder, data / "presets");
    QCOMPARE(folders.removedStartupEntriesFolder, data / "removed-startup-entries");
    QCOMPARE(folders.bisectionFolder, data / "bisection");
}

void LinuxKnownFoldersTest::NothingIsCreatedOnDiskJustByAskingWhereItWouldGo()
{
    static_cast<void>(LinuxAppFolders());

    QVERIFY(QDir(root_.path()).isEmpty());
}

QTEST_APPLESS_MAIN(LinuxKnownFoldersTest)

#include "tst_linux_known_folders.moc"
