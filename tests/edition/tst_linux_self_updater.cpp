#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <unistd.h>

#include "edition/SelfUpdater.h"
#include "infrastructure/update/AppImageUpdateService.h"
#include "infrastructure/update/NoticeOnlyUpdateService.h"

namespace
{
    class LinuxSelfUpdaterTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void cleanup();
        static void WithoutAnAppImageTheCopyOnlyTellsAboutNewVersions();
        static void AnImageInAWritableFolderUpdatesItself();
        static void AnImageInAFolderNobodyCanWriteOnlyTellsAboutNewVersions();
    };
}

namespace
{
    constexpr auto kVariable = "APPIMAGE";

    bool Touch(const QString& path)
    {
        QFile file(path);

        return file.open(QIODevice::WriteOnly) && file.write("x") == 1;
    }
}

void LinuxSelfUpdaterTest::cleanup()
{
    qunsetenv(kVariable);
}

void LinuxSelfUpdaterTest::WithoutAnAppImageTheCopyOnlyTellsAboutNewVersions()
{
    qunsetenv(kVariable);

    const SelfUpdater updater = BuildTheSelfUpdater({}, QStringLiteral("0.1.0"));

    QCOMPARE(updater.delivery, UpdateDelivery::NoticeOnlyOnGithub);
    QVERIFY(dynamic_cast<NoticeOnlyUpdateService*>(updater.service.get()) != nullptr);
}

void LinuxSelfUpdaterTest::AnImageInAWritableFolderUpdatesItself()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString image = directory.path() + QStringLiteral("/fs-organizer.AppImage");
    QVERIFY(Touch(image));
    qputenv(kVariable, image.toUtf8());

    const SelfUpdater updater = BuildTheSelfUpdater({}, QStringLiteral("0.1.0"));

    QCOMPARE(updater.delivery, UpdateDelivery::SelfUpdate);
    QVERIFY(dynamic_cast<AppImageUpdateService*>(updater.service.get()) != nullptr);
}

void LinuxSelfUpdaterTest::AnImageInAFolderNobodyCanWriteOnlyTellsAboutNewVersions()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString image = directory.path() + QStringLiteral("/fs-organizer.AppImage");
    QVERIFY(Touch(image));
    qputenv(kVariable, image.toUtf8());

    QVERIFY(QFile::setPermissions(directory.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    const SelfUpdater updater = BuildTheSelfUpdater({}, QStringLiteral("0.1.0"));

    QFile::setPermissions(directory.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    if (geteuid() == 0)
    {
        qInfo("running as root: the folder is writable to us whatever its mode, so the copy updates itself");
        QCOMPARE(updater.delivery, UpdateDelivery::SelfUpdate);
    }
    else
    {
        qInfo("running as a user: the folder cannot be written, so the copy only tells");
        QCOMPARE(updater.delivery, UpdateDelivery::NoticeOnlyOnGithub);
        QVERIFY(dynamic_cast<NoticeOnlyUpdateService*>(updater.service.get()) != nullptr);
    }
}

QTEST_GUILESS_MAIN(LinuxSelfUpdaterTest)

#include "tst_linux_self_updater.moc"
