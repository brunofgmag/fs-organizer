#include "infrastructure/platform/DesktopIconReload.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

namespace
{
    constexpr int kEveryIconGroup = 0;
}

void AskTheDesktopToReloadItsIcons()
{
    QDBusMessage changed = QDBusMessage::createSignal(
        QStringLiteral("/KIconLoader"), QStringLiteral("org.kde.KIconLoader"), QStringLiteral("iconChanged"));
    changed << kEveryIconGroup;

    static_cast<void>(QDBusConnection::sessionBus().send(changed));
}
