#include <QtTest/QtTest>

#include <algorithm>
#include <filesystem>
#include <vector>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>

#include "domain/support/PathUtils.h"
#include "infrastructure/manual/GithubManual.h"
#include "support/PathText.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class GithubManualTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFreshlyWrittenManualIsNeverAmongTheCopiesToForget();
        static void EveryOtherManualCopyInTheFolderIsForgotten();
        static void WhatIsNotAManualCopyIsLeftAlone();
    };

    void Write(const std::filesystem::path& file)
    {
        QFile born(AsText(file));
        born.open(QIODevice::WriteOnly | QIODevice::Truncate);
        born.write("%PDF-1.5");
        born.close();
    }

    [[nodiscard]] bool Forgets(const std::vector<std::filesystem::path>& forgotten, const std::filesystem::path& file)
    {
        return std::ranges::any_of(forgotten,
                                   [&file](const std::filesystem::path& one)
                                   {
                                       return one.filename() == file.filename();
                                   });
    }
}

void GithubManualTest::TheFreshlyWrittenManualIsNeverAmongTheCopiesToForget()
{
    QTemporaryDir dir;
    const std::filesystem::path folder = AsPath(dir.path());
    const std::filesystem::path kept = PathUnder(folder, PathFromUtf8("fs-organizer-en-0.54.1.pdf"));

    Write(kept);

    QVERIFY(!Forgets(ManualCopiesToForget(folder, kept), kept));
}

void GithubManualTest::EveryOtherManualCopyInTheFolderIsForgotten()
{
    QTemporaryDir dir;
    const std::filesystem::path folder = AsPath(dir.path());
    const std::filesystem::path kept = PathUnder(folder, PathFromUtf8("fs-organizer-en-0.54.1.pdf"));
    const std::filesystem::path older = PathUnder(folder, PathFromUtf8("fs-organizer-en-0.53.0.pdf"));
    const std::filesystem::path another = PathUnder(folder, PathFromUtf8("fs-organizer-pt_BR-0.54.1.pdf"));

    Write(kept);
    Write(older);
    Write(another);

    const std::vector<std::filesystem::path> forgotten = ManualCopiesToForget(folder, kept);

    QVERIFY(!Forgets(forgotten, kept));
    QVERIFY(Forgets(forgotten, older));
    QVERIFY(Forgets(forgotten, another));
}

void GithubManualTest::WhatIsNotAManualCopyIsLeftAlone()
{
    QTemporaryDir dir;
    const std::filesystem::path folder = AsPath(dir.path());
    const std::filesystem::path kept = PathUnder(folder, PathFromUtf8("fs-organizer-en-0.54.1.pdf"));
    const std::filesystem::path stranger = PathUnder(folder, PathFromUtf8("aerosoft-crj-checklist.pdf"));

    Write(kept);
    Write(stranger);

    QVERIFY(!Forgets(ManualCopiesToForget(folder, kept), stranger));
}

QTEST_APPLESS_MAIN(GithubManualTest)

#include "tst_github_manual.moc"
