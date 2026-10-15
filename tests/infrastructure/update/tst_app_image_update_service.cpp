#include <memory>
#include <string>
#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QtTest>

#include <unistd.h>

#include "application/ports/UpdateService.h"
#include "infrastructure/update/AppImageUpdateService.h"

namespace
{
    class AppImageUpdateServiceTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BuildingTheServiceLeavesWhateverIsAlreadyThere();
        static void DiscardingTakesTheStagedAndThePartialDownloadAndNothingElse();
        static void AnImageThatPassesTheChecksumIsStagedBesideTheRunningOneAndRunnable();
        static void AnImageThatFailsTheChecksumIsDeletedAndNothingIsStaged();
        static void AChecksumFileThatIsNotAHashStopsBeforeTheImageIsAsked();
        static void AReleaseWithoutTheAppImageStopsBeforeAnythingIsAsked();
        static void AnImageTheHostRefusesLeavesNothingBehind();
        static void AFolderTheUserCannotWriteNeverTouchesTheRunningImage();
        static void ProgressIsReportedWhileTheImageComes();
        static void ApplyingSwapsTheImageAndLeavesTheRunningProcessAlone();
        static void ApplyingWithNothingStagedLeavesTheRunningImage();
        static void ApplyingTwiceIsOneSwap();
        static void AStagedImageThatWasRemovedBehindOurBackIsNotApplied();
        static void TheNewImageStartsOnlyAfterTheOwnerIsGone();
        static void ANewerReleaseThanTheStagedOneDiscardsTheStaged();
    };
}

namespace
{
    constexpr auto kChecksumPath = "/checksum";
    constexpr auto kImagePath = "/image";
    constexpr auto kFeedPath = "/feed";
    constexpr auto kImageName = "fs-organizer-9.9.9-x86_64.AppImage";
    constexpr auto kRunningName = "fs-organizer.AppImage";
    constexpr auto kStagedName = ".fs-organizer-update.AppImage";
    constexpr auto kPartName = ".fs-organizer-update.AppImage.part";
    constexpr int kStageTimeoutMs = 30000;
    constexpr int kSettleMs = 300;
    constexpr int kImageBytes = 3'000'000;
    constexpr int kHttpNotFound = 404;
    constexpr int kHttpOk = 200;

    QByteArray TheNewImage()
    {
        QByteArray bytes("#!/bin/sh\nexit 0\n");

        for (int i = 0; bytes.size() < kImageBytes; ++i)
        {
            bytes.append(static_cast<char>(i % 251));
        }

        return bytes;
    }

    QByteArray ChecksumFileFor(const QByteArray& bytes)
    {
        return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() + "  " + kImageName + "\n";
    }

    QByteArray ReleaseOf(const QString& version)
    {
        return R"({"tag_name": "v)" + version.toUtf8() + R"(", "assets": []})";
    }

    bool RunningAsRoot()
    {
        return geteuid() == 0;
    }

    struct Install
    {
        QTemporaryDir directory;

        [[nodiscard]] QString Path(const char* name) const
        {
            return directory.path() + QLatin1Char('/') + QLatin1String(name);
        }

        [[nodiscard]] QString RunningImage() const
        {
            return Path(kRunningName);
        }

        [[nodiscard]] bool Put(const char* name, const QByteArray& bytes, const QFileDevice::Permissions mode) const
        {
            QFile file(Path(name));
            if (!file.open(QIODevice::WriteOnly))
            {
                return false;
            }

            file.write(bytes);
            file.close();

            return file.setPermissions(mode);
        }

        [[nodiscard]] bool Has(const char* name) const
        {
            return QFileInfo::exists(Path(name));
        }

        [[nodiscard]] QByteArray Read(const char* name) const
        {
            QFile file(Path(name));

            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
        }

        [[nodiscard]] QStringList Listing() const
        {
            QStringList names = QDir(directory.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
            names.sort();

            return names;
        }
    };

    constexpr QFileDevice::Permissions kUserRuns =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    constexpr QFileDevice::Permissions kUserReads = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

    struct FolderThatCannotBeWritten
    {
        explicit FolderThatCannotBeWritten(QString path) : folder(std::move(path))
        {
            QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
        }

