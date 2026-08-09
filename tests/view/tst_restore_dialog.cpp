#include <QtTest/QtTest>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>

#include <filesystem>
#include <vector>

#include "application/model/RestorePlan.h"
#include "tests/support/PathPrinting.h"
#include "view/quarantine/RestoreDialog.h"

namespace
{
    class RestoreDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ACollisionWithAnAddonOccupantIsOfferedTheSwapAndNobodyIsPreselected();
        static void AnOccupantWithoutAManifestIsListedAsRefusedWithNoOfferAtAll();
        static void TheCountedLineSeparatesWhatGoesBackFromWhatReplaces();
        static void ARowThatNeedsAPlaceIsStillAskedInsteadOfBeingOfferedASwap();
        static void ARefusedComparisonLeavesTheRowAsItWas();
        static void TheTotalCountsTheReplacementsItSaysAreAmongThem();
    };

    const std::filesystem::path kHeld = "D:/Library/_fsorganizer-quarantine/simbridge";
    const std::filesystem::path kOccupied = "D:/Library/Utils/simbridge";

    RestoreOffer ACollisionThatCanBeSwapped()
    {
        return RestoreOffer{.check = RestoreCheck{.item = QuarantinedItem{.path = kHeld, .origin = kOccupied},
                                                  .target = kOccupied,
                                                  .result = FileResult::TheIdentityIsTaken,
                                                  .occupant = kOccupied,
                                                  .version = "2.4.1",
                                                  .occupantVersion = "2.5.0",
                                                  .occupantIsAnAddon = true}};
    }

    RestoreOffer ASettledRestore()
    {
        return RestoreOffer{
            .check = RestoreCheck{.item = QuarantinedItem{.path = "D:/Library/_fsorganizer-quarantine/fenix-a320",
                                                          .origin = "D:/Library/Aircraft/fenix-a320"},
                                  .target = "D:/Library/Aircraft/fenix-a320"}};
    }

    QPushButton* TheOfferIn(const RestoreDialog& dialog)
    {
        return dialog.findChild<QPushButton*>(QStringLiteral("CompareAndReplace"));
    }

    AskAboutTheCollision AlwaysAgrees()
    {
        return [](const RestoreCheck&)
        {
            return true;
        };
    }

    AskAboutTheCollision NeverAgrees()
    {
        return [](const RestoreCheck&)
        {
            return false;
        };
    }
}

void RestoreDialogTest::ACollisionWithAnAddonOccupantIsOfferedTheSwapAndNobodyIsPreselected()
{
    RestoreDialog dialog({ACollisionThatCanBeSwapped()}, AlwaysAgrees());

    QPushButton* offer = TheOfferIn(dialog);

    QVERIFY(offer != nullptr);
    QVERIFY(dialog.Restorable().empty());
    QVERIFY(dialog.TheOnesReplacingWhatIsThere().empty());

    offer->click();

    const std::vector<QuarantinedItem> replacing = dialog.TheOnesReplacingWhatIsThere();

    QCOMPARE(replacing.size(), std::size_t{1});
    QCOMPARE(replacing.front().path, kHeld);
    QVERIFY(dialog.Restorable().empty());
}

void RestoreDialogTest::AnOccupantWithoutAManifestIsListedAsRefusedWithNoOfferAtAll()
{
    RestoreOffer refused = ACollisionThatCanBeSwapped();
    refused.check.result = FileResult::TheOriginIsOccupied;
    refused.check.occupantIsAnAddon = false;
    refused.check.occupantVersion.clear();

    const RestoreDialog dialog({refused}, AlwaysAgrees());

    QVERIFY(TheOfferIn(dialog) == nullptr);
    QVERIFY(dialog.Restorable().empty());
    QVERIFY(dialog.TheOnesReplacingWhatIsThere().empty());
}

void RestoreDialogTest::TheCountedLineSeparatesWhatGoesBackFromWhatReplaces()
{
    RestoreDialog dialog({ASettledRestore(), ACollisionThatCanBeSwapped()}, AlwaysAgrees());

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    const auto counted = std::ranges::find_if(labels,
                                              [](const QLabel* label)
                                              {
                                                  return label->text().contains(QStringLiteral("will be restored"));
                                              });

    QVERIFY(counted != labels.end());
    QVERIFY(!(*counted)->text().contains(QStringLiteral("occupant")));

    TheOfferIn(dialog)->click();

    QVERIFY((*counted)->text().contains(QStringLiteral("occupant")));
    QCOMPARE(dialog.Restorable().size(), std::size_t{1});
    QCOMPARE(dialog.TheOnesReplacingWhatIsThere().size(), std::size_t{1});
}

void RestoreDialogTest::ARowThatNeedsAPlaceIsStillAskedInsteadOfBeingOfferedASwap()
{
    RestoreOffer lost = ASettledRestore();
    lost.check.item.origin.clear();
    lost.check.result = FileResult::TheOriginIsUnknown;
    lost.places = {
        RestorePlace{.place = "D:/Library/Aircraft", .target = "D:/Library/Aircraft/fenix-a320", .label = "Aircraft"}};

    const RestoreDialog dialog({lost}, AlwaysAgrees());

    QVERIFY(TheOfferIn(dialog) == nullptr);
    QVERIFY(dialog.findChild<QComboBox*>() != nullptr);
}

void RestoreDialogTest::ARefusedComparisonLeavesTheRowAsItWas()
{
    RestoreDialog dialog({ACollisionThatCanBeSwapped()}, NeverAgrees());

    TheOfferIn(dialog)->click();

    QVERIFY(dialog.TheOnesReplacingWhatIsThere().empty());
    QVERIFY(dialog.Restorable().empty());
}

void RestoreDialogTest::TheTotalCountsTheReplacementsItSaysAreAmongThem()
{
    RestoreDialog dialog({ACollisionThatCanBeSwapped()}, AlwaysAgrees());

    TheOfferIn(dialog)->click();

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    const auto counted = std::ranges::find_if(labels,
                                              [](const QLabel* label)
                                              {
                                                  return label->text().contains(QStringLiteral("will be restored"));
                                              });

    QVERIFY(counted != labels.end());
    QVERIFY(dialog.Restorable().empty());
    QCOMPARE(dialog.TheOnesReplacingWhatIsThere().size(), std::size_t{1});
    QVERIFY2(!(*counted)->text().startsWith(QStringLiteral("0 ")),
             "the button treats a replacement as work, so the sentence beside it cannot count zero");
}

QTEST_MAIN(RestoreDialogTest)

#include "tst_restore_dialog.moc"
