#include <QtTest/QtTest>

#include "platform/Platform.h"

namespace
{
    class LinuxPlatformTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryPartTheCompositionHandsOutIsThere();
        static void LinuxHasOnlySymbolicLinksAndSaysSo();
        static void TheFoldersAreFilledSoNothingIsWrittenToTheWorkingDirectory();
    };
}

void LinuxPlatformTest::EveryPartTheCompositionHandsOutIsThere()
{
    const PlatformParts platform = BuildThePlatform();

    QVERIFY(platform.instance != nullptr);
    QVERIFY(platform.links != nullptr);
    QVERIFY(platform.filesystem != nullptr);
    QVERIFY(platform.files != nullptr);
    QVERIFY(platform.sidecars != nullptr);
    QVERIFY(platform.processes != nullptr);
    QVERIFY(platform.folderArchives != nullptr);
    QVERIFY(platform.packedArchives != nullptr);
    QVERIFY(platform.locator != nullptr);
    QVERIFY(platform.legacyConfig != nullptr);
}

void LinuxPlatformTest::LinuxHasOnlySymbolicLinksAndSaysSo()
{
    QVERIFY(!BuildThePlatform().capabilities.offersJunctions);
}

void LinuxPlatformTest::TheFoldersAreFilledSoNothingIsWrittenToTheWorkingDirectory()
{
    const AppFolders folders = BuildThePlatform().folders;

    for (const std::filesystem::path* path :
         {&folders.settingsFile, &folders.journalFile, &folders.presetsFolder, &folders.removedStartupEntriesFolder,
          &folders.bisectionFolder, &folders.sceneryCacheFile, &folders.documentIndexFile, &folders.manualFolder})
    {
        QVERIFY2(path->is_absolute(), path->c_str());
    }
}

QTEST_APPLESS_MAIN(LinuxPlatformTest)

#include "tst_linux_platform.moc"
