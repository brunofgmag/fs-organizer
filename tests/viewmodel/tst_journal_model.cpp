#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QtTest>

#include "viewmodel/JournalModel.h"

class JournalModelTest : public QObject
{
    Q_OBJECT

private slots:
    static void AnImportIsOneRowWithItsStepsUnderneath();
    static void TheLibraryAppearsByItsLabelAndNeverAsAUuid();
    static void TheNewestOperationComesFirst();
    static void FilteringKeepsOnlyWhatFailed();
    static void SearchingReachesTheStepsOfAnImport();
};

namespace
{
    const std::filesystem::path kSource = "E:/Sim/Community/simbridge";
    const std::filesystem::path kTarget = "D:/Library/Utils/simbridge";

    std::chrono::system_clock::time_point Moment(const int seconds)
    {
        return std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000 + seconds}};
    }

    OperationRecord
    Step(const OperationKind kind, const int seconds, const ImportResult result = ImportResult::Completed)
    {
        return OperationRecord::OfImport(Moment(seconds), kind, AddonId{"lib-1", "simbridge"}, kSource, kTarget,
                                         result);
    }

    OperationRecord Link(const OperationKind kind, const int seconds, const LinkFailure failure = LinkFailure::None)
    {
        return OperationRecord::OfLink(Moment(seconds), kind, AddonId{"lib-1", "pmdg-aircraft-77w"},
                                       "D:/Library/Aircrafts/pmdg-aircraft-77w", "E:/Sim/Community/pmdg-aircraft-77w",
                                       failure);
    }

    SimulatorProfile Profile()
    {
        SimulatorProfile profile;
        profile.libraries = {Library{"lib-1", "D:/Library", "Biblioteca do Bruno"}};

        return profile;
    }

    std::vector<OperationRecord> AnImportAndALink()
    {
        return {
            Step(OperationKind::ImportCopyToStaging, 0),
            Step(OperationKind::ImportVerifyStaging, 1),
            Step(OperationKind::ImportMoveIntoPlace, 2),
            Step(OperationKind::ImportRemoveSource, 3),
            OperationRecord::OfLink(Moment(4), OperationKind::EnableAddon, AddonId{"lib-1", "simbridge"}, kTarget,
                                    kSource, LinkFailure::None),
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
    QCOMPARE(run.data(Qt::DisplayRole).toString(), QStringLiteral("Importação (5 passo(s))"));
    QVERIFY(run.data(JournalModel::SucceededRole).toBool());

    const QModelIndex firstStep = model.index(0, JournalModel::OperationColumn, model.index(1, 0, {}));
    QCOMPARE(firstStep.data(Qt::DisplayRole).toString(), QStringLiteral("Copiar para a área de staging"));
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
             QStringLiteral("(biblioteca removida)"));
}

void JournalModelTest::TheNewestOperationComesFirst()
{
    JournalModel model;
    model.ShowRecords(AnImportAndALink(), Profile());

    QCOMPARE(model.index(0, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Desabilitar addon"));
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
             QStringLiteral("não foi possível remover o link"));
}

void JournalModelTest::SearchingReachesTheStepsOfAnImport()
{
    JournalModel model;
    model.ShowRecords(AnImportAndALink(), Profile());

    JournalFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search(QStringLiteral("simbridge"));
    QCOMPARE(filter.rowCount({}), 1);

    filter.Search(QStringLiteral("verificar"));
    QCOMPARE(filter.rowCount({}), 1);
    QCOMPARE(filter.rowCount(filter.index(0, 0, {})), 1);

    filter.Search(QStringLiteral("nada disso existe"));
    QCOMPARE(filter.rowCount({}), 0);
}

QTEST_MAIN(JournalModelTest)

#include "tst_journal_model.moc"
