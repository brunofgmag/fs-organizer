#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "infrastructure/platform/AppImageDesktopEntry.h"
#include "support/PathText.h"

namespace
{
    class AppImageDesktopEntryTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheEntryLaunchesTheImageAndKeepsEveryOtherLineOfTheShippedOne();
        static void APathWithCharactersTheExecKeyReservesIsEscapedTheWayTheSpecificationSays();
        static void InstallingWritesTheEntryAndTheIconUnderTheDataHome();
        static void InstallingAgainFromTheSamePlaceLeavesTheFilesAlone();
        static void InstallingFromANewPlaceRewritesWhatTheEntryLaunches();
        static void OnlyAnIconJustWrittenAsksTheDesktopToReloadItsIcons();
        static void AnIconJustWrittenTouchesTheThemeFolderTheWayXdgIconResourceDoes();
    };
}

namespace
{
    const std::filesystem::path kImage = "/home/pilot/Apps/fs-organizer-1.0.0-x86_64.AppImage";
    const std::string kIcon = "\x89PNG\r\n\x1a\nnot really an image";

    std::string ContentOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        std::ostringstream read;
        read << stream.rdbuf();

        return read.str();
    }

    std::string ShippedEntry()
    {
        return ContentOf(FSORG_DESKTOP_ENTRY);
    }

    QStringList LinesBesidesWhatItLaunches(const std::string& entry)
    {
        QStringList kept;

        for (const QString& line : QString::fromStdString(entry).split(QLatin1Char('\n')))
        {
            if (!line.startsWith(QStringLiteral("Exec=")) && !line.startsWith(QStringLiteral("TryExec=")))
            {
                kept << line;
            }
        }

        return kept;
    }

    QString LineStarting(const std::string& entry, const QString& key)
    {
        for (const QString& line : QString::fromStdString(entry).split(QLatin1Char('\n')))
        {
            if (line.startsWith(key))
            {
                return line;
            }
        }

        return {};
    }
}

void AppImageDesktopEntryTest::TheEntryLaunchesTheImageAndKeepsEveryOtherLineOfTheShippedOne()
{
    const std::string shipped = ShippedEntry();
    QVERIFY2(!shipped.empty(), "the shipped entry is the template, and without it the test compares nothing");

    const std::string entry = DesktopEntryLaunching(shipped, kImage);

    QCOMPARE(LinesBesidesWhatItLaunches(entry), LinesBesidesWhatItLaunches(shipped));
    QCOMPARE(LineStarting(entry, QStringLiteral("Exec=")),
             QStringLiteral("Exec=\"/home/pilot/Apps/fs-organizer-1.0.0-x86_64.AppImage\""));
    QVERIFY2(LineStarting(entry, QStringLiteral("TryExec=")) == QStringLiteral("TryExec=") + AsText(kImage),
             "the menu hides an entry whose TryExec names a file that is gone, which is what a deleted image leaves");
}

void AppImageDesktopEntryTest::APathWithCharactersTheExecKeyReservesIsEscapedTheWayTheSpecificationSays()
{
    const std::string entry =
        DesktopEntryLaunching(ShippedEntry(), "/home/pilot/My Apps/fs$org \"beta\" 100%.AppImage");

    QVERIFY2(LineStarting(entry, QStringLiteral("Exec="))
                 == QStringLiteral(R"(Exec="/home/pilot/My Apps/fs\\$org \\"beta\\" 100%%.AppImage")"),
             "inside quotes the dollar and the quote take a backslash, the backslash is doubled again as a string, "
             "and a percent sign is doubled so it is not read as a field code");
    QCOMPARE(LineStarting(entry, QStringLiteral("TryExec=")),
             QStringLiteral("TryExec=/home/pilot/My Apps/fs$org \"beta\" 100%.AppImage"));
}

