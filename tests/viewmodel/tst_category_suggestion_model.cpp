#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QtTest>

#include "tests/support/PathPrinting.h"
#include "viewmodel/CategorySuggestionModel.h"

namespace
{
    class CategorySuggestionModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void TheRulesThatAreTrustedOnTheirOwnStartCheckedAndTheLiveryRuleDoesNot();
        static void OnlyTheSuggestionsThatWouldActuallyMoveTheAddonAreListed();
        static void TheChosenMovesAreTheRowsLeftCheckedAndNothingElse();
        static void TheOverallStateTellsAllFromNoneFromAMixture();
        static void ChoosingAllReachesEveryRowAndAnnouncesTheWholeColumn();
        static void ChoosingAllOnAnEmptyListSaysNothingInsteadOfAnnouncingARangeThatDoesNotExist();
    };
}

namespace
{
    const std::filesystem::path kAircrafts = "D:/MSFS 2024/Aircrafts";
    const std::filesystem::path kSceneries = "D:/MSFS 2024/Sceneries";
    const std::filesystem::path kLiveries = "D:/MSFS 2024/Liveries";

    CategorySuggestion Suggestion(const std::string& folder,
                                  const std::filesystem::path& current,
                                  const std::filesystem::path& suggested,
                                  const CategoryRule rule)
    {
        return CategorySuggestion{
            .addonFolder = current / folder, .currentCategory = current, .suggestedCategory = suggested, .rule = rule};
    }

    std::vector<CategorySuggestion> BothRules()
    {
        return {Suggestion("orbx-ksea", kAircrafts, kSceneries, CategoryRule::TheContentTypeIsScenery),
                Suggestion("fenix-a320-lufthansa", kAircrafts, kLiveries, CategoryRule::TheContentTypeIsLivery)};
    }
}

void CategorySuggestionModelTest::TheRulesThatAreTrustedOnTheirOwnStartCheckedAndTheLiveryRuleDoesNot()
{
    CategorySuggestionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.Show(BothRules());

    QCOMPARE(model.rowCount({}), 2);
    QCOMPARE(model.data(model.index(0, 0, {}), Qt::CheckStateRole).value<Qt::CheckState>(), Qt::Checked);
    QCOMPARE(model.data(model.index(1, 0, {}), Qt::CheckStateRole).value<Qt::CheckState>(), Qt::Unchecked);
}

void CategorySuggestionModelTest::OnlyTheSuggestionsThatWouldActuallyMoveTheAddonAreListed()
{
    CategorySuggestionModel model;

    model.Show({Suggestion("orbx-ksea", kAircrafts, kSceneries, CategoryRule::TheContentTypeIsScenery),
                Suggestion("already-there", kSceneries, kSceneries, CategoryRule::TheContentTypeIsScenery),
                Suggestion("unclassified", kAircrafts, {}, CategoryRule::None)});

    QCOMPARE(model.rowCount({}), 1);
    QCOMPARE(model.data(model.index(0, 0, {}), Qt::DisplayRole).toString(), QStringLiteral("orbx-ksea"));
}

void CategorySuggestionModelTest::TheChosenMovesAreTheRowsLeftCheckedAndNothingElse()
{
    CategorySuggestionModel model;
    model.Show(BothRules());

    QCOMPARE(model.Chosen().size(), std::size_t{1});
    QCOMPARE(model.Chosen().front().addonFolder, kAircrafts / "orbx-ksea");

    QVERIFY(model.setData(model.index(1, 0, {}), Qt::Checked, Qt::CheckStateRole));
    QVERIFY(model.setData(model.index(0, 0, {}), Qt::Unchecked, Qt::CheckStateRole));

    QCOMPARE(model.Chosen().size(), std::size_t{1});
    QCOMPARE(model.Chosen().front().addonFolder, kAircrafts / "fenix-a320-lufthansa");
}

void CategorySuggestionModelTest::TheOverallStateTellsAllFromNoneFromAMixture()
{
    CategorySuggestionModel model;
    model.Show(BothRules());

    QCOMPARE(model.ChosenState(), Qt::PartiallyChecked);

    model.ChooseAll(true);
    QCOMPARE(model.ChosenState(), Qt::Checked);

    model.ChooseAll(false);
    QCOMPARE(model.ChosenState(), Qt::Unchecked);
}

void CategorySuggestionModelTest::ChoosingAllReachesEveryRowAndAnnouncesTheWholeColumn()
{
    CategorySuggestionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);
    model.Show(BothRules());

    const QSignalSpy announced(&model, &QAbstractItemModel::dataChanged);

    model.ChooseAll(true);

    QCOMPARE(model.Chosen().size(), std::size_t{2});
    QCOMPARE(announced.size(), 1);
    QCOMPARE(announced.front().at(0).toModelIndex().row(), 0);
    QCOMPARE(announced.front().at(1).toModelIndex().row(), 1);

    model.ChooseAll(false);

    QVERIFY(model.Chosen().empty());
}

void CategorySuggestionModelTest::ChoosingAllOnAnEmptyListSaysNothingInsteadOfAnnouncingARangeThatDoesNotExist()
{
    CategorySuggestionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    const QSignalSpy announced(&model, &QAbstractItemModel::dataChanged);

    model.ChooseAll(true);

    QCOMPARE(announced.size(), 0);
    QCOMPARE(model.ChosenState(), Qt::Unchecked);
}

QTEST_MAIN(CategorySuggestionModelTest)

#include "tst_category_suggestion_model.moc"
