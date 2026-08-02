#include <QtCore/QFile>
#include <QtTest/QtTest>

#include "infrastructure/update/GithubReleaseParser.h"

namespace
{
    class GithubReleaseParserTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ARealPayloadOfOurOwnReleaseGivesVersionZipAndChecksum();
        static void ARealPayloadOfAnotherProjectParsesWithoutOfferingAnything();
        static void JsonThatIsNotAReleaseIsRefused();
        static void AReleaseWithoutATagIsRefused();
        static void TheChecksumFileIsReadWhateverComesAfterTheHash();
        static void ChecksumContentThatIsNotAHashIsRefused();
        static void OnlyAHigherVersionCountsAsNewer();
    };
}

namespace
{
    QByteArray Fixture(const QString& name)
    {
        QFile file(QStringLiteral(FSORG_FIXTURES_DIR "/") + name);

        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }
}

void GithubReleaseParserTest::ARealPayloadOfOurOwnReleaseGivesVersionZipAndChecksum()
{
    const std::optional<UpdateInfo> info = ParseLatestRelease(Fixture(QStringLiteral("github-latest-release.json")));

    QVERIFY(info.has_value());
    QCOMPARE(QString::fromStdString(info->version), QStringLiteral("0.2.0"));
    QCOMPARE(QString::fromStdString(info->releasePageUrl),
             QStringLiteral("https://github.com/brunofgmag/fs-organizer/releases/tag/v0.2.0"));
    QCOMPARE(QString::fromStdString(info->zipName), QStringLiteral("fs-organizer-0.2.0.zip"));
    QVERIFY(QString::fromStdString(info->zipUrl).endsWith(QStringLiteral("/fs-organizer-0.2.0.zip")));
    QVERIFY(QString::fromStdString(info->shaUrl).endsWith(QStringLiteral("/fs-organizer-0.2.0.zip.sha256")));
}

void GithubReleaseParserTest::ARealPayloadOfAnotherProjectParsesWithoutOfferingAnything()
{
    const std::optional<UpdateInfo> info =
        ParseLatestRelease(Fixture(QStringLiteral("github-release-of-another-project.json")));

    QVERIFY(info.has_value());
    QCOMPARE(QString::fromStdString(info->version), QStringLiteral("2.97.0"));
    QVERIFY(info->zipUrl.empty());
    QVERIFY(info->shaUrl.empty());
}

void GithubReleaseParserTest::JsonThatIsNotAReleaseIsRefused()
{
    QVERIFY(!ParseLatestRelease("{not json").has_value());
    QVERIFY(!ParseLatestRelease("[]").has_value());
    QVERIFY(!ParseLatestRelease({}).has_value());
}

void GithubReleaseParserTest::AReleaseWithoutATagIsRefused()
{
    const QByteArray withoutTag = "{\"html_url\": \"https://example.com\"}";
    const QByteArray blankTag = "{\"tag_name\": \"   \"}";

    QVERIFY(!ParseLatestRelease(withoutTag).has_value());
    QVERIFY(!ParseLatestRelease(blankTag).has_value());
}

void GithubReleaseParserTest::TheChecksumFileIsReadWhateverComesAfterTheHash()
{
    const QByteArray hash(64, 'a');

    QCOMPARE(ParseSha256File(hash), QString::fromUtf8(hash));
    QCOMPARE(ParseSha256File(hash + "  fs-organizer-0.2.0.zip"), QString::fromUtf8(hash));
    QCOMPARE(ParseSha256File(hash + "  fs-organizer-0.2.0.zip\n"), QString::fromUtf8(hash));
    QCOMPARE(ParseSha256File(QByteArray(64, 'A')), QString::fromUtf8(QByteArray(64, 'a')));
}

void GithubReleaseParserTest::ChecksumContentThatIsNotAHashIsRefused()
{
    QVERIFY(ParseSha256File({}).isEmpty());
    QVERIFY(ParseSha256File("not a hash").isEmpty());
    QVERIFY(ParseSha256File(QByteArray(63, 'a')).isEmpty());
    QVERIFY(ParseSha256File(QByteArray(64, 'g')).isEmpty());
}

void GithubReleaseParserTest::OnlyAHigherVersionCountsAsNewer()
{
    QVERIFY(IsNewerVersion(QStringLiteral("v0.3.0"), QStringLiteral("0.2.0")));
    QVERIFY(IsNewerVersion(QStringLiteral("0.2.1"), QStringLiteral("0.2.0")));
    QVERIFY(!IsNewerVersion(QStringLiteral("v0.2.0"), QStringLiteral("0.2.0")));
    QVERIFY(!IsNewerVersion(QStringLiteral("v0.1.0"), QStringLiteral("0.2.0")));
    QVERIFY(!IsNewerVersion(QStringLiteral("nightly"), QStringLiteral("0.2.0")));
    QVERIFY(!IsNewerVersion(QStringLiteral("v0.2.0"), QString{}));
}

QTEST_APPLESS_MAIN(GithubReleaseParserTest)

#include "tst_github_release_parser.moc"
