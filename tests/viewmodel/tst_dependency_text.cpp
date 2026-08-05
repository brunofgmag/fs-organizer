#include <QtTest/QtTest>

#include <chrono>

#include "support/MomentText.h"
#include "viewmodel/DependencyText.h"

namespace
{
    class DependencyTextTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheLibraryAnswerSaysOnAndOffInWords();
        static void ThePackageTheSimulatorCarriesIsSaidToBeOutsideTheLibrary();
        static void TheThirdAnswerIsNotVerifiableAndNeverCallsTheEntryAProblem();
        static void TheListIsDatedForWhoeverReadsTheAnswer();
        static void TheAccountFolderIsNamedOnlyWhenThereWasSomethingToChooseBetween();
        static void WithNothingComingFromTheListThereIsNoNoteAtAll();
    };

    DependencyAnswer AnswerOf(const DependencyResolution resolution, const bool enabled = false)
    {
        return {.name = "fs-base-ui",
                .declaredVersion = "0.1.10",
                .resolution = resolution,
                .enabled = enabled,
                .libraryVersion = {}};
    }

    std::chrono::system_clock::time_point Moment()
    {
        return std::chrono::sys_days{std::chrono::year{2026} / std::chrono::August / 4} + std::chrono::hours{12};
    }
}

void DependencyTextTest::TheLibraryAnswerSaysOnAndOffInWords()
{
    const QString on = AnswerFor(AnswerOf(DependencyResolution::InThisLibrary, true));
    const QString off = AnswerFor(AnswerOf(DependencyResolution::InThisLibrary, false));

    QVERIFY(!on.isEmpty());
    QVERIFY(!off.isEmpty());
    QVERIFY(on != off);
    QVERIFY(on.contains(QStringLiteral("library")));
    QVERIFY(off.contains(QStringLiteral("library")));
}

void DependencyTextTest::ThePackageTheSimulatorCarriesIsSaidToBeOutsideTheLibrary()
{
    const QString said = AnswerFor(AnswerOf(DependencyResolution::InTheSimulator));

    QVERIFY(said.contains(QStringLiteral("simulator")));
    QVERIFY(said != AnswerFor(AnswerOf(DependencyResolution::InThisLibrary, true)));
}

void DependencyTextTest::TheThirdAnswerIsNotVerifiableAndNeverCallsTheEntryAProblem()
{
    const QString said = AnswerFor(AnswerOf(DependencyResolution::Unverifiable));

    QCOMPARE(said, QStringLiteral("Not verifiable"));

    for (const DependencyResolution resolution :
         {DependencyResolution::InThisLibrary, DependencyResolution::InTheSimulator,
          DependencyResolution::Unverifiable})
    {
        for (const QString& forbidden : {QStringLiteral("missing"), QStringLiteral("broken"), QStringLiteral("absent"),
                                         QStringLiteral("Missing"), QStringLiteral("Broken"), QStringLiteral("Absent")})
        {
            QVERIFY(!AnswerFor(AnswerOf(resolution, true)).contains(forbidden));
            QVERIFY(!AnswerFor(AnswerOf(resolution, false)).contains(forbidden));
        }
    }
}

void DependencyTextTest::TheListIsDatedForWhoeverReadsTheAnswer()
{
    DependencyReport report;
    report.answers.push_back(AnswerOf(DependencyResolution::InTheSimulator));
    report.listTakenAt = Moment();

    const QString note = WhereTheListCameFrom(report);

    QVERIFY(note.contains(AsMoment(Moment())));
}

void DependencyTextTest::TheAccountFolderIsNamedOnlyWhenThereWasSomethingToChooseBetween()
{
    DependencyReport named;
    named.answers.push_back(AnswerOf(DependencyResolution::InTheSimulator));
    named.listTakenAt = Moment();
    named.listAccountFolder = "NathosT";

    DependencyReport alone;
    alone.answers.push_back(AnswerOf(DependencyResolution::InTheSimulator));
    alone.listTakenAt = Moment();

    QVERIFY(WhereTheListCameFrom(named).contains(QStringLiteral("NathosT")));
    QVERIFY(!WhereTheListCameFrom(alone).contains(QStringLiteral("NathosT")));
}

void DependencyTextTest::WithNothingComingFromTheListThereIsNoNoteAtAll()
{
    DependencyReport fromTheLibraryAlone;
    fromTheLibraryAlone.answers.push_back(AnswerOf(DependencyResolution::InThisLibrary, true));

    QVERIFY(WhereTheListCameFrom(fromTheLibraryAlone).isEmpty());
    QVERIFY(WhereTheListCameFrom({}).isEmpty());
}

QTEST_APPLESS_MAIN(DependencyTextTest)

#include "tst_dependency_text.moc"
