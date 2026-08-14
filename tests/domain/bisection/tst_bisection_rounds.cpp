#include <QtTest/QtTest>

#include "domain/bisection/BisectionRounds.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"

namespace
{
    class BisectionRoundsTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheFirstApplicationIsTheReferenceRoundWithNothingOn();
        static void AReferenceRoundThatCrashesPutsTheCauseOutsideWhatTheAppManages();
        static void AReferenceRoundThatRanFineOpensTheFirstRoundOverEveryUnit();
        static void ARoundTurnsOnHalfTheSuspectsRoundedDown();
        static void NoRoundEverTurnsOnEverySuspect();
        static void AnswerCrashedKeepsTheHalfThatWasOn();
        static void AnswerRanFineKeepsTheHalfThatWasOff();
        static void NoRoundOfTheFirstPassTurnsOnPartOfAGroup();
        static void TheSearchStopsInsteadOfRunningARoundOverOneUnit();
        static void AUnitOfOneAddonIsTheFirstOutcome();
        static void AUnitOfSeveralAddonsIsTheIrreducibleSet();
        static void TheWorstCaseIsTheCeilingOfTheLogarithm();
        static void TheSecondPassSplitsTheSatellitesAndKeepsTheBaseOnInEveryRound();
        static void TheSecondPassIsNotPossibleWithoutAnIdentifiableBase();
        static void AFirstPassOverTwentyUnitsConvergesInFiveRounds();
    };
}

namespace
{
    std::filesystem::path AddonNumber(const std::size_t number)
    {
        return PathFromUtf8("D:/MSFS 2024/Aircrafts/addon-" + std::to_string(number));
    }

    std::vector<SearchUnit> LonelyUnits(const std::size_t howMany)
    {
        std::vector<SearchUnit> units;
        units.reserve(howMany);

        for (std::size_t number = 0; number < howMany; ++number)
        {
            units.push_back(SearchUnit{.addons = {AddonNumber(number)}});
        }

        return units;
    }

    [[nodiscard]] SearchUnit AGroupOf(const std::size_t howMany)
    {
        SearchUnit unit;
        unit.base = PathFromUtf8("D:/MSFS 2024/Aircrafts (2024)/base");
        unit.addons.push_back(*unit.base);

        for (std::size_t number = 0; number < howMany; ++number)
        {
            unit.addons.push_back(PathFromUtf8("D:/MSFS 2024/Liveries/livery-" + std::to_string(number)));
        }

        return unit;
    }

    [[nodiscard]] BisectionRun PastTheReferenceRound(const std::vector<SearchUnit>& units)
    {
        return AfterAnswering(RunOver(units), BisectionAnswer::ItRanFine);
    }
}

void BisectionRoundsTest::TheFirstApplicationIsTheReferenceRoundWithNothingOn()
{
    const BisectionRun run = RunOver(LonelyUnits(8));
    const BisectionRound round = TheRound(run);

    QCOMPARE(round.number, std::size_t{0});
    QVERIFY(round.unitsOn.empty());
    QVERIFY(round.addonsOn.empty());
    QCOMPARE(OutcomeOf(run), BisectionOutcome::StillSearching);
}

void BisectionRoundsTest::AReferenceRoundThatCrashesPutsTheCauseOutsideWhatTheAppManages()
{
    const BisectionRun answered = AfterAnswering(RunOver(LonelyUnits(8)), BisectionAnswer::ItCrashed);

    QCOMPARE(OutcomeOf(answered), BisectionOutcome::NotAmongTheManagedOnes);
    QVERIFY(WhatIsLeft(answered).empty());
}

void BisectionRoundsTest::AReferenceRoundThatRanFineOpensTheFirstRoundOverEveryUnit()
{
    const BisectionRun answered = PastTheReferenceRound(LonelyUnits(8));

    QCOMPARE(OutcomeOf(answered), BisectionOutcome::StillSearching);
    QCOMPARE(answered.suspects.size(), std::size_t{8});
    QCOMPARE(TheRound(answered).number, std::size_t{1});
}

