#include <QtTest/QtTest>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>

#include <filesystem>
#include <string>
#include <vector>

#include "application/model/RestorePlan.h"
#include "support/PathText.h"
#include "tests/support/PathPrinting.h"
#include "view/quarantine/RestoreDialog.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class RestoreDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void ACollisionWithAnAddonOccupantIsOfferedTheSwapAndNobodyIsPreselected();
        static void AnOccupantWithoutAManifestIsListedAsRefusedWithNoOfferAtAll();
        static void TheCountedLineSeparatesWhatGoesBackFromWhatReplaces();
        static void ARowWhoseOriginWearsALinkSaysTheLinkGoesAwayWithIt();
        static void ARowThatNeedsAPlaceIsStillAskedInsteadOfBeingOfferedASwap();
        static void ARefusedComparisonLeavesTheRowAsItWas();
        static void TheTotalCountsTheReplacementsItSaysAreAmongThem();
        static void EveryOfferIsVisibleWithoutScrolling();
        static void TheViewportIsNoTallerThanWhatTheOffersNeed();
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

    QString ThePromiseIn(const RestoreDialog& dialog)
    {
        const QLabel* promise = dialog.findChild<QLabel*>(QStringLiteral("PanelPromise"));

        return promise == nullptr ? QString{} : promise->text();
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

    std::vector<RestoreOffer> ManyOffers(const int count)
    {
        const std::filesystem::path deep =
            "D:/Library/Aircraft/Vendor With A Long Enough Name/Second Level Of Folders/Third Level Of Folders";

        std::vector<RestoreOffer> offers;

        for (int index = 0; index < count; ++index)
        {
            const std::string name = "addon-" + std::to_string(index);

            offers.push_back(RestoreOffer{
                .check =
                    RestoreCheck{.item = QuarantinedItem{.path = kHeld.parent_path() / name, .origin = deep / name},
                                 .target = deep / name}});
        }

        return offers;
    }

    QScrollArea* TheScrollIn(const RestoreDialog& dialog)
    {
        return dialog.findChild<QScrollArea*>();
    }

    void Expose(RestoreDialog& dialog)
    {
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
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

void RestoreDialogTest::ARowWhoseOriginWearsALinkSaysTheLinkGoesAwayWithIt()
{
    RestoreOffer wearing = ASettledRestore();
    wearing.check.theOriginHoldsALink = true;

    const RestoreDialog withTheLink({wearing}, AlwaysAgrees());
    const RestoreDialog plain({ASettledRestore()}, AlwaysAgrees());

    const QString place = AsText(wearing.check.target.parent_path());

    QVERIFY(!ThePromiseIn(plain).isEmpty());
    QVERIFY(ThePromiseIn(plain).contains(place));
    QVERIFY(ThePromiseIn(withTheLink) != ThePromiseIn(plain));
    QVERIFY(ThePromiseIn(withTheLink).contains(place));
    QCOMPARE(withTheLink.Restorable().size(), std::size_t{1});
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

void RestoreDialogTest::EveryOfferIsVisibleWithoutScrolling()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    RestoreDialog dialog(ManyOffers(8), AlwaysAgrees());
    Expose(dialog);

    QScrollArea* scroll = TheScrollIn(dialog);

    QVERIFY(scroll != nullptr);
    QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
}

void RestoreDialogTest::TheViewportIsNoTallerThanWhatTheOffersNeed()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    RestoreDialog dialog(ManyOffers(4), AlwaysAgrees());
    Expose(dialog);

    const QScrollArea* scroll = TheScrollIn(dialog);
    const int needed = scroll->widget()->heightForWidth(scroll->viewport()->width());

    QVERIFY(needed > 0);
    QVERIFY2(scroll->viewport()->height() - needed < dialog.fontMetrics().height(),
             qPrintable(QStringLiteral("the viewport is %1 tall for content that needs %2")
                            .arg(scroll->viewport()->height())
                            .arg(needed)));
}

QTEST_MAIN(RestoreDialogTest)

#include "tst_restore_dialog.moc"