void AppImageDesktopEntryTest::InstallingWritesTheEntryAndTheIconUnderTheDataHome()
{
    const QTemporaryDir dataHome;
    const std::filesystem::path home = AsPath(dataHome.path());

    QVERIFY(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).held);

    QCOMPARE(DesktopEntryUnder(home), home / "applications" / "fs-organizer.desktop");
    QCOMPARE(DesktopIconUnder(home), home / "icons" / "hicolor" / "256x256" / "apps" / "fs-organizer.png");
    QCOMPARE(ContentOf(DesktopEntryUnder(home)), DesktopEntryLaunching(ShippedEntry(), kImage));
    QCOMPARE(ContentOf(DesktopIconUnder(home)), kIcon);
}

void AppImageDesktopEntryTest::InstallingAgainFromTheSamePlaceLeavesTheFilesAlone()
{
    const QTemporaryDir dataHome;
    const std::filesystem::path home = AsPath(dataHome.path());

    QVERIFY(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).held);

    const auto longAgo = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24);
    std::filesystem::last_write_time(DesktopEntryUnder(home), longAgo);
    std::filesystem::last_write_time(DesktopIconUnder(home), longAgo);

    QVERIFY(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).held);

    QVERIFY2(std::filesystem::last_write_time(DesktopEntryUnder(home)) == longAgo,
             "every start runs the installer, and rewriting an unchanged entry makes the desktop rebuild its menus");
    QVERIFY(std::filesystem::last_write_time(DesktopIconUnder(home)) == longAgo);
}

void AppImageDesktopEntryTest::InstallingFromANewPlaceRewritesWhatTheEntryLaunches()
{
    const QTemporaryDir dataHome;
    const std::filesystem::path home = AsPath(dataHome.path());
    const std::filesystem::path moved = "/home/pilot/Downloads/fs-organizer-1.0.0-x86_64.AppImage";

    QVERIFY(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).held);
    QVERIFY(InstallTheDesktopEntry(moved, home, ShippedEntry(), kIcon).held);

    QCOMPARE(ContentOf(DesktopEntryUnder(home)), DesktopEntryLaunching(ShippedEntry(), moved));
}

void AppImageDesktopEntryTest::OnlyAnIconJustWrittenAsksTheDesktopToReloadItsIcons()
{
    const QTemporaryDir dataHome;
    const std::filesystem::path home = AsPath(dataHome.path());
    const std::filesystem::path moved = "/home/pilot/Downloads/fs-organizer-1.0.0-x86_64.AppImage";

    QVERIFY2(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).iconWritten,
             "a running menu keeps the lookup it made before the icon existed, and only this tells it to look again");
    QVERIFY2(!InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).iconWritten,
             "every start runs the installer, and asking every desktop process to reload its icons on each start "
             "is noise");
    QVERIFY(!InstallTheDesktopEntry(moved, home, ShippedEntry(), kIcon).iconWritten);
}

void AppImageDesktopEntryTest::AnIconJustWrittenTouchesTheThemeFolderTheWayXdgIconResourceDoes()
{
    const QTemporaryDir dataHome;
    const std::filesystem::path home = AsPath(dataHome.path());
    const std::filesystem::path theme = IconThemeUnder(home);

    QVERIFY(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon).held);
    QCOMPARE(theme, home / "icons" / "hicolor");

    const auto longAgo = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24);

    std::filesystem::last_write_time(theme, longAgo);
    QVERIFY(InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon + "a newer drawing").iconWritten);
    QVERIFY2(std::filesystem::last_write_time(theme) != longAgo,
             "GTK rescans a theme when the theme folder changes, and an icon written two folders down does not "
             "change it");
    QVERIFY2(!std::filesystem::exists(theme / ".fs-organizer-touch"), "the touch leaves nothing behind");

    std::filesystem::last_write_time(theme, longAgo);
    QVERIFY(!InstallTheDesktopEntry(kImage, home, ShippedEntry(), kIcon + "a newer drawing").iconWritten);
    QVERIFY(std::filesystem::last_write_time(theme) == longAgo);
}

QTEST_APPLESS_MAIN(AppImageDesktopEntryTest)

#include "tst_app_image_desktop_entry.moc"
