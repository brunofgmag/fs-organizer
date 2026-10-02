#include <QtTest/QAbstractItemModelTester>
#include <QtCore/QTranslator>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>
#include <vector>

#include "support/PathText.h"
#include "viewmodel/JournalModel.h"
#include "viewmodel/RowTagRoles.h"

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
        static void ASecondSearchOverTheSameRecordsBuildsNoSearchText();
        static void SearchingAfterNewRecordsFindsOnlyWhatTheyHold();
        static void SearchingAfterALanguageChangeFindsTheNewLanguageOnly();
        static void ATermSpanningTwoColumnsMatchesNothing();
        static void ASwapIsOneRowNamingBothAddons();
        static void WhatSupportsTheOperationIsQuietAndAFailedResultKeepsTheNameInk();
        static void ADisableWithItsStartupEntryIsOneRowNamedAfterBoth();
        static void AStartupEntryOutsideYourAddonsIsNamedByItsLabel();
        static void ASwitchWithNoAddonSaysItsOwnWordsAndTheOneInsideAGroupKeepsTheirs();
        static void TheStartupAndPackageOperationsAreRowsNamedAfterTheEntryOrThePackage();
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

    int RowsUnder(const QAbstractItemModel& model, const QModelIndex& parent = {})
    {
        int rows = model.rowCount(parent);

        for (int row = 0; row < model.rowCount(parent); ++row)
        {
            rows += RowsUnder(model, model.index(row, 0, parent));
        }

        return rows;
    }

    class CopyInPortuguese final : public QTranslator
    {
    public:
        [[nodiscard]] bool isEmpty() const override
        {
            return false;
        }

        [[nodiscard]] QString translate(const char* context, const char* source, const char*, int) const override
        {
            if (QLatin1String(context) == QLatin1String("JournalModel")
                && QLatin1String(source) == QLatin1String("Copy to a temporary folder"))
            {
                return QStringLiteral("Copiar para uma pasta temporaria");
            }

            return {};
        }
    };

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
    QCOMPARE(firstStep.data(Qt::DisplayRole).toString(), QStringLiteral("Copy to a temporary folder"));
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

void JournalModelTest::ASecondSearchOverTheSameRecordsBuildsNoSearchText()
{
    JournalModel model;
    model.ShowRecords(AnImportAndALink(), Profile());

    JournalFilterModel filter;
    filter.setSourceModel(&model);

    QCOMPARE(model.TimesItBuiltASearchText(), 0);

    filter.Search(QStringLiteral("none of that exists"));
    QCOMPARE(filter.rowCount({}), 0);

    const int firstSearch = model.TimesItBuiltASearchText();
    QVERIFY(firstSearch > 0);
    QVERIFY(firstSearch <= RowsUnder(model));

    filter.Search(QStringLiteral("check the copy"));
    QCOMPARE(filter.rowCount({}), 1);
    filter.Search(QStringLiteral("simbridge"));
    QCOMPARE(filter.rowCount({}), 1);

    QCOMPARE(model.TimesItBuiltASearchText(), firstSearch);
}

void JournalModelTest::SearchingAfterNewRecordsFindsOnlyWhatTheyHold()
{
    const auto link = [](const char* folder)
    {
        return OperationRecord::OfLink(Moment(0), OperationKind::EnableAddon,
                                       AddonId{.libraryId = "lib-1", .folderName = folder}, kTarget, kSource,
                                       LinkFailure::None);
    };

    JournalModel model;
    model.ShowRecords({link("old-addon")}, Profile());

    JournalFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search(QStringLiteral("old-addon"));
    QCOMPARE(filter.rowCount({}), 1);

    model.ShowRecords({link("new-addon")}, Profile());

    filter.Search(QStringLiteral("old-addon"));
    QCOMPARE(filter.rowCount({}), 0);

    filter.Search(QStringLiteral("new-addon"));
    QCOMPARE(filter.rowCount({}), 1);
}

void JournalModelTest::SearchingAfterALanguageChangeFindsTheNewLanguageOnly()
{
    JournalModel model;
    model.ShowRecords(AnImportAndALink(), Profile());

    JournalFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search(QStringLiteral("copy to a temporary"));
    QCOMPARE(filter.rowCount({}), 1);

    CopyInPortuguese portuguese;
    QCoreApplication::installTranslator(&portuguese);
    model.Retranslate();

    filter.Search(QStringLiteral("COPIAR PARA UMA PASTA"));
    const int theNewLanguage = filter.rowCount({});

    filter.Search(QStringLiteral("copy to a temporary"));
    const int theOldLanguage = filter.rowCount({});

    QCoreApplication::removeTranslator(&portuguese);

    QCOMPARE(theNewLanguage, 1);
    QCOMPARE(theOldLanguage, 0);
}

