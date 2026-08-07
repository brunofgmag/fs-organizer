#include <QtTest/QtTest>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>

#include <filesystem>

#include "application/model/RestorePlan.h"
#include "tests/support/PathPrinting.h"
#include "view/quarantine/CollisionDialog.h"

namespace
{
    class CollisionDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void BothSidesAreNamedWithTheirVersionAndTheirSize();
        static void ASideNobodyCouldMeasureSaysSoInsteadOfShowingZero();
        static void ASideWithoutAVersionInItsManifestStillShowsTheSize();
        static void NothingIsPreselectedAndTheReplaceButtonIsNotTheDefault();
        static void TheDialogOpensBeforeTheMeasurementAndFillsInWhenItLands();
    };

    RestoreCheck ACollision()
    {
        return RestoreCheck{.item = QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/simbridge",
                                                    .origin = "D:/Library/Utils/simbridge"},
                            .target = "D:/Library/Utils/simbridge",
                            .result = FileResult::TheIdentityIsTaken,
                            .occupant = "D:/Library/Utils/simbridge",
                            .version = "2.4.1",
                            .occupantVersion = "2.5.0",
                            .occupantIsAnAddon = true};
    }

    QStringList EverythingWritten(const CollisionDialog& dialog)
    {
        QStringList said;

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said;
    }

    bool Says(const CollisionDialog& dialog, const QString& wanted)
    {
        return EverythingWritten(dialog).join(QStringLiteral("\n")).contains(wanted);
    }
}

void CollisionDialogTest::BothSidesAreNamedWithTheirVersionAndTheirSize()
{
    CollisionDialog dialog(ACollision());
    dialog.ShowTheSizes(TwoSides{.held = MeasuredFolder{.bytes = 2040109466, .measured = true},
                                 .occupant = MeasuredFolder{.bytes = 2254857830, .measured = true}});

    QVERIFY(Says(dialog, QStringLiteral("simbridge")));
    QVERIFY(Says(dialog, QStringLiteral("2.4.1")));
    QVERIFY(Says(dialog, QStringLiteral("2.5.0")));
    QVERIFY(Says(dialog, QStringLiteral("1,90 GiB")));
    QVERIFY(Says(dialog, QStringLiteral("2,10 GiB")));
}

void CollisionDialogTest::ASideNobodyCouldMeasureSaysSoInsteadOfShowingZero()
{
    CollisionDialog dialog(ACollision());
    dialog.ShowTheSizes(
        TwoSides{.held = MeasuredFolder{.bytes = 2040109466, .measured = true}, .occupant = MeasuredFolder{}});

    QVERIFY(Says(dialog, QStringLiteral("1,90 GiB")));
    QVERIFY(!Says(dialog, QStringLiteral("0 B")));
    QVERIFY(Says(dialog, QStringLiteral("could not be measured")));
}

void CollisionDialogTest::ASideWithoutAVersionInItsManifestStillShowsTheSize()
{
    RestoreCheck quiet = ACollision();
    quiet.occupantVersion.clear();

    CollisionDialog dialog(quiet);
    dialog.ShowTheSizes(TwoSides{.held = MeasuredFolder{.bytes = 2040109466, .measured = true},
                                 .occupant = MeasuredFolder{.bytes = 2254857830, .measured = true}});

    QVERIFY(Says(dialog, QStringLiteral("2,10 GiB")));
    QVERIFY(Says(dialog, QStringLiteral("the manifest does not say")));
}

void CollisionDialogTest::NothingIsPreselectedAndTheReplaceButtonIsNotTheDefault()
{
    const CollisionDialog dialog(ACollision());

    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    QVERIFY(buttons != nullptr);

    const QPushButton* cancel = buttons->button(QDialogButtonBox::Cancel);
    QVERIFY(cancel != nullptr);
    QVERIFY(cancel->isDefault());

    const QPushButton* replace = dialog.findChild<QPushButton*>(QStringLiteral("ReplaceWhatIsThere"));
    QVERIFY(replace != nullptr);
    QVERIFY(!replace->isDefault());
}

void CollisionDialogTest::TheDialogOpensBeforeTheMeasurementAndFillsInWhenItLands()
{
    CollisionDialog dialog(ACollision());

    QVERIFY(Says(dialog, QStringLiteral("2.4.1")));
    QVERIFY(Says(dialog, QStringLiteral("could not be measured")));
    QVERIFY(!Says(dialog, QStringLiteral("GiB")));

    dialog.ShowTheSizes(TwoSides{.held = MeasuredFolder{.bytes = 2040109466, .measured = true},
                                 .occupant = MeasuredFolder{.bytes = 2254857830, .measured = true}});

    QVERIFY(Says(dialog, QStringLiteral("1,90 GiB")));
    QVERIFY(Says(dialog, QStringLiteral("2.4.1")));
    QVERIFY(!Says(dialog, QStringLiteral("could not be measured")));
}

QTEST_MAIN(CollisionDialogTest)

#include "tst_collision_dialog.moc"