        FolderThatCannotBeWritten(const FolderThatCannotBeWritten&) = delete;

        FolderThatCannotBeWritten& operator=(const FolderThatCannotBeWritten&) = delete;

        ~FolderThatCannotBeWritten()
        {
            QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        }

        QString folder;
    };

    struct ReleaseHost
    {
        QTcpServer server;
        QByteArray checksumFile;
        QByteArray image;
        QByteArray feed;
        int imageStatus = kHttpOk;
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

        [[nodiscard]] std::string UrlOf(const char* path) const
        {
            return (QStringLiteral("http://127.0.0.1:") + QString::number(server.serverPort()) + QLatin1String(path))
                .toStdString();
        }

        [[nodiscard]] bool WasAskedFor(const char* path) const
        {
            for (const QByteArray& line : asked)
            {
                if (line.startsWith(QByteArray("GET ") + path))
                {
                    return true;
                }
            }

            return false;
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

            int status = kHttpOk;
            QByteArray body;

            if (request.startsWith(QByteArray("GET ") + kChecksumPath))
            {
                body = checksumFile;
            }
            else if (request.startsWith(QByteArray("GET ") + kImagePath))
            {
                status = imageStatus;
                body = imageStatus == kHttpOk ? image : QByteArray{};
            }
            else
            {
                body = feed;
            }

            socket->write("HTTP/1.1 " + QByteArray::number(status)
                          + " Whatever\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
            socket->write(body);
        }
    };

    struct Outcome final : UpdateServiceObserver
    {
        void OnCheckFinished(const bool ok, const bool available, const UpdateInfo& info, const std::string&) override
        {
            ++checks;
            checkSucceeded = ok;
            updateAvailable = available;
            offeredVersion = info.version;
        }

        void OnDownloadProgress(const long long received, const long long total) override
        {
            ++progressReports;
            lastReceived = received;
            lastTotal = total;
        }

        void OnStageFinished(const bool ok, const std::string& error) override
        {
            ++stages;
            staged = ok;
            whyNot = error;
        }

        int checks = 0;
        bool checkSucceeded = false;
        bool updateAvailable = false;
        std::string offeredVersion;
        int progressReports = 0;
        long long lastReceived = 0;
        long long lastTotal = 0;
        int stages = 0;
        bool staged = false;
        std::string whyNot;
    };

    struct Attempt
    {
        Install install;
        ReleaseHost host;
        Outcome outcome;
        QByteArray runningBytes = "the image that is running now";
        std::unique_ptr<AppImageUpdateService> service;

        [[nodiscard]] bool Prepare(const QByteArray& checksumFile,
                                   const QByteArray& imageBytes,
                                   const qint64 ownerPid = QCoreApplication::applicationPid(),
                                   const QByteArray& feedBody = {})
        {
            host.checksumFile = checksumFile;
            host.image = imageBytes;
            host.feed = feedBody;

            if (!install.directory.isValid() || !install.Put(kRunningName, runningBytes, kUserRuns) || !host.Start())
            {
                return false;
            }

            service =
                std::make_unique<AppImageUpdateService>(QString::fromStdString(host.UrlOf(kFeedPath)),
                                                        QStringLiteral("0.1.0"), install.RunningImage(), ownerPid);
            service->AddObserver(&outcome);

            return true;
        }

        [[nodiscard]] UpdateInfo Offered() const
        {
            UpdateInfo info;
            info.version = "9.9.9";
            info.appImageName = kImageName;
            info.appImageShaUrl = host.UrlOf(kChecksumPath);
            info.appImageUrl = host.UrlOf(kImagePath);

            return info;
        }