void JournalModelTest::ATermSpanningTwoColumnsMatchesNothing()
{
    JournalModel model;
    model.ShowRecords({Link(OperationKind::EnableAddon, 0)}, Profile());

    const QModelIndex row = model.index(0, 0, {});
    const QString operation = row.siblingAtColumn(JournalModel::OperationColumn).data().toString();
    const QString addon = row.siblingAtColumn(JournalModel::AddonColumn).data().toString();

    JournalFilterModel filter;
    filter.setSourceModel(&model);

    filter.Search(operation.right(3) + addon.left(3));
    QCOMPARE(filter.rowCount({}), 0);

    filter.Search(operation.right(3));
    QCOMPARE(filter.rowCount({}), 1);

    filter.Search(addon.left(3));
    QCOMPARE(filter.rowCount({}), 1);
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

void JournalModelTest::WhatSupportsTheOperationIsQuietAndAFailedResultKeepsTheNameInk()
{
    std::vector<OperationRecord> records = AnImportAndALink();
    records.back() = Link(OperationKind::DisableAddon, 5, LinkFailure::CouldNotRemoveLink);

    JournalModel model;
    model.ShowRecords(records, Profile());

    const QModelIndex failed = model.index(0, 0, {});
    const QModelIndex worked = model.index(1, 0, {});

    QVERIFY(!failed.data(JournalModel::SucceededRole).toBool());
    QVERIFY(worked.data(JournalModel::SucceededRole).toBool());

    for (const int column : {JournalModel::WhenColumn, JournalModel::LibraryColumn, JournalModel::SourceColumn,
                             JournalModel::TargetColumn})
    {
        QVERIFY(worked.siblingAtColumn(column).data(QuietRole).toBool());
    }

    QVERIFY(!worked.siblingAtColumn(JournalModel::OperationColumn).data(QuietRole).toBool());
    QVERIFY(!worked.siblingAtColumn(JournalModel::AddonColumn).data(QuietRole).toBool());

    QVERIFY(worked.siblingAtColumn(JournalModel::OutcomeColumn).data(QuietRole).toBool());
    QVERIFY(!failed.siblingAtColumn(JournalModel::OutcomeColumn).data(QuietRole).toBool());

    const QModelIndex step = model.index(0, JournalModel::SourceColumn, model.index(1, 0, {}));
    QVERIFY(step.data(QuietRole).toBool());
}

void JournalModelTest::ADisableWithItsStartupEntryIsOneRowNamedAfterBoth()
{
    const std::filesystem::path executable = "E:/Sim/Community/pmdg-aircraft-77w/bin/loader.exe";

    JournalModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.ShowRecords(
        {Link(OperationKind::DisableAddon, 0),
         OperationRecord::OfImport(Moment(1), OperationKind::TurnOffTheStartupEntry,
                                   AddonId{.libraryId = "lib-1", .folderName = "pmdg-aircraft-77w"},
                                   "D:/Library/Aircrafts/pmdg-aircraft-77w", executable, FileResult::Completed)},
        Profile());

    QCOMPARE(model.rowCount({}), 1);
    QCOMPARE(model.rowCount(model.index(0, 0, {})), 2);
    QCOMPARE(model.index(0, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Disable addon and its startup entry"));
    QCOMPARE(model.index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("pmdg-aircraft-77w"));
    QCOMPARE(model.index(0, JournalModel::TargetColumn, {}).data(Qt::DisplayRole).toString(), AsText(executable));
    QVERIFY(model.index(0, 0, {}).data(JournalModel::SucceededRole).toBool());
}

void JournalModelTest::AStartupEntryOutsideYourAddonsIsNamedByItsLabel()
{
    const std::filesystem::path executable = "C:/Program Files/Other/agent.exe";

    JournalModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.ShowRecords(
        {OperationRecord::OfImport(Moment(0), OperationKind::TurnOffTheStartupEntry, AddonId{}, std::filesystem::path{},
                                   executable, FileResult::Completed, OriginSource::Unknown, "Fenix")},
        Profile());

    QCOMPARE(model.rowCount({}), 1);
    QCOMPARE(model.index(0, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(), QStringLiteral("Fenix"));
}

void JournalModelTest::ASwitchWithNoAddonSaysItsOwnWordsAndTheOneInsideAGroupKeepsTheirs()
{
    const std::filesystem::path executable = "C:/Program Files/Other/agent.exe";

    JournalModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.ShowRecords(
        {OperationRecord::OfImport(Moment(0), OperationKind::TurnOffTheStartupEntry, AddonId{}, std::filesystem::path{},
                                   executable, FileResult::Completed, OriginSource::Unknown, "Fenix"),
         OperationRecord::OfImport(Moment(1), OperationKind::TurnOnTheStartupEntry, AddonId{}, std::filesystem::path{},
                                   executable, FileResult::Completed, OriginSource::Unknown, "Fenix"),
         Link(OperationKind::DisableAddon, 2),
         OperationRecord::OfImport(Moment(3), OperationKind::TurnOffTheStartupEntry,
                                   AddonId{.libraryId = "lib-1", .folderName = "pmdg-aircraft-77w"},
                                   "D:/Library/Aircrafts/pmdg-aircraft-77w", executable, FileResult::Completed)},
        Profile());

    QCOMPARE(model.rowCount({}), 3);

    QCOMPARE(model.index(0, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Disable addon and its startup entry"));
    QCOMPARE(model.index(1, JournalModel::OperationColumn, model.index(0, 0, {})).data(Qt::DisplayRole).toString(),
             QStringLiteral("Disable its startup entry"));

    QCOMPARE(model.index(1, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Enable a startup entry"));
    QCOMPARE(model.index(1, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(), QStringLiteral("Fenix"));
    QCOMPARE(model.index(1, JournalModel::TargetColumn, {}).data(Qt::DisplayRole).toString(), AsText(executable));
    QCOMPARE(model.rowCount(model.index(1, 0, {})), 0);

    QCOMPARE(model.index(2, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
             QStringLiteral("Disable a startup entry"));
}

void JournalModelTest::TheStartupAndPackageOperationsAreRowsNamedAfterTheEntryOrThePackage()
{
    struct Expected
    {
        OperationKind kind;
        const char* operation;
    };

    const std::vector<Expected> expected{
        {OperationKind::AddTheStartupEntry, "Add a startup entry"},
        {OperationKind::RemoveTheStartupEntry, "Remove a startup entry"},
        {OperationKind::EditTheStartupEntry, "Edit a startup entry"},
        {OperationKind::RestoreTheStartupEntry, "Restore a removed startup entry"},
        {OperationKind::ForgetTheStartupEntry, "Discard a removed startup entry"},
        {OperationKind::TurnOffTheSimulatorPackage, "Disable a simulator package"},
        {OperationKind::TurnOnTheSimulatorPackage, "Enable a simulator package"},
    };

    std::vector<OperationRecord> records;

    for (std::size_t at = 0; at < expected.size(); ++at)
    {
        records.push_back(OperationRecord::OfImport(Moment(static_cast<int>(at)), expected[at].kind, AddonId{}, {},
                                                    "C:/Tools/tool.exe", FileResult::TheProgramDoesNotExist,
                                                    OriginSource::Unknown, "The label " + std::to_string(at)));
    }

    JournalModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);

    model.ShowRecords(records, Profile());

    QCOMPARE(model.rowCount({}), static_cast<int>(expected.size()));

    for (std::size_t at = 0; at < expected.size(); ++at)
    {
        const int row = static_cast<int>(expected.size() - 1 - at);

        QCOMPARE(model.index(row, JournalModel::OperationColumn, {}).data(Qt::DisplayRole).toString(),
                 QString::fromLatin1(expected[at].operation));
        QCOMPARE(model.index(row, JournalModel::AddonColumn, {}).data(Qt::DisplayRole).toString(),
                 QStringLiteral("The label %1").arg(at));
        QVERIFY(!model.index(row, 0, {}).data(JournalModel::SucceededRole).toBool());
        QCOMPARE(model.index(row, JournalModel::OutcomeColumn, {}).data(Qt::DisplayRole).toString(),
                 QStringLiteral("that program file does not exist, so nothing changed"));
    }
}

QTEST_MAIN(JournalModelTest)

#include "tst_journal_model.moc"
