#include <memory>
#include <string>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QtTest>

#include "application/ports/UpdateService.h"
#include "infrastructure/update/GithubUpdateService.h"

namespace
{
    class GithubUpdateServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BuildingTheServiceLeavesWhateverIsAlreadyThere();
        static void DiscardingTakesTheDownloadAndTheStagedAndNothingElse();
        static void TheDefaultFolderIsUnderTheApplicationData();
        static void AZipThatFailsTheChecksumIsDeletedAndNothingIsPrepared();
        static void AZipThatPassesTheChecksumSurvivesAndThePreparationGoesOn();
    };
}

namespace
{
    struct Updates
    {
        QTemporaryDir directory;

        [[nodiscard]] QString Root() const
        {
            return directory.path();
        }

        void Put(const QString& relativePath) const
        {
            const QString full = Root() + QLatin1Char('/') + relativePath;
            QDir().mkpath(QFileInfo(full).absolutePath());

            QFile file(full);
            file.open(QIODevice::WriteOnly);
            file.write("x");
        }

        [[nodiscard]] bool Has(const QString& relativePath) const
        {
            return QFileInfo::exists(Root() + QLatin1Char('/') + relativePath);
        }
    };

    constexpr auto kChecksumPath = "/checksum";
    constexpr auto kZipPath = "/package";
    constexpr auto kZipName = "fs-organizer-9.9.9.zip";
    constexpr auto kZipBytes = "the bytes the release actually published";
    constexpr int kStageTimeoutMs = 30000;

    QByteArray ChecksumFileFor(const QByteArray& bytes)
    {
        return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() + "  " + kZipName + "\n";
    }

    struct ReleaseHost
    {
        QTcpServer server;
        QByteArray checksumFile;
        QByteArray zip;

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

        [[nodiscard]] std::string UrlOf(const char* path) const
        {
            return (QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort()) + QLatin1String(path))
                .toStdString();
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

        void Answer(QTcpSocket* socket, const QByteArray& request) const
        {
            const QByteArray body = request.startsWith(QByteArray("GET ") + kChecksumPath) ? checksumFile : zip;

            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
            socket->write(body);
            socket->disconnectFromHost();
        }
    };

    struct StageOutcome final : UpdateServiceObserver
    {
        void OnCheckFinished(bool, bool, const UpdateInfo&, const std::string&) override
        {
        }

        void OnDownloadProgress(long long, long long) override
        {
        }

        void OnStageFinished(const bool ok, const std::string&) override
        {
            ++times;
            succeeded = ok;
        }

        int times = 0;
        bool succeeded = false;
    };

    struct StagingAttempt
    {
        Updates updates;
        ReleaseHost host;
        StageOutcome outcome;
        GithubUpdateService service{{}, QStringLiteral("0.1.0"), updates.Root()};

        [[nodiscard]] bool Begin(const QByteArray& checksumFile)
        {
            host.checksumFile = checksumFile;
            host.zip = QByteArray(kZipBytes);

            if (!host.Start())
            {
                return false;
            }

            service.AddObserver(&outcome);

            UpdateInfo info;
            info.version = "9.9.9";
            info.zipName = kZipName;
            info.shaUrl = host.UrlOf(kChecksumPath);
            info.zipUrl = host.UrlOf(kZipPath);

            service.DownloadAndStage(info);

            return true;
        }

        [[nodiscard]] bool KeptTheDownload() const
        {
            return updates.Has(QStringLiteral("download/") + QLatin1String(kZipName));
        }

        [[nodiscard]] bool StartedPreparing() const
        {
            return updates.Has(QStringLiteral("staged"));
        }
    };
}

void GithubUpdateServiceTest::BuildingTheServiceLeavesWhateverIsAlreadyThere()
{
    const Updates updates;
    updates.Put(QStringLiteral("staged/fs-organizer/fs-organizer.exe"));
    updates.Put(QStringLiteral("download/fs-organizer-0.2.0.zip"));

    const GithubUpdateService service({}, QStringLiteral("0.1.0"), updates.Root());

    QVERIFY2(updates.Has(QStringLiteral("staged/fs-organizer/fs-organizer.exe")),
             "building the service threw away a staged update nobody asked it to touch");
    QVERIFY2(updates.Has(QStringLiteral("download/fs-organizer-0.2.0.zip")),
             "building the service threw away a download nobody asked it to touch");
}

void GithubUpdateServiceTest::DiscardingTakesTheDownloadAndTheStagedAndNothingElse()
{
    const Updates updates;
    updates.Put(QStringLiteral("staged/fs-organizer/fs-organizer.exe"));
    updates.Put(QStringLiteral("download/fs-organizer-0.2.0.zip"));
    updates.Put(QStringLiteral("apply.ps1"));

    GithubUpdateService service({}, QStringLiteral("0.1.0"), updates.Root());
    service.DiscardStaged();

    QVERIFY(!updates.Has(QStringLiteral("staged/fs-organizer/fs-organizer.exe")));
    QVERIFY(!updates.Has(QStringLiteral("download/fs-organizer-0.2.0.zip")));
    QVERIFY2(updates.Has(QStringLiteral("apply.ps1")),
             "discarding took something that was neither a download nor a staged update");
}

void GithubUpdateServiceTest::TheDefaultFolderIsUnderTheApplicationData()
{
    const QString folder = GithubUpdateService::DefaultUpdatesFolder();

    QVERIFY(!folder.isEmpty());
    QVERIFY(folder.endsWith(QStringLiteral("/updates")));
}

void GithubUpdateServiceTest::AZipThatFailsTheChecksumIsDeletedAndNothingIsPrepared()
{
    StagingAttempt attempt;
    QVERIFY(attempt.Begin(ChecksumFileFor("bytes some other release published")));

    QTRY_VERIFY_WITH_TIMEOUT(attempt.outcome.times == 1, kStageTimeoutMs);

    QVERIFY2(!attempt.outcome.succeeded, "staging reported success over a zip that failed the checksum");
    QVERIFY2(!attempt.KeptTheDownload(), "the zip that failed the checksum was left on disk");
    QVERIFY2(!attempt.StartedPreparing(), "the update was prepared out of a zip that failed the checksum");
    QVERIFY(!attempt.service.HasStagedUpdate());
}

void GithubUpdateServiceTest::AZipThatPassesTheChecksumSurvivesAndThePreparationGoesOn()
{
    StagingAttempt attempt;
    QVERIFY(attempt.Begin(ChecksumFileFor(kZipBytes)));

    QTRY_VERIFY_WITH_TIMEOUT(attempt.outcome.times == 1, kStageTimeoutMs);

    QVERIFY2(attempt.KeptTheDownload(), "the zip that passed the checksum was deleted anyway");
    QVERIFY2(attempt.StartedPreparing(), "the preparation never started for a zip that passed the checksum");
}

QTEST_GUILESS_MAIN(GithubUpdateServiceTest)

#include "tst_github_update_service.moc"
