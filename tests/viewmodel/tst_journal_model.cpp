#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QtTest>

#include "support/PathText.h"
#include "viewmodel/JournalModel.h"

namespace
{
    class JournalModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AnImportIsOneRowWithItsStepsUnderneath();
        static void TheLibraryAppearsByItsLabelAndNeverAsAUuid();
        static void TheNewestOperationComesFirst();
        static void FilteringKeepsOnlyWhatFailed();
        static void SearchingReachesTheStepsOfAnImport();
        static void ASwapIsOneRowNamingBothAddons();
    };
}

namespace
{
    const std::filesystem::path kSource = "E:/Sim/Community/simbridge";
    const std::filesystem::path kTarget = "D:/Library/Utils/simbridge";

    std::chrono::system_clock::time_point Moment(const int seconds)
    {
        return std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000 + seconds}};
    }

    OperationRecord Step(const OperationKind kind, const int seconds, const FileResult result = FileResult::Completed)
    {
        return OperationRecord::OfImport(
            Moment(seconds), kind, AddonId{.libraryId = "lib-1", .folderName = "simbridge"}, kSource, kTarget, result);
    }

    OperationRecord Link(const OperationKind kind, const int seconds, const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(
            Moment(seconds), kind, AddonId{.libraryId = "lib-1", .folderName = "pmdg-aircraft-77w"},
            "D:/Library/Aircrafts/pmdg-aircraft-77w", "E:/Sim/Community/pmdg-aircraft-77w", failure);
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.libraries = {Library{.id = "lib-1", .path = "D:/Library", .label = "Biblioteca do Bruno"}};

        return profile;
    }

    std::vector<OperationRecord> AnImportAndALink()
    {
        return {
            Step(OperationKind::ImportCopyToStaging, 0),
            Step(OperationKind::ImportVerifyStaging, 1),
            Step(OperationKind::ImportMoveIntoPlace, 2),
            Step(OperationKind::ImportRemoveSource, 3),
            OperationRecord::OfLink(Moment(4), OperationKind::EnableAddon,
                                    AddonId{.libraryId = "lib-1", .folderName = "simbridge"}, kTarget, kSource,
                                    LinkFailure::None),
            Link(OperationKind::DisableAddon, 5),
        };
    }
}

void JournalModelTest::AnImportIsOneRowWithItsStepsUnderneath()
{
    JournalModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.ShowRecords(AnImportAndALink(), Profile());

    QCOMPARE(model.rowCount({}), 2);

    const QModelIndex run = model.index(1, JournalModel::OperationColumn, {});
    QCOMPARE(model.rowCount(model.index(1, 0, {})), 5);
    QCOMPARE(run.data(Qt::DisplayRole).toString(), QStringLiteral("Import (5 step)"));
    QVERIFY(run.data(JournalModel::SucceededRole).toBool());

    const QModelIndex firstStep = model.index(0, JournalModel::OperationColumn, model.index(1, 0, {}));
    QCOMPARE(firstStep.data(Qt::DisplayRole).toString(), QStringLiteral("Copy to the staging area"));
    QCOMPARE(model.rowCount(firstStep), 0);
    QCOMPARE(model.parent(firstStep), model.index(1, 0, {}));
}

void JournalModelTest::TheLibraryAppearsByItsLabelAndNeverAsAUuid()
{
    JournalModel model;
    model.ShowRecords({Link(OperationKind::EnableAddon, 0)}, Profile());

    QCOMPARE(model.index(0, JournalModel::LibraryColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Biblioteca do Bruno"));

    JournalModel orphan;
    orphan.ShowRecords({Link(OperationKind::EnableAddon, 0)}, SimulatorProfile{});

    QCOMPARE(orphan.index(0, JournalModel::LibraryColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("(library removed)"));
}

void JournalModelTest::TheNewestOperationComesFirst()
{
    JournalModel model;
    model.ShowRecords(AnImportAndALink(), Profile());

    QCOMPARE(model.index(0, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Disable addon"));
    QCOMPARE(model.index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("pmdg-aircraft-77w"));
}

void JournalModelTest::FilteringKeepsOnlyWhatFailed()
{
    std::vector<OperationRecord> records = AnImportAndALink();
    records.back() = Link(OperationKind::DisableAddon, 5, LinkFailure::CouldNotRemoveLink);

    JournalModel model;
    model.ShowRecords(records, Profile());

    JournalFilterModel filter;
    filter.setSourceModel(&model);
    filter.ShowOnlyWhatFailed(true);

    QCOMPARE(filter.rowCount({}), 1);
    QCOMPARE(filter.index(0, JournalModel::OutcomeColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("the link could not be removed"));
}

void JournalModelTest::SearchingReachesTheStepsOfAnImport()
{
    JournalModel model;
    model.ShowRecords(AnImportAndALink(), Profile());

    JournalFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search(QStringLiteral("simbridge"));
    QCOMPARE(filter.rowCount({}), 1);

    filter.Search(QStringLiteral("check the copy"));
    QCOMPARE(filter.rowCount({}), 1);
    QCOMPARE(filter.rowCount(filter.index(0, 0, {})), 1);

    filter.Search(QStringLiteral("none of that exists"));
    QCOMPARE(filter.rowCount({}), 0);
}

void JournalModelTest::ASwapIsOneRowNamingBothAddons()
{
    const std::filesystem::path place = "E:/Sim/Community/pmdg-aircraft-77w";

    JournalModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.ShowRecords({Link(OperationKind::DisableAddon, 0),
                       OperationRecord::OfLink(Moment(1), OperationKind::EnableAddon,
                                               AddonId{.libraryId = "lib-1", .folderName = "fenix-a320"},
                                               "D:/Library/Aircrafts/fenix-a320", place, LinkFailure::None)},
                      Profile());

    QCOMPARE(model.rowCount({}), 1);
    QCOMPARE(model.rowCount(model.index(0, 0, {})), 2);
    QCOMPARE(model.index(0, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Swap addons"));
    QCOMPARE(model.index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("pmdg-aircraft-77w out, fenix-a320 in"));
    QCOMPARE(model.index(0, JournalModel::TargetColumn, {}).data(Qt::DisplayRole).toString(), AsText(place));
    QVERIFY(model.index(0, 0, {}).data(JournalModel::SucceededRole).toBool());
}

QTEST_MAIN(JournalModelTest)

#include "tst_journal_model.moc"
