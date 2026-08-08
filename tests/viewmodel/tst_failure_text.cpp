#include <QtTest/QtTest>

#include "support/PathText.h"
#include "viewmodel/FailureText.h"

namespace
{
    class FailureTextTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryFileResultExceptSuccessCarriesAnExplanation();
        static void EveryLinkFailureExceptSuccessCarriesAnExplanation();
        static void EveryCategoryRuleThatMatchedSaysWhyItMatched();
        static void ARefusedImportNamesTheFolderAndWhyItWasRefused();
        static void AnIdentityAlreadyTakenSaysWhereTheOccupantIs();
        static void ARefusedQuarantineRestoreAlsoSaysWhereTheOccupantIs();
        static void EachDeletedAddonSaysWhichOfTheTwoRoutesTookIt();
        static void ADeletionStoppedByALinkSaysWhichLinksAlreadyWentAway();
    };
}

void FailureTextTest::EveryFileResultExceptSuccessCarriesAnExplanation()
{
    for (const FileResult result : kAllFileResults)
    {
        if (result == FileResult::Completed)
        {
            QVERIFY(Explain(result).isEmpty());
            continue;
        }

        QVERIFY2(!Explain(result).isEmpty(), QByteArray::number(static_cast<int>(result)).constData());
    }
}

void FailureTextTest::EveryLinkFailureExceptSuccessCarriesAnExplanation()
{
    for (const LinkFailure failure : kAllLinkFailures)
    {
        if (failure == LinkFailure::None)
        {
            QVERIFY(Explain(failure).isEmpty());
            continue;
        }

        QVERIFY2(!Explain(failure).isEmpty(), QByteArray::number(static_cast<int>(failure)).constData());
    }
}

void FailureTextTest::EveryCategoryRuleThatMatchedSaysWhyItMatched()
{
    for (const CategoryRule rule : kAllCategoryRules)
    {
        if (rule == CategoryRule::None)
        {
            QVERIFY(Explain(rule).isEmpty());
            continue;
        }

        QVERIFY2(!Explain(rule).isEmpty(), QByteArray::number(static_cast<int>(rule)).constData());
    }
}

void FailureTextTest::ARefusedImportNamesTheFolderAndWhyItWasRefused()
{
    const ImportOperationResult result{
        .request = ImportRequest{.source = "E:/Sim/Community/simbridge", .category = "D:/Library/Sceneries"},
        .result = FileResult::NotEnoughFreeSpace};

    const QString line = Describe(result);

    QVERIFY(line.contains(QStringLiteral("simbridge")));
    QVERIFY(line.contains(Explain(FileResult::NotEnoughFreeSpace)));
}

void FailureTextTest::AnIdentityAlreadyTakenSaysWhereTheOccupantIs()
{
    const ImportOperationResult result{
        .request = ImportRequest{.source = "E:/Sim/Community/simbridge", .category = "D:/Library/Sceneries"},
        .result = FileResult::TheIdentityIsTaken,
        .occupant = "D:/Library/Utils/simbridge"};

    const QString line = Describe(result);

    QVERIFY(line.contains(Explain(FileResult::TheIdentityIsTaken)));
    QVERIFY(line.contains(AsText("D:/Library/Utils/simbridge")));
}

void FailureTextTest::ARefusedQuarantineRestoreAlsoSaysWhereTheOccupantIs()
{
    const FileOperationResult result{.path = "D:/Library/_fsorganizer-quarantine/simbridge",
                                     .result = FileResult::TheIdentityIsTaken,
                                     .occupant = "D:/Library/Sceneries/simbridge"};

    const QString line = Describe(result);

    QVERIFY(line.contains(QStringLiteral("simbridge")));
    QVERIFY(line.contains(AsText("D:/Library/Sceneries/simbridge")));
}

void FailureTextTest::EachDeletedAddonSaysWhichOfTheTwoRoutesTookIt()
{
    const DeletionResult gone{.folder = "D:/Library/Aircrafts/aerosoft-crj"};

    const QString recycled = Describe(gone, DeletionRoute::RecycleBin);
    const QString erased = Describe(gone, DeletionRoute::Permanently);

    QVERIFY(recycled.contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(erased.contains(QStringLiteral("aerosoft-crj")));
    QVERIFY(recycled != erased);
}

void FailureTextTest::ADeletionStoppedByALinkSaysWhichLinksAlreadyWentAway()
{
    const DeletionResult stopped{.folder = "D:/Library/Aircrafts/aerosoft-crj",
                                 .result = FileResult::CouldNotRemoveTheLink,
                                 .linksRemoved = {"E:/Sim/Community/aerosoft-crj"}};

    const QString line = Describe(stopped, DeletionRoute::Permanently);

    QVERIFY(line.contains(Explain(FileResult::CouldNotRemoveTheLink)));
    QVERIFY(line.contains(AsText("E:/Sim/Community/aerosoft-crj")));
}

QTEST_APPLESS_MAIN(FailureTextTest)

#include "tst_failure_text.moc"