        [[nodiscard]] bool Stage()
        {
            const int before = outcome.stages;
            service->DownloadAndStage(Offered());

            return QTest::qWaitFor(
                [this, before]
                {
                    return outcome.stages > before;
                },
                kStageTimeoutMs);
        }
    };

}

void AppImageUpdateServiceTest::BuildingTheServiceLeavesWhateverIsAlreadyThere()
{
    const Install install;
    QVERIFY(install.Put(kRunningName, "running", kUserRuns));
    QVERIFY(install.Put(kStagedName, "staged", kUserRuns));
    QVERIFY(install.Put(kPartName, "half", kUserReads));

    const AppImageUpdateService service({}, QStringLiteral("0.1.0"), install.RunningImage(), 1);

    QCOMPARE(install.Read(kRunningName), QByteArray("running"));
    QVERIFY2(install.Has(kStagedName), "building the service threw away a staged image nobody asked it to touch");
    QVERIFY2(install.Has(kPartName), "building the service threw away a download nobody asked it to touch");
}

void AppImageUpdateServiceTest::DiscardingTakesTheStagedAndThePartialDownloadAndNothingElse()
{
    const Install install;
    QVERIFY(install.Put(kRunningName, "running", kUserRuns));
    QVERIFY(install.Put(kStagedName, "staged", kUserRuns));
    QVERIFY(install.Put(kPartName, "half", kUserReads));
    QVERIFY(install.Put("notes.txt", "the user's own file", kUserReads));

    AppImageUpdateService service({}, QStringLiteral("0.1.0"), install.RunningImage(), 1);
    service.DiscardStaged();

    QVERIFY(!install.Has(kStagedName));
    QVERIFY(!install.Has(kPartName));
    QCOMPARE(install.Read(kRunningName), QByteArray("running"));
    QVERIFY2(install.Has("notes.txt"), "discarding took a file that is neither the staged image nor its download");
    QVERIFY(!service.HasStagedUpdate());
}

void AppImageUpdateServiceTest::AnImageThatPassesTheChecksumIsStagedBesideTheRunningOneAndRunnable()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image));

    QVERIFY2(attempt.Stage(), "the staging never finished");

    QVERIFY2(attempt.outcome.staged, attempt.outcome.whyNot.c_str());
    QVERIFY(attempt.service->HasStagedUpdate());
    QVERIFY2(attempt.install.Read(kStagedName) == image, "the staged image is not the bytes that were published");
    QVERIFY2(!attempt.install.Has(kPartName), "the partial download was left behind");
    QCOMPARE(attempt.install.Read(kRunningName), attempt.runningBytes);

    const QFileDevice::Permissions mode = QFile::permissions(attempt.install.Path(kStagedName));

    QVERIFY2(mode.testFlag(QFileDevice::ExeOwner) && mode.testFlag(QFileDevice::ExeGroup)
                 && mode.testFlag(QFileDevice::ExeOther),
             "the staged image cannot be executed");
    QStringList expected = {QString::fromLatin1(kStagedName), QString::fromLatin1(kRunningName)};
    expected.sort();

    QCOMPARE(attempt.install.Listing(), expected);
}

void AppImageUpdateServiceTest::AnImageThatFailsTheChecksumIsDeletedAndNothingIsStaged()
{
    Attempt attempt;
    QVERIFY(attempt.Prepare(ChecksumFileFor("bytes some other release published"), TheNewImage()));

    QVERIFY2(attempt.Stage(), "the staging never finished");

    QVERIFY2(!attempt.outcome.staged, "staging reported success over an image that failed the checksum");
    QVERIFY(!attempt.outcome.whyNot.empty());
    QVERIFY(!attempt.service->HasStagedUpdate());
    QVERIFY2(!attempt.install.Has(kStagedName), "an image that failed the checksum was staged");
    QVERIFY2(!attempt.install.Has(kPartName), "an image that failed the checksum was left on disk");
    QCOMPARE(attempt.install.Read(kRunningName), attempt.runningBytes);
    QVERIFY2(!attempt.service->LaunchApplyHelper(false), "an image that failed the checksum was applied");
    QCOMPARE(attempt.install.Read(kRunningName), attempt.runningBytes);
}

void AppImageUpdateServiceTest::AChecksumFileThatIsNotAHashStopsBeforeTheImageIsAsked()
{
    Attempt attempt;
    QVERIFY(attempt.Prepare("this is not a hash\n", TheNewImage()));

    QVERIFY2(attempt.Stage(), "the staging never finished");

    QVERIFY(!attempt.outcome.staged);
    QVERIFY2(!attempt.host.WasAskedFor(kImagePath), "the image was downloaded without a checksum to check it by");
    QVERIFY(!attempt.install.Has(kPartName));
    QVERIFY(!attempt.install.Has(kStagedName));
}

