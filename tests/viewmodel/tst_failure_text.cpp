#include <QtTest/QtTest>

#include "support/PathText.h"
#include "viewmodel/FailureText.h"

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
};

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
    const ImportOperationResult result{ImportRequest{"E:/Sim/Community/simbridge", "D:/Library/Sceneries"},
                                       FileResult::NotEnoughFreeSpace};

    const QString line = Describe(result);

    QVERIFY(line.contains(QStringLiteral("simbridge")));
    QVERIFY(line.contains(Explain(FileResult::NotEnoughFreeSpace)));
}

void FailureTextTest::AnIdentityAlreadyTakenSaysWhereTheOccupantIs()
{
    const ImportOperationResult result{ImportRequest{"E:/Sim/Community/simbridge", "D:/Library/Sceneries"},
                                       FileResult::TheIdentityIsTaken, "D:/Library/Utils/simbridge"};

    const QString line = Describe(result);

    QVERIFY(line.contains(Explain(FileResult::TheIdentityIsTaken)));
    QVERIFY(line.contains(AsText("D:/Library/Utils/simbridge")));
}

void FailureTextTest::ARefusedQuarantineRestoreAlsoSaysWhereTheOccupantIs()
{
    const FileOperationResult result{"D:/Library/_fsorganizer-quarantine/simbridge", FileResult::TheIdentityIsTaken,
                                     "D:/Library/Sceneries/simbridge"};

    const QString line = Describe(result);

    QVERIFY(line.contains(QStringLiteral("simbridge")));
    QVERIFY(line.contains(AsText("D:/Library/Sceneries/simbridge")));
}

QTEST_APPLESS_MAIN(FailureTextTest)

#include "tst_failure_text.moc"
