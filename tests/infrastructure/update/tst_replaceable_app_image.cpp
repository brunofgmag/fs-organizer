#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include <unistd.h>

#include "infrastructure/update/ReplaceableAppImage.h"

namespace
{
    class ReplaceableAppImageTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnUnsetVariableMeansThereIsNoImageToReplace();
        static void AVariablePointingNowhereMeansThereIsNoImageToReplace();
        static void AFolderInPlaceOfTheImageIsNotAnImage();
        static void AnImageInAWritableFolderIsGivenBackAsItStands();
        static void AnImageReachedThroughALinkIsGivenBackAsTheFileItself();
        static void AnImageInAFolderNobodyCanWriteIsNotReplaceable();
    };
}

namespace
{
    bool Touch(const QString& path)
    {
        QFile file(path);

        return file.open(QIODevice::WriteOnly) && file.write("x") == 1;
    }

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
}

void ReplaceableAppImageTest::AnUnsetVariableMeansThereIsNoImageToReplace()
{
    QVERIFY(ReplaceableAppImage({}).isEmpty());
    QVERIFY(ReplaceableAppImage(QString{}).isEmpty());
}

void ReplaceableAppImageTest::AVariablePointingNowhereMeansThereIsNoImageToReplace()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QVERIFY(ReplaceableAppImage(directory.path() + QStringLiteral("/gone.AppImage")).isEmpty());
}

void ReplaceableAppImageTest::AFolderInPlaceOfTheImageIsNotAnImage()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QVERIFY(ReplaceableAppImage(directory.path()).isEmpty());
}

void ReplaceableAppImageTest::AnImageInAWritableFolderIsGivenBackAsItStands()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString image = directory.path() + QStringLiteral("/fs-organizer.AppImage");
    QVERIFY(Touch(image));

    QCOMPARE(ReplaceableAppImage(image), QFileInfo(image).canonicalFilePath());
}

void ReplaceableAppImageTest::AnImageReachedThroughALinkIsGivenBackAsTheFileItself()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QVERIFY(QDir().mkpath(directory.path() + QStringLiteral("/apps")));
    QVERIFY(QDir().mkpath(directory.path() + QStringLiteral("/bin")));

    const QString image = directory.path() + QStringLiteral("/apps/fs-organizer.AppImage");
    const QString link = directory.path() + QStringLiteral("/bin/fs-organizer");
    QVERIFY(Touch(image));
    QVERIFY(QFile::link(image, link));

    QCOMPARE(ReplaceableAppImage(link), QFileInfo(image).canonicalFilePath());
}

void ReplaceableAppImageTest::AnImageInAFolderNobodyCanWriteIsNotReplaceable()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString image = directory.path() + QStringLiteral("/fs-organizer.AppImage");
    QVERIFY(Touch(image));

    const FolderThatCannotBeWritten locked(directory.path());

    if (geteuid() == 0)
    {
        qInfo("running as root: the folder is writable to us whatever its mode, so the image is replaceable");
        QVERIFY(!ReplaceableAppImage(image).isEmpty());
    }
    else
    {
        qInfo("running as a user: the folder cannot be written, so the image is not replaceable");
        QVERIFY2(ReplaceableAppImage(image).isEmpty(), "an image in a folder nobody can write was called replaceable");
    }
}

QTEST_GUILESS_MAIN(ReplaceableAppImageTest)

#include "tst_replaceable_app_image.moc"