void AppImageUpdateServiceTest::AReleaseWithoutTheAppImageStopsBeforeAnythingIsAsked()
{
    Attempt attempt;
    QVERIFY(attempt.Prepare(ChecksumFileFor(TheNewImage()), TheNewImage()));

    UpdateInfo onlyTheZip;
    onlyTheZip.version = "9.9.9";
    onlyTheZip.zipUrl = attempt.host.UrlOf(kImagePath);
    onlyTheZip.shaUrl = attempt.host.UrlOf(kChecksumPath);

    attempt.service->DownloadAndStage(onlyTheZip);

    QCOMPARE(attempt.outcome.stages, 1);
    QVERIFY(!attempt.outcome.staged);
    QCOMPARE(attempt.host.asked.size(), 0);
}

void AppImageUpdateServiceTest::AnImageTheHostRefusesLeavesNothingBehind()
{
    Attempt attempt;
    QVERIFY(attempt.Prepare(ChecksumFileFor(TheNewImage()), TheNewImage()));
    attempt.host.imageStatus = kHttpNotFound;

    QVERIFY2(attempt.Stage(), "the staging never finished");

    QVERIFY(!attempt.outcome.staged);
    QVERIFY2(QString::fromStdString(attempt.outcome.whyNot).contains(QStringLiteral("404")),
             attempt.outcome.whyNot.c_str());
    QVERIFY(!attempt.install.Has(kPartName));
    QVERIFY(!attempt.install.Has(kStagedName));
    QCOMPARE(attempt.install.Read(kRunningName), attempt.runningBytes);
}

void AppImageUpdateServiceTest::AFolderTheUserCannotWriteNeverTouchesTheRunningImage()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image));

    const FolderThatCannotBeWritten locked(attempt.install.directory.path());

    QVERIFY2(attempt.Stage(), "the staging never finished");

    if (RunningAsRoot())
    {
        qInfo("running as root: the folder is writable to us whatever its mode, so the image is staged");
        QVERIFY2(attempt.outcome.staged, attempt.outcome.whyNot.c_str());
    }
    else
    {
        qInfo("running as a user: the folder cannot be written, so staging must fail");
        QVERIFY2(!attempt.outcome.staged, "staging reported success in a folder nobody can write");
        QVERIFY(!attempt.outcome.whyNot.empty());
        QVERIFY(!attempt.service->HasStagedUpdate());
        QVERIFY2(!attempt.host.WasAskedFor(kImagePath), "the image was downloaded into a folder that cannot take it");
    }

    QCOMPARE(attempt.install.Read(kRunningName), attempt.runningBytes);
}

void AppImageUpdateServiceTest::ProgressIsReportedWhileTheImageComes()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image));

    QVERIFY2(attempt.Stage(), "the staging never finished");

    QVERIFY(attempt.outcome.progressReports > 0);
    QCOMPARE(attempt.outcome.lastReceived, static_cast<long long>(image.size()));
    QCOMPARE(attempt.outcome.lastTotal, static_cast<long long>(image.size()));
}

void AppImageUpdateServiceTest::ApplyingSwapsTheImageAndLeavesTheRunningProcessAlone()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image));

    QVERIFY(attempt.install.Put(kRunningName, "#!/bin/sh\nexec sleep 30\n", kUserRuns));

    QProcess running;
    running.start(attempt.install.RunningImage(), {});
    QVERIFY2(running.waitForStarted(kStageTimeoutMs), "the image standing in for the running one did not start");

    QVERIFY2(attempt.Stage(), "the staging never finished");
    QVERIFY2(attempt.outcome.staged, attempt.outcome.whyNot.c_str());

    QVERIFY2(attempt.service->LaunchApplyHelper(false), "the swap over a running image failed");

    QVERIFY2(attempt.install.Read(kRunningName) == image, "the image in place is not the staged one");
    QVERIFY(!attempt.install.Has(kStagedName));
    QVERIFY(!attempt.service->HasStagedUpdate());
    QVERIFY(QFile::permissions(attempt.install.RunningImage()).testFlag(QFileDevice::ExeOwner));

    QTest::qWait(kSettleMs);

    QVERIFY2(running.state() == QProcess::Running, "the swap killed the process that was running the old image");

    running.kill();
    running.waitForFinished(kStageTimeoutMs);
}

