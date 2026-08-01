#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>

#include "infrastructure/update/GithubUpdateService.h"

class GithubUpdateServiceTest : public QObject
{
    Q_OBJECT

private slots:
    static void BuildingTheServiceLeavesWhateverIsAlreadyThere();
    static void DiscardingTakesTheDownloadAndTheStagedAndNothingElse();
    static void TheDefaultFolderIsUnderTheApplicationData();
};

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

QTEST_GUILESS_MAIN(GithubUpdateServiceTest)

#include "tst_github_update_service.moc"
