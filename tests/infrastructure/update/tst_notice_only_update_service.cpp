#include <memory>
#include <string>
#include <utility>

#include <QtCore/QByteArrayList>
#include <QtCore/QFile>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QtTest>

#include "application/ports/UpdateService.h"
#include "infrastructure/update/NoticeOnlyUpdateService.h"

namespace
{
    class NoticeOnlyUpdateServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ANewerVersionIsAnnouncedAndNothingIsDownloaded();
        static void TheSameVersionIsUpToDate();
        static void NothingIsEverStagedOrApplied();
    };
}

namespace
{
    constexpr auto kFeedPath = "/releases/latest";
    constexpr int kAnswerTimeoutMs = 30000;
    constexpr int kSettleMs = 200;

    QByteArray TheLatestRelease()
    {
        QFile file(QStringLiteral(FSORG_FIXTURES_DIR "/github-latest-release.json"));

        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }

    struct ReleaseHost
    {
        QTcpServer server;
        QByteArray release;
        QByteArrayList asked;

        [[nodiscard]] bool Start()
        {
            if (!server.listen(QHostAddress::LocalHost, 0))
            {
                return false;
            }

            QObject::connect(&server, &QTcpServer::newConnection, &server,
                             [this]
                             {
                                 Accept();
                             });

            return true;
        }

        [[nodiscard]] QString FeedUrl() const
        {
            return QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort())
                + QLatin1String(kFeedPath);
        }

        void Accept()
        {
            QTcpSocket* socket = server.nextPendingConnection();
            const auto request = std::make_shared<QByteArray>();

            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                             [this, socket, request]
                             {
                                 request->append(socket->readAll());

                                 if (request->contains("\r\n\r\n"))
                                 {
                                     Answer(socket, *request);
                                 }
                             });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }

        void Answer(QTcpSocket* socket, const QByteArray& request)
        {
            asked.append(request.left(request.indexOf("\r\n")));

            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                          + QByteArray::number(release.size()) + "\r\nConnection: close\r\n\r\n");
            socket->write(release);
            socket->disconnectFromHost();
        }
    };

    struct Heard final : UpdateServiceObserver
    {
        void
        OnCheckFinished(const bool ok, const bool updateAvailable, const UpdateInfo& info, const std::string&) override
        {
            ++checks;
            succeeded = ok;
            available = updateAvailable;
            version = info.version;
        }

        void OnDownloadProgress(long long, long long) override
        {
            ++progress;
        }

        void OnStageFinished(bool, const std::string&) override
        {
            ++stages;
        }

        int checks = 0;
        int progress = 0;
        int stages = 0;
        bool succeeded = false;
        bool available = false;
        std::string version;
    };

    struct Checking
    {
        explicit Checking(QString version) : currentVersion(std::move(version))
        {
        }

        [[nodiscard]] bool Begin()
        {
            host.release = TheLatestRelease();

            if (host.release.isEmpty() || !host.Start())
            {
                return false;
            }

            service = std::make_unique<NoticeOnlyUpdateService>(host.FeedUrl(), currentVersion);
            service->AddObserver(&heard);
            service->CheckForUpdates();

            return true;
        }

        QString currentVersion;
        ReleaseHost host;
        Heard heard;
        std::unique_ptr<NoticeOnlyUpdateService> service;
    };
}

void NoticeOnlyUpdateServiceTest::ANewerVersionIsAnnouncedAndNothingIsDownloaded()
{
    Checking checking(QStringLiteral("0.1.0"));
    QVERIFY(checking.Begin());

    QTRY_VERIFY_WITH_TIMEOUT(checking.heard.checks == 1, kAnswerTimeoutMs);

    QVERIFY(checking.heard.succeeded);
    QVERIFY(checking.heard.available);
    QCOMPARE(QString::fromStdString(checking.heard.version), QStringLiteral("0.2.0"));

    UpdateInfo offered;
    offered.version = checking.heard.version;
    offered.zipUrl = checking.host.FeedUrl().toStdString() + "/fs-organizer-0.2.0.zip";
    offered.shaUrl = offered.zipUrl + ".sha256";
    checking.service->DownloadAndStage(offered);

    QTest::qWait(kSettleMs);

    QCOMPARE(checking.host.asked.size(), 1);
    QVERIFY(checking.host.asked.front().startsWith(QByteArray("GET ") + kFeedPath));
    QCOMPARE(checking.heard.progress, 0);
    QCOMPARE(checking.heard.stages, 0);
}

void NoticeOnlyUpdateServiceTest::TheSameVersionIsUpToDate()
{
    Checking checking(QStringLiteral("0.2.0"));
    QVERIFY(checking.Begin());

    QTRY_VERIFY_WITH_TIMEOUT(checking.heard.checks == 1, kAnswerTimeoutMs);

    QVERIFY(checking.heard.succeeded);
    QVERIFY(!checking.heard.available);
}

void NoticeOnlyUpdateServiceTest::NothingIsEverStagedOrApplied()
{
    NoticeOnlyUpdateService service({}, QStringLiteral("0.1.0"));

    service.DiscardStaged();

    QVERIFY(!service.HasStagedUpdate());
    QVERIFY(!service.LaunchApplyHelper(true));
    QVERIFY(!service.LaunchApplyHelper(false));
}

QTEST_GUILESS_MAIN(NoticeOnlyUpdateServiceTest)

#include "tst_notice_only_update_service.moc"
