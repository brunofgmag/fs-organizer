#include "infrastructure/platform/DesktopIconReload.h"

#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <system_error>

#include "support/PathText.h"

namespace
{
    constexpr int kEveryIconGroup = 0;
    constexpr int kLongestACacheRebuildMayTake = 10000;
}

QString IconThemeCacheTool()
{
    const QString gtk3 = QStandardPaths::findExecutable(QStringLiteral("gtk-update-icon-cache"));

    return gtk3.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("gtk4-update-icon-cache")) : gtk3;
}

void AskTheDesktopToReloadItsIcons()
{
    QDBusMessage changed = QDBusMessage::createSignal(
        QStringLiteral("/KIconLoader"), QStringLiteral("org.kde.KIconLoader"), QStringLiteral("iconChanged"));
    changed << kEveryIconGroup;

    static_cast<void>(QDBusConnection::sessionBus().send(changed));
}

void RefreshTheIconThemeCacheIn(const std::filesystem::path& theme)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(theme / "icon-theme.cache", error))
    {
        return;
    }

    const QString tool = IconThemeCacheTool();
    if (tool.isEmpty())
    {
        return;
    }

    QProcess rebuild;
    rebuild.start(tool, {QStringLiteral("-f"), QStringLiteral("-t"), AsText(theme)});
    static_cast<void>(rebuild.waitForFinished(kLongestACacheRebuildMayTake));
}
