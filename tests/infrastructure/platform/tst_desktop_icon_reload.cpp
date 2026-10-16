#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "infrastructure/platform/DesktopIconReload.h"
#include "support/PathText.h"

namespace
{
    class DesktopIconReloadTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AThemeWithoutACacheIsLeftWithoutOne();
        static void AStaleCacheIsRebuiltWithTheIconWrittenSinceIt();
    };
}

namespace
{
    std::string ContentOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        std::ostringstream read;
        read << stream.rdbuf();

        return read.str();
    }

    bool CopyTheBrandIconTo(const std::filesystem::path& file)
    {
        std::filesystem::create_directories(file.parent_path());

        return QFile::copy(QStringLiteral(FSORG_BRAND_ICON), AsText(file));
    }
}

void DesktopIconReloadTest::AThemeWithoutACacheIsLeftWithoutOne()
{
    const QTemporaryDir dataHome;
    const std::filesystem::path theme = AsPath(dataHome.path()) / "icons" / "hicolor";

    QVERIFY(CopyTheBrandIconTo(theme / "256x256" / "apps" / "fs-organizer.png"));

    RefreshTheIconThemeCacheIn(theme);

    QVERIFY2(!std::filesystem::exists(theme / "icon-theme.cache"),
             "a cache nobody made goes stale the moment another program adds an icon without rebuilding it, so "
             "xdg-icon-resource never creates one, and neither does this");
}

void DesktopIconReloadTest::AStaleCacheIsRebuiltWithTheIconWrittenSinceIt()
{
    const QString tool = IconThemeCacheTool();
    if (tool.isEmpty())
    {
        QSKIP("neither gtk-update-icon-cache nor gtk4-update-icon-cache is installed here: the rebuild was not "
              "exercised");
    }

    const QTemporaryDir dataHome;
    const std::filesystem::path theme = AsPath(dataHome.path()) / "icons" / "hicolor";

    QVERIFY(CopyTheBrandIconTo(theme / "48x48" / "apps" / "some-other-program.png"));
    QCOMPARE(QProcess::execute(tool, {QStringLiteral("-f"), QStringLiteral("-t"), AsText(theme)}), 0);
    QVERIFY(ContentOf(theme / "icon-theme.cache").find("fs-organizer") == std::string::npos);

    QVERIFY(CopyTheBrandIconTo(theme / "256x256" / "apps" / "fs-organizer.png"));

    RefreshTheIconThemeCacheIn(theme);

    QVERIFY2(ContentOf(theme / "icon-theme.cache").find("fs-organizer") != std::string::npos,
             "GTK trusts a cache that is there, and one that does not list the icon hides it from every menu");
}

QTEST_GUILESS_MAIN(DesktopIconReloadTest)

#include "tst_desktop_icon_reload.moc"
