#include <QtTest/QtTest>

namespace
{
    class SmokeTest : public QObject
    {
        Q_OBJECT

    private slots:
        void QtRuntimeMatchesBuild();
    };
}

void SmokeTest::QtRuntimeMatchesBuild()
{
    const QString runtime = QString::fromLatin1(qVersion()).section(u'.', 0, 1);
    const QString build = QStringLiteral(QT_VERSION_STR).section(u'.', 0, 1);
    QCOMPARE(runtime, build);
}

QTEST_APPLESS_MAIN(SmokeTest)

#include "tst_smoke.moc"
