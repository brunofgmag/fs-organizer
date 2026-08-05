#include <QtTest/QtTest>
#include <QtWidgets/QLabel>

#include <chrono>
#include <vector>

#include "support/MomentText.h"
#include "view/panels/DependencySection.h"

namespace
{
    class DependencySectionTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnAddonThatDeclaresNothingGetsNoSectionAtAll();
        static void EveryDeclaredDependencyGetsItsOwnLineWithItsName();
        static void TheDeclaredVersionAndTheOneTheLibraryHoldsAppearSideBySide();
        static void TheSectionSaysWhereTheListCameFromWhenTheListAnswered();
        static void NothingInTheSectionEverCallsADependencyAProblem();
    };

    DependencyAnswer Answer(const std::string& name,
                            const DependencyResolution resolution,
                            const std::string& declared = {},
                            const std::string& held = {})
    {
        return {.name = name,
                .declaredVersion = declared,
                .resolution = resolution,
                .enabled = false,
                .libraryVersion = held};
    }

    QStringList EverythingWritten(const DependencySection& section)
    {
        QStringList written;

        for (const QLabel* label : section.findChildren<QLabel*>())
        {
            written.append(label->text());
        }

        return written;
    }
}

void DependencySectionTest::AnAddonThatDeclaresNothingGetsNoSectionAtAll()
{
    DependencySection section;
    section.Show({});

    QVERIFY(section.isHidden());

    DependencyReport declaring;
    declaring.answers.push_back(Answer("fs-base-ui", DependencyResolution::Unverifiable));
    section.Show(declaring);

    QVERIFY(!section.isHidden());
}

void DependencySectionTest::EveryDeclaredDependencyGetsItsOwnLineWithItsName()
{
    DependencyReport report;
    report.answers.push_back(Answer("pmdg-global-lib", DependencyResolution::InThisLibrary));
    report.answers.push_back(Answer("as a346 light mod", DependencyResolution::Unverifiable));
    report.answers.push_back(Answer("asobo-vcockpits-core", DependencyResolution::InTheSimulator));

    DependencySection section;
    section.Show(report);

    const QStringList written = EverythingWritten(section);

    QVERIFY(written.contains(QStringLiteral("pmdg-global-lib")));
    QVERIFY(written.contains(QStringLiteral("as a346 light mod")));
    QVERIFY(written.contains(QStringLiteral("asobo-vcockpits-core")));
    QVERIFY(written.contains(QStringLiteral("Dependencies · 3")));
}

void DependencySectionTest::TheDeclaredVersionAndTheOneTheLibraryHoldsAppearSideBySide()
{
    DependencyReport report;
    report.answers.push_back(Answer("pmdg-global-lib", DependencyResolution::InThisLibrary, "1.0.0", "1.0.1"));
    report.answers.push_back(Answer("navdata-base", DependencyResolution::Unverifiable));

    DependencySection section;
    section.Show(report);

    const QStringList written = EverythingWritten(section);

    QVERIFY(written.contains(QStringLiteral("needs 1.0.0")));
    QVERIFY(written.contains(QStringLiteral("has 1.0.1")));
    QCOMPARE(written.filter(QStringLiteral("needs ")).size(), 1);
    QCOMPARE(written.filter(QStringLiteral("has ")).size(), 1);
}

void DependencySectionTest::TheSectionSaysWhereTheListCameFromWhenTheListAnswered()
{
    const std::chrono::system_clock::time_point written =
        std::chrono::sys_days{std::chrono::year{2026} / std::chrono::August / 4} + std::chrono::hours{12};

    DependencyReport report;
    report.answers.push_back(Answer("asobo-vcockpits-core", DependencyResolution::InTheSimulator));
    report.listTakenAt = written;
    report.listAccountFolder = "NathosT";

    DependencySection section;
    section.Show(report);

    QCOMPARE(EverythingWritten(section).filter(QStringLiteral("NathosT")).size(), 1);
    QCOMPARE(EverythingWritten(section).filter(AsDay(written)).size(), 1);

    DependencyReport fromTheLibraryAlone;
    fromTheLibraryAlone.answers.push_back(Answer("pmdg-global-lib", DependencyResolution::InThisLibrary));
    section.Show(fromTheLibraryAlone);

    QVERIFY(EverythingWritten(section).filter(QStringLiteral("NathosT")).isEmpty());
}

void DependencySectionTest::NothingInTheSectionEverCallsADependencyAProblem()
{
    DependencyReport report;
    report.answers.push_back(Answer("pmdg-global-lib", DependencyResolution::InThisLibrary, "1.0.0", "1.0.1"));
    report.answers.push_back(Answer("navdata-base", DependencyResolution::Unverifiable, "2402"));
    report.answers.push_back(Answer("asobo-vcockpits-core", DependencyResolution::InTheSimulator, "0.1.12"));

    DependencySection section;
    section.Show(report);

    const QString written = EverythingWritten(section).join(QStringLiteral(" ")).toLower();

    QVERIFY(!written.contains(QStringLiteral("missing")));
    QVERIFY(!written.contains(QStringLiteral("broken")));
    QVERIFY(!written.contains(QStringLiteral("absent")));
    QVERIFY(!written.contains(QStringLiteral("unsatisfied")));
    QVERIFY(written.contains(QStringLiteral("not verifiable")));
}

QTEST_MAIN(DependencySectionTest)

#include "tst_dependency_section.moc"