void BisectionRoundsTest::ARoundTurnsOnHalfTheSuspectsRoundedDown()
{
    QCOMPARE(TheRound(PastTheReferenceRound(LonelyUnits(8))).unitsOn.size(), std::size_t{4});
    QCOMPARE(TheRound(PastTheReferenceRound(LonelyUnits(7))).unitsOn.size(), std::size_t{3});
    QCOMPARE(TheRound(PastTheReferenceRound(LonelyUnits(3))).unitsOn.size(), std::size_t{1});
    QCOMPARE(TheRound(PastTheReferenceRound(LonelyUnits(2))).unitsOn.size(), std::size_t{1});
}

void BisectionRoundsTest::NoRoundEverTurnsOnEverySuspect()
{
    for (std::size_t howMany = 2; howMany <= 41; ++howMany)
    {
        BisectionRun run = PastTheReferenceRound(LonelyUnits(howMany));

        while (OutcomeOf(run) == BisectionOutcome::StillSearching)
        {
            const BisectionRound round = TheRound(run);

            QVERIFY2(round.unitsOn.size() < run.suspects.size(),
                     qPrintable(QString("a round turned on every one of the %1 suspects").arg(run.suspects.size())));
            QVERIFY(!round.unitsOn.empty());

            run = AfterAnswering(run, BisectionAnswer::ItCrashed);
        }
    }
}

void BisectionRoundsTest::AnswerCrashedKeepsTheHalfThatWasOn()
{
    const BisectionRun run = PastTheReferenceRound(LonelyUnits(8));
    const BisectionRound round = TheRound(run);
    const BisectionRun narrowed = AfterAnswering(run, BisectionAnswer::ItCrashed);

    QCOMPARE(narrowed.suspects, round.unitsOn);
    QCOMPARE(narrowed.cleared.size(), std::size_t{4});
    QCOMPARE(TheRound(narrowed).number, std::size_t{2});
}

void BisectionRoundsTest::AnswerRanFineKeepsTheHalfThatWasOff()
{
    const BisectionRun run = PastTheReferenceRound(LonelyUnits(8));
    const BisectionRound round = TheRound(run);
    const BisectionRun narrowed = AfterAnswering(run, BisectionAnswer::ItRanFine);

    QCOMPARE(narrowed.suspects.size(), std::size_t{4});
    QCOMPARE(narrowed.cleared, round.unitsOn);

    for (const std::size_t suspect : narrowed.suspects)
    {
        QVERIFY(std::ranges::find(round.unitsOn, suspect) == round.unitsOn.end());
    }
}

void BisectionRoundsTest::NoRoundOfTheFirstPassTurnsOnPartOfAGroup()
{
    std::vector<SearchUnit> units = LonelyUnits(6);
    units.push_back(AGroupOf(17));

    for (const BisectionAnswer answer : {BisectionAnswer::ItCrashed, BisectionAnswer::ItRanFine})
    {
        BisectionRun run = PastTheReferenceRound(units);

        while (OutcomeOf(run) == BisectionOutcome::StillSearching)
        {
            const BisectionRound round = TheRound(run);

            for (const std::size_t unit : round.unitsOn)
            {
                for (const std::filesystem::path& member : run.units[unit].addons)
                {
                    QVERIFY2(std::ranges::find(round.addonsOn, member) != round.addonsOn.end(),
                             qPrintable(QString("a round left out %1").arg(QString::fromStdString(AsUtf8(member)))));
                }
            }

            std::size_t membersOfTheUnitsOn = 0;

            for (const std::size_t unit : round.unitsOn)
            {
                membersOfTheUnitsOn += run.units[unit].addons.size();
            }

            QCOMPARE(round.addonsOn.size(), membersOfTheUnitsOn);

            run = AfterAnswering(run, answer);
        }
    }
}

void BisectionRoundsTest::TheSearchStopsInsteadOfRunningARoundOverOneUnit()
{
    BisectionRun run = PastTheReferenceRound(LonelyUnits(2));
    run = AfterAnswering(run, BisectionAnswer::ItCrashed);

    QCOMPARE(run.suspects.size(), std::size_t{1});
    QVERIFY(OutcomeOf(run) != BisectionOutcome::StillSearching);
}

