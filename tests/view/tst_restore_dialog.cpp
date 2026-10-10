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
#include "domain/importing/ImportPaths.h"
#include "support/PathText.h"
#include "tests/support/PathPrinting.h"
#include "view/quarantine/RestoreDialog.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/FailureText.h"

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
        static void AnItemInItsSwapSlotIsNamedByTheFolderItWasBeforeTheSwapInEveryKindOfRow();
        static void TheReasonOfACollisionIsWrittenApartFromTheQuietDetailUnderIt();
        static void TheOfferOfACollisionNeverBecomesTheDefaultButton();
        static void TheOfferOfACollisionStandsToTheRightOfItsReason();
        static void AgreeingToASwapLeavesEveryRowVisibleWithoutScrolling();
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

    QLabel* TheLabelSaying(const RestoreDialog& dialog, const QString& text)
    {
        const QList<QLabel*> labels = dialog.findChildren<QLabel*>();

        for (QLabel* label : labels)
        {
            if (label->text() == text)
            {
                return label;
            }
        }

        return nullptr;
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
    QVERIFY(!(*counted)->text().contains(QStringLiteral("what is there")));

    TheOfferIn(dialog)->click();

    QVERIFY((*counted)->text().contains(QStringLiteral("what is there")));
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

void RestoreDialogTest::AnItemInItsSwapSlotIsNamedByTheFolderItWasBeforeTheSwapInEveryKindOfRow()
{
    const std::filesystem::path quarantine = kHeld.parent_path();

    RestoreOffer settled = ASettledRestore();
    settled.check.item.path = SwapSlotFor(quarantine / "alpha");

    RestoreOffer asked = ASettledRestore();
    asked.check.item.path = SwapSlotFor(quarantine / "beta");
    asked.check.item.origin.clear();
    asked.check.result = FileResult::TheOriginIsUnknown;
    asked.places = {RestorePlace{.place = "D:/Library/Aircraft", .target = "D:/Library/Aircraft/beta"}};

    RestoreOffer collided = ACollisionThatCanBeSwapped();
    collided.check.item.path = SwapSlotFor(quarantine / "gamma");

    const RestoreDialog dialog({settled, asked, collided}, AlwaysAgrees());

    QStringList names;
    for (const QLabel* label : dialog.findChildren<QLabel*>())
    {
        QVERIFY2(!label->text().contains(QStringLiteral("fsorg-swap")), qPrintable(label->text()));
        names.push_back(label->text());
    }

    for (const QString& real : {QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")})
    {
        QVERIFY2(names.contains(real), qPrintable(real));
    }
}

void RestoreDialogTest::TheReasonOfACollisionIsWrittenApartFromTheQuietDetailUnderIt()
{
    const RestoreOffer collision = ACollisionThatCanBeSwapped();
    const RestoreDialog dialog({collision}, AlwaysAgrees());

    const QString reason = Explain(collision.check.result);
    const QLabel* said = TheLabelSaying(dialog, reason);

    QVERIFY2(said != nullptr, "the reason has no label of its own");
    QVERIFY(said->objectName() != QStringLiteral("PanelPromise"));

    const QString quiet = ThePromiseIn(dialog);

    QVERIFY(!quiet.contains(reason));
    QVERIFY(quiet.contains(AsText(collision.check.occupant)));
    QVERIFY(quiet.contains(QString::fromStdString(collision.check.version)));
    QVERIFY(quiet.contains(QString::fromStdString(collision.check.occupantVersion)));
}

void RestoreDialogTest::TheOfferOfACollisionNeverBecomesTheDefaultButton()
{
    RestoreDialog dialog({ACollisionThatCanBeSwapped()}, AlwaysAgrees());
    Expose(dialog);

    QPushButton* offer = TheOfferIn(dialog);
    offer->setFocus();

    QVERIFY(!offer->autoDefault());
    QVERIFY2(!offer->isDefault(), "the focused offer was promoted to the default button of the dialog");
}

void RestoreDialogTest::TheOfferOfACollisionStandsToTheRightOfItsReason()
{
    const RestoreOffer collision = ACollisionThatCanBeSwapped();
    RestoreDialog dialog({collision}, AlwaysAgrees());
    Expose(dialog);

    const QPushButton* offer = TheOfferIn(dialog);
    const QLabel* said = TheLabelSaying(dialog, Explain(collision.check.result));

    QVERIFY(said != nullptr);

    const QRect reason(said->mapTo(&dialog, QPoint(0, 0)), said->size());
    const QRect button(offer->mapTo(&dialog, QPoint(0, 0)), offer->size());

    QVERIFY2(button.left() > reason.right(), "the offer is not to the right of the reason");
    QVERIFY2(button.top() < reason.bottom(), "the offer starts below the reason instead of beside it");
}

void RestoreDialogTest::AgreeingToASwapLeavesEveryRowVisibleWithoutScrolling()
{
    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));

    RestoreOffer deep = ACollisionThatCanBeSwapped();
    deep.check.item.path = kHeld.parent_path() / "a-folder-name-long-enough-to-squeeze-the-reason";
    deep.check.occupant =
        "D:/Library/Aircraft/Vendor With A Long Enough Name/Second Level Of Folders/Third Level Of Folders/simbridge";

    RestoreDialog dialog({ACollisionThatCanBeSwapped(), deep, ASettledRestore()}, AlwaysAgrees());
    Expose(dialog);

    const QScrollArea* scroll = TheScrollIn(dialog);

    QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);

    TheOfferIn(dialog)->click();
    QCoreApplication::processEvents();

    QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
}

QTEST_MAIN(RestoreDialogTest)

#include "tst_restore_dialog.moc"
