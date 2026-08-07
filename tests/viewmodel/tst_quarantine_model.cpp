#include <QtTest/QtTest>

#include <QtCore/QDir>

#include "viewmodel/QuarantineModel.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class QuarantineModelTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EachQuarantinedFolderIsARowThatSaysWhereItWouldGoBackTo();
        static void AnItemNeitherSourceKnowsSaysSoInsteadOfShowingAnEmptyCell();
        static void NoCellRepeatsItsOwnTextAsATooltip();
        static void TheVersionIsEmptyUntilItLandsAndTheRowIsListedAnyway();
        static void ADetailThatLandsFillsTheVersionAndMarksWhatWasAlreadyReplaced();
        static void ASizeThatLandsReachesOnlyTheRowItBelongsTo();
        static void ListingAgainForgetsTheVersionAndSizeOfTheOldRows();
        static void TheWhenAndTheSizeShareTheSecondLineOfTheNameCell();
        static void TheSourceColumnTellsTheFourWaysAnOriginCanBeAnswered();
        static void OnlyTheDisagreementIsLoudEnoughToWearATag();
    };
}

namespace
{
    constexpr auto kMoment = std::chrono::system_clock::time_point{std::chrono::seconds{1'769'000'000}};

    std::vector<QuarantinedItem> TwoItems()
    {
        return {
            QuarantinedItem{.path = "E:/Sim/_fsorganizer-quarantine/simbridge",
                            .origin = "E:/Sim/Community/simbridge",
                            .quarantinedAt = kMoment,
                            .source = OriginSource::Sidecar},
            QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/orphan",
                            .origin = {},
                            .quarantinedAt = std::nullopt,
                            .source = OriginSource::Unknown},
        };
    }

    QString CellOf(const QuarantineModel& model, const int row, const int column)
    {
        return model.data(model.index(row, column), Qt::DisplayRole).toString();
    }
}

void QuarantineModelTest::EachQuarantinedFolderIsARowThatSaysWhereItWouldGoBackTo()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    QCOMPARE(model.rowCount({}), 2);
    QCOMPARE(model.columnCount({}), QuarantineModel::SourceColumn + 1);
    QCOMPARE(CellOf(model, 0, QuarantineModel::NameColumn), QStringLiteral("simbridge"));
    QCOMPARE(CellOf(model, 0, QuarantineModel::OriginColumn),
             QDir::toNativeSeparators(QStringLiteral("E:/Sim/Community/simbridge")));

    QVERIFY(model.ItemAt(model.index(1, 0)) != nullptr);
    QCOMPARE(model.Items().size(), std::size_t{2});
}

void QuarantineModelTest::AnItemNeitherSourceKnowsSaysSoInsteadOfShowingAnEmptyCell()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    QCOMPARE(CellOf(model, 1, QuarantineModel::OriginColumn), QStringLiteral("not recorded"));
    QCOMPARE(CellOf(model, 1, QuarantineModel::SourceColumn), QStringLiteral("neither has it"));
}

void QuarantineModelTest::NoCellRepeatsItsOwnTextAsATooltip()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    for (int column = 0; column <= QuarantineModel::SourceColumn; ++column)
    {
        QVERIFY(model.data(model.index(0, column), Qt::ToolTipRole).toString().isEmpty());
    }
}

void QuarantineModelTest::TheVersionIsEmptyUntilItLandsAndTheRowIsListedAnyway()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    QCOMPARE(model.rowCount({}), 2);
    QVERIFY(CellOf(model, 0, QuarantineModel::VersionColumn).isEmpty());
    QVERIFY(!model.data(model.index(0, QuarantineModel::NameColumn), SecondLineRole)
                 .toString()
                 .contains(QStringLiteral("GiB")));
}

void QuarantineModelTest::ADetailThatLandsFillsTheVersionAndMarksWhatWasAlreadyReplaced()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    model.ShowDetails({QuarantineDetail{.path = "E:/Sim/_fsorganizer-quarantine/simbridge",
                                        .version = "2.4.1",
                                        .replacedBy = "E:/Sim/Community/simbridge",
                                        .replacementVersion = "2.5.0"},
                       QuarantineDetail{.path = "D:/Library/_fsorganizer-quarantine/orphan"}});

    QCOMPARE(CellOf(model, 0, QuarantineModel::VersionColumn), QStringLiteral("2.4.1"));
    QVERIFY(model.data(model.index(0, QuarantineModel::NameColumn), QuarantineModel::ReplacedRole).toBool());
    QVERIFY(!model.data(model.index(1, QuarantineModel::NameColumn), QuarantineModel::ReplacedRole).toBool());

    QCOMPARE(model.DetailAt(model.index(0, 0))->replacementVersion, std::string{"2.5.0"});
}

