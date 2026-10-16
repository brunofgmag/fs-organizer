#include "infrastructure/platform/LinuxInstanceGuard.h"

#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QtLogging>
#include <QtNetwork/QLocalSocket>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include "support/PathText.h"

namespace
{
    constexpr int kConnectTimeoutMs = 1000;

    QString SocketBesides(const std::filesystem::path& lockFile)
    {
        return AsText(std::filesystem::path(lockFile).replace_extension(".sock"));
    }

    void BringTheMainWindowForward()
    {
        for (QWidget* window : QApplication::topLevelWidgets())
        {
            if (window->windowTitle() != QCoreApplication::applicationName())
            {
                continue;
            }

            window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            window->show();
            window->raise();
            window->activateWindow();
        }
    }
}

LinuxInstanceGuard::LinuxInstanceGuard(const std::filesystem::path& lockFile)
    : lock_(AsText(lockFile)), socketName_(SocketBesides(lockFile))
{
    std::error_code error;
    std::filesystem::create_directories(lockFile.parent_path(), error);

    lock_.setStaleLockTime(0);
    anotherIsRunning_ = !lock_.tryLock(0) && lock_.error() == QLockFile::LockFailedError;

    if (!anotherIsRunning_)
    {
        AnswerTheNextLaunch();
    }
}

bool LinuxInstanceGuard::AnotherIsRunning() const
{
    return anotherIsRunning_;
}

void LinuxInstanceGuard::BringTheRunningOneForward() const
{
    QLocalSocket socket;
    socket.connectToServer(socketName_);

    if (!socket.waitForConnected(kConnectTimeoutMs))
    {
        qWarning("FS Organizer is already running, but it did not answer, so this launch exits without raising it.");
        return;
    }

    socket.disconnectFromServer();

    if (socket.state() != QLocalSocket::UnconnectedState)
    {
        static_cast<void>(socket.waitForDisconnected(kConnectTimeoutMs));
    }
}

void LinuxInstanceGuard::AnswerTheNextLaunch()
{
    QLocalServer::removeServer(socketName_);

    server_ = std::make_unique<QLocalServer>();
    server_->setSocketOptions(QLocalServer::UserAccessOption);

    if (!server_->listen(socketName_))
    {
        qWarning("FS Organizer could not listen for a second launch, so a second launch will not raise this window.");
        return;
    }

    QObject::connect(server_.get(), &QLocalServer::newConnection, server_.get(),
                     [this]
                     {
                         while (QLocalSocket* launch = server_->nextPendingConnection())
                         {
                             launch->deleteLater();
                         }

                         BringTheMainWindowForward();
                     });
}