void AppImageUpdateServiceTest::ApplyingWithNothingStagedLeavesTheRunningImage()
{
    const Install install;
    QVERIFY(install.Put(kRunningName, "running", kUserRuns));

    AppImageUpdateService service({}, QStringLiteral("0.1.0"), install.RunningImage(), 1);

    QVERIFY(!service.LaunchApplyHelper(false));
    QVERIFY(!service.LaunchApplyHelper(true));
    QCOMPARE(install.Read(kRunningName), QByteArray("running"));
}

void AppImageUpdateServiceTest::ApplyingTwiceIsOneSwap()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image));
    QVERIFY2(attempt.Stage(), "the staging never finished");
    QVERIFY(attempt.outcome.staged);

    QVERIFY(attempt.service->LaunchApplyHelper(false));
    QVERIFY(attempt.install.Put(kRunningName, "put there after the swap", kUserRuns));

    QVERIFY2(attempt.service->LaunchApplyHelper(false), "the second ask after a swap said it had failed");
    QCOMPARE(attempt.install.Read(kRunningName), QByteArray("put there after the swap"));
}

void AppImageUpdateServiceTest::AStagedImageThatWasRemovedBehindOurBackIsNotApplied()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image));
    QVERIFY2(attempt.Stage(), "the staging never finished");
    QVERIFY(attempt.outcome.staged);

    QVERIFY(QFile::remove(attempt.install.Path(kStagedName)));

    QVERIFY(!attempt.service->HasStagedUpdate());
    QVERIFY(!attempt.service->LaunchApplyHelper(false));
    QCOMPARE(attempt.install.Read(kRunningName), attempt.runningBytes);
}

void AppImageUpdateServiceTest::TheNewImageStartsOnlyAfterTheOwnerIsGone()
{
    QProcess owner;
    owner.start(QStringLiteral("sleep"), {QStringLiteral("30")});
    QVERIFY(owner.waitForStarted(kStageTimeoutMs));

    Attempt attempt;
    const QString marker = attempt.install.Path("relaunched.txt");
    const QByteArray script = "#!/bin/sh\necho relaunched > '" + marker.toUtf8() + "'\n";
    QVERIFY(attempt.Prepare(ChecksumFileFor(script), script, owner.processId()));

    QVERIFY2(attempt.Stage(), "the staging never finished");
    QVERIFY2(attempt.outcome.staged, attempt.outcome.whyNot.c_str());
    QVERIFY(attempt.service->LaunchApplyHelper(true));

    QVERIFY2(attempt.install.Read(kRunningName) == script, "the image in place is not the staged one");

    QTest::qWait(kSettleMs);
    QVERIFY2(!attempt.install.Has("relaunched.txt"), "the new image started while the old one was still running");

    owner.kill();
    owner.waitForFinished(kStageTimeoutMs);

    QTRY_VERIFY2_WITH_TIMEOUT(attempt.install.Has("relaunched.txt"),
                              "the new image never started once the owner was gone", kStageTimeoutMs);
}

void AppImageUpdateServiceTest::ANewerReleaseThanTheStagedOneDiscardsTheStaged()
{
    Attempt attempt;
    const QByteArray image = TheNewImage();
    QVERIFY(attempt.Prepare(ChecksumFileFor(image), image, QCoreApplication::applicationPid(),
                            ReleaseOf(QStringLiteral("10.0.0"))));
    QVERIFY2(attempt.Stage(), "the staging never finished");
    QVERIFY(attempt.service->HasStagedUpdate());

    attempt.service->CheckForUpdates();
    QTRY_VERIFY_WITH_TIMEOUT(attempt.outcome.checks == 1, kStageTimeoutMs);

    QVERIFY(attempt.outcome.checkSucceeded);
    QVERIFY(attempt.outcome.updateAvailable);
    QCOMPARE(QString::fromStdString(attempt.outcome.offeredVersion), QStringLiteral("10.0.0"));
    QVERIFY2(!attempt.service->HasStagedUpdate(), "an update older than the newest release stayed staged");
    QVERIFY(!attempt.install.Has(kStagedName));
}

QTEST_GUILESS_MAIN(AppImageUpdateServiceTest)

#include "tst_app_image_update_service.moc"