void QuarantineModelTest::ASizeThatLandsReachesOnlyTheRowItBelongsTo()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());

    model.ShowSizes({MeasuredFolder{
        .folder = "E:/Sim/_fsorganizer-quarantine/simbridge", .bytes = 2ULL * 1024 * 1024 * 1024, .measured = true}});

    QVERIFY(model.data(model.index(0, QuarantineModel::NameColumn), SecondLineRole)
                .toString()
                .contains(QStringLiteral("GiB")));
    QVERIFY(!model.data(model.index(1, QuarantineModel::NameColumn), SecondLineRole)
                 .toString()
                 .contains(QStringLiteral("GiB")));
}

void QuarantineModelTest::ListingAgainForgetsTheVersionAndSizeOfTheOldRows()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());
    model.ShowDetails({QuarantineDetail{.path = "E:/Sim/_fsorganizer-quarantine/simbridge", .version = "2.4.1"}});
    model.ShowSizes({MeasuredFolder{
        .folder = "E:/Sim/_fsorganizer-quarantine/simbridge", .bytes = 2ULL * 1024 * 1024, .measured = true}});

    model.ShowItems(TwoItems());

    QVERIFY(CellOf(model, 0, QuarantineModel::VersionColumn).isEmpty());
    QVERIFY(!model.data(model.index(0, QuarantineModel::NameColumn), SecondLineRole)
                 .toString()
                 .contains(QStringLiteral("MiB")));
}

void QuarantineModelTest::TheWhenAndTheSizeShareTheSecondLineOfTheNameCell()
{
    QuarantineModel model;
    model.ShowItems(TwoItems());
    model.ShowSizes({MeasuredFolder{
        .folder = "E:/Sim/_fsorganizer-quarantine/simbridge", .bytes = 2ULL * 1024 * 1024 * 1024, .measured = true}});

    const QString line = model.data(model.index(0, QuarantineModel::NameColumn), SecondLineRole).toString();

    QVERIFY(line.contains(QStringLiteral("2026")));
    QVERIFY(line.contains(QStringLiteral("GiB")));
    QVERIFY(line.contains(QStringLiteral("\u00b7")));

    QVERIFY(model.data(model.index(1, QuarantineModel::NameColumn), SecondLineRole).toString().isEmpty());
    QVERIFY(model.data(model.index(0, QuarantineModel::OriginColumn), SecondLineRole).toString().isEmpty());
}

void QuarantineModelTest::TheSourceColumnTellsTheFourWaysAnOriginCanBeAnswered()
{
    QuarantineModel model;

    model.ShowItems({
        QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/only-beside",
                        .origin = "D:/Library/Utils/only-beside",
                        .source = OriginSource::Sidecar},
        QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/agreeing",
                        .origin = "D:/Library/Utils/agreeing",
                        .source = OriginSource::Sidecar,
                        .theOtherSourceSays = "D:/Library/Utils/agreeing"},
        QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/disagreeing",
                        .origin = "D:/Library/Misc/disagreeing",
                        .source = OriginSource::Sidecar,
                        .theOtherSourceSays = "D:/Library/Sceneries/disagreeing"},
        QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/from-the-journal",
                        .origin = "D:/Library/Utils/from-the-journal",
                        .source = OriginSource::Journal},
    });

    QCOMPARE(CellOf(model, 0, QuarantineModel::SourceColumn), QStringLiteral("the record only"));
    QCOMPARE(CellOf(model, 1, QuarantineModel::SourceColumn), QStringLiteral("the record and the Journal agree"));
    QCOMPARE(CellOf(model, 2, QuarantineModel::SourceColumn), QStringLiteral("they disagree"));
    QCOMPARE(CellOf(model, 3, QuarantineModel::SourceColumn), QStringLiteral("the Journal only"));
}

void QuarantineModelTest::OnlyTheDisagreementIsLoudEnoughToWearATag()
{
    QuarantineModel model;

    model.ShowItems({
        QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/agreeing",
                        .origin = "D:/Library/Utils/agreeing",
                        .source = OriginSource::Sidecar,
                        .theOtherSourceSays = "D:/Library/Utils/agreeing"},
        QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/disagreeing",
                        .origin = "D:/Library/Misc/disagreeing",
                        .source = OriginSource::Sidecar,
                        .theOtherSourceSays = "D:/Library/Sceneries/disagreeing"},
    });

    QVERIFY(model.data(model.index(0, QuarantineModel::SourceColumn), TagTextRole).toString().isEmpty());
    QVERIFY(model.data(model.index(0, QuarantineModel::SourceColumn), QuietRole).toBool());

    QCOMPARE(model.data(model.index(1, QuarantineModel::SourceColumn), TagTextRole).toString(),
             QStringLiteral("they disagree"));
    QVERIFY(!model.data(model.index(1, QuarantineModel::SourceColumn), QuietRole).toBool());
}

QTEST_APPLESS_MAIN(QuarantineModelTest)

#include "tst_quarantine_model.moc"