void BisectionRoundsTest::AUnitOfOneAddonIsTheFirstOutcome()
{
    BisectionRun run = PastTheReferenceRound(LonelyUnits(2));
    run = AfterAnswering(run, BisectionAnswer::ItCrashed);

    QCOMPARE(OutcomeOf(run), BisectionOutcome::OneAddonLeft);
    QCOMPARE(WhatIsLeft(run), std::vector<std::filesystem::path>{AddonNumber(0)});
}

void BisectionRoundsTest::AUnitOfSeveralAddonsIsTheIrreducibleSet()
{
    const std::vector<SearchUnit> units = {AGroupOf(2), SearchUnit{.addons = {AddonNumber(9)}}};
    BisectionRun run = PastTheReferenceRound(units);
    run = AfterAnswering(run, BisectionAnswer::ItCrashed);

    QCOMPARE(OutcomeOf(run), BisectionOutcome::AnIrreducibleSet);
    QCOMPARE(WhatIsLeft(run).size(), std::size_t{3});
}

void BisectionRoundsTest::TheWorstCaseIsTheCeilingOfTheLogarithm()
{
    QCOMPARE(RoundsInTheWorstCase(41), std::size_t{6});
    QCOMPARE(RoundsInTheWorstCase(20), std::size_t{5});
    QCOMPARE(RoundsInTheWorstCase(17), std::size_t{5});
    QCOMPARE(RoundsInTheWorstCase(16), std::size_t{4});
    QCOMPARE(RoundsInTheWorstCase(2), std::size_t{1});
    QCOMPARE(RoundsInTheWorstCase(1), std::size_t{0});
    QCOMPARE(RoundsInTheWorstCase(0), std::size_t{0});
}

void BisectionRoundsTest::TheSecondPassSplitsTheSatellitesAndKeepsTheBaseOnInEveryRound()
{
    const std::vector<SearchUnit> units = {AGroupOf(4), SearchUnit{.addons = {AddonNumber(9)}}};
    BisectionRun run = AfterAnswering(PastTheReferenceRound(units), BisectionAnswer::ItCrashed);

    QCOMPARE(OutcomeOf(run), BisectionOutcome::AnIrreducibleSet);
    QVERIFY(ASecondPassIsPossible(run));

    BisectionRun second = IntoTheSecondPass(run);

    QCOMPARE(second.pass, BisectionPass::InsideTheGroup);
    QCOMPARE(second.units.size(), std::size_t{4});
    QCOMPARE(OutcomeOf(second), BisectionOutcome::StillSearching);

    const std::filesystem::path base = PathFromUtf8("D:/MSFS 2024/Aircrafts (2024)/base");

    while (OutcomeOf(second) == BisectionOutcome::StillSearching)
    {
        const BisectionRound round = TheRound(second);

        QVERIFY2(std::ranges::find(round.addonsOn, base) != round.addonsOn.end(),
                 "a round of the second pass ran without the base");

        second = AfterAnswering(second, BisectionAnswer::ItCrashed);
    }

    QCOMPARE(OutcomeOf(second), BisectionOutcome::OneAddonLeft);
    QCOMPARE(WhatIsLeft(second).size(), std::size_t{1});
}

void BisectionRoundsTest::TheSecondPassIsNotPossibleWithoutAnIdentifiableBase()
{
    SearchUnit shadowing;
    shadowing.addons = {AddonNumber(1), AddonNumber(2)};

    BisectionRun run = AfterAnswering(PastTheReferenceRound({shadowing, SearchUnit{.addons = {AddonNumber(9)}}}),
                                      BisectionAnswer::ItCrashed);

    QCOMPARE(OutcomeOf(run), BisectionOutcome::AnIrreducibleSet);
    QVERIFY(!ASecondPassIsPossible(run));
}

void BisectionRoundsTest::AFirstPassOverTwentyUnitsConvergesInFiveRounds()
{
    BisectionRun run = PastTheReferenceRound(LonelyUnits(20));
    std::size_t rounds = 0;

    while (OutcomeOf(run) == BisectionOutcome::StillSearching)
    {
        ++rounds;
        run = AfterAnswering(run, BisectionAnswer::ItRanFine);
    }

    QCOMPARE(rounds, RoundsInTheWorstCase(20));
    QCOMPARE(OutcomeOf(run), BisectionOutcome::OneAddonLeft);
}

QTEST_APPLESS_MAIN(BisectionRoundsTest)

#include "tst_bisection_rounds.moc"
