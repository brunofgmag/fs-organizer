#include <QtTest/QtTest>

#include <QtCore/QTranslator>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>

#include <algorithm>
#include <vector>

#include "tests/support/ButtonLookup.h"
#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/FixedHeadings.h"
#include "tests/support/InstalledCatalogue.h"
#include "view/library/SharedAirportsDialog.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class SharedAirportsDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void initTestCase();
        static void BothAddonsAreNamedAndTheAirportRidesAlongTheLine();
        static void AThousandCodesOfOnePairStayOneLine();
        static void NeitherAnswerIsADeadEndAndOnlyOneOfThemWrites();
        static void NothingIsRememberedUntilAPairIsChecked();
        static void OnlyTheCheckedPairsComeBack();
        static void NeitherSideOfTheOverlapIsCalledAProblem();
        static void TheWarningSaysTheCodeWasReadFromTheScenery();
        static void TheHeadingsLeaveTheScrollAndHoldTheFourCatalogueHeadings();
        static void EachHeadingStandsOverItsColumn_data();
        static void EachHeadingStandsOverItsColumn();
        static void ScrollingTheListMovesTheRowsAndLeavesTheHeadingsAndCheckAllWhereTheyAre();
        static void NamesThatCannotWrapScrollAcrossAndTheHeadingsFollowAtBothEnds();
        static void TheButtonRowFitsTheDialogInPortugueseWithFortyAddons();
        static void NothingIsCutInEitherLanguage_data();
        static void NothingIsCutInEitherLanguage();
        static void ShortListsNeverShowTheBarAndOneRowIsAsTallAsItself();
        static void ExactlyTwoRulesFrameTheListAndTheListHasNoFrame();
        static void TheTwoNameColumnsTakeTheSpareWidthAndNotTheAirportsColumn();
        static void TheThirdButtonIsEnabledWithNothingChecked();
        static void TheThirdButtonIsPlainSoOnlyKeepingBothIsPrimary();
        static void TheThirdButtonCountsTheAddonsYouEnabled();
        static void ALineForEachPairCountsTheAddonOnceInTheButtonAndInTheExplanation();
        static void TheThirdButtonEndsTheDialogWithItsAnswerAndHandsOverWhatWasEnabled();
        static void KeepingBothStaysThePrimaryAnswerAndEnterAndEscapeKeepThem();
    };

    constexpr int kTallestWindow = 760;
    constexpr int kRoomForAnyList = 2400;
    constexpr int kDialogWidth = 700;

    [[nodiscard]] SharedAirportsLine Sharing(const QString& turningOn, const QString& alreadyOn, QStringList codes)
    {
        return {.turningOn = turningOn,
                .alreadyOn = alreadyOn,
                .codes = std::move(codes),
                .one = {.libraryId = "library-1", .folderName = turningOn.toStdString()},
                .other = {.libraryId = "library-1", .folderName = alreadyOn.toStdString()}};
    }

    [[nodiscard]] std::vector<SharedAirportsLine> ManyPairs(const int count)
    {
        const QStringList airports = {QStringLiteral("EHAM"), QStringLiteral("LFPG"), QStringLiteral("LEBL"),
                                      QStringLiteral("SBGL"), QStringLiteral("SBGR"), QStringLiteral("LPPT")};
        std::vector<SharedAirportsLine> lines;

        for (int at = 0; at < count; ++at)
        {
            const QString& code = airports.at(at % airports.size());
            const QStringList codes = at % 3 == 0 ? airports : QStringList{code};

            lines.push_back(Sharing(
                QStringLiteral("ferro-airport-%1-some-long-city-and-field-name-v%2").arg(code.toLower()).arg(at),
                QStringLiteral("asobo-airport-%1-another-long-city-and-field-name-v%2").arg(code.toLower()).arg(at),
                codes));
        }

        return lines;
    }

    [[nodiscard]] std::vector<QCheckBox*> PairBoxesOf(const QDialog& dialog)
    {
        std::vector<QCheckBox*> boxes;

        for (QCheckBox* box : dialog.findChildren<QCheckBox*>())
        {
            if (box->text().isEmpty())
            {
                boxes.push_back(box);
            }
        }

        return boxes;
    }

    [[nodiscard]] const QCheckBox* CheckAllOf(const QDialog& dialog)
    {
        for (const QCheckBox* box : dialog.findChildren<QCheckBox*>())
        {
            if (!box->text().isEmpty())
            {
                return box;
            }
        }

        return nullptr;
    }

    [[nodiscard]] QPushButton* TurnBackOffOf(const QDialog& dialog)
    {
        return dialog.findChild<QPushButton*>(QStringLiteral("DisableWhatWasEnabled"));
    }

    [[nodiscard]] QStringList TextsOf(const QDialog& dialog)
    {
        QStringList said;
        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        for (const QPushButton* button : dialog.findChildren<QPushButton*>())
        {
            said.append(button->text());
        }

        return said;
    }

    struct Window
    {
        explicit Window(const int tall)
        {
            parent.resize(1000, tall);
        }

        QWidget parent;
    };

    void AddEveryLanguageAndSize()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<int>("pairs");

        for (const QString& language : {QStringLiteral("en"), QStringLiteral("pt_BR")})
        {
            for (const int pairs : {1, 3, 40})
            {
                QTest::newRow(qPrintable(QStringLiteral("%1, %2 pairs").arg(language).arg(pairs))) << language << pairs;
            }
        }
    }
}

void SharedAirportsDialogTest::initTestCase()
{
    ApplyModernistTheme(*qApp);
}

void SharedAirportsDialogTest::BothAddonsAreNamedAndTheAirportRidesAlongTheLine()
{
    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("flytampa-airport-eham-amsterdam"), QStringLiteral("asobo-airport-eham-amsterdam"),
                 {QStringLiteral("EHAM")})});

    const QStringList said = TextsOf(dialog);

    QVERIFY(said.contains(QStringLiteral("flytampa-airport-eham-amsterdam")));
    QVERIFY2(said.contains(QStringLiteral("asobo-airport-eham-amsterdam")),
             "the one that was already on is the half the user cannot see from the gesture they just made");
    QVERIFY(said.contains(QStringLiteral("EHAM")));
}

void SharedAirportsDialogTest::AThousandCodesOfOnePairStayOneLine()
{
    QStringList many;
    for (int at = 0; at < 1403; ++at)
    {
        many << QStringLiteral("EH%1").arg(at);
    }

    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("navigraph-nav-jepp"), QStringLiteral("navigraph-nav-base"), many)});

    const QStringList said = TextsOf(dialog);

    QVERIFY2(said.filter(QStringLiteral("navigraph-nav-base")).size() == 1,
             "the two data packages of the reference installation share 1403 codes, and the unit of the list is the "
             "pair of addons, never the code");
    QVERIFY2(said.join(QLatin1Char(' ')).contains(QStringLiteral("1399 more")),
             "the codes are the detail of the pair, spelled out while they are few and counted once they are not");
}

void SharedAirportsDialogTest::NeitherAnswerIsADeadEndAndOnlyOneOfThemWrites()
{
    SharedAirportsDialog agreeing(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});
    QSignalSpy accepted(&agreeing, &QDialog::accepted);
    PairBoxesOf(agreeing).front()->setChecked(true);
    ButtonSaying(agreeing, QStringLiteral("Stop warning about the checked ones"))->click();
    QCOMPARE(accepted.count(), 1);

    SharedAirportsDialog refusing(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});
    QSignalSpy rejected(&refusing, &QDialog::rejected);
    ButtonSaying(refusing, QStringLiteral("Keep both enabled"))->click();
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(refusing.result(), static_cast<int>(QDialog::Rejected));
    QVERIFY2(refusing.Chosen().empty(), "refusing writes nothing, which is what it promises");
}

void SharedAirportsDialogTest::NothingIsRememberedUntilAPairIsChecked()
{
    SharedAirportsDialog dialog({Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});

    QVERIFY2(!ButtonSaying(dialog, QStringLiteral("Stop warning about the checked ones"))->isEnabled(),
             "the pairs come unchecked, so the answer that writes has nothing to write yet");
    QVERIFY(dialog.Chosen().empty());

    PairBoxesOf(dialog).front()->setChecked(true);

    QVERIFY(ButtonSaying(dialog, QStringLiteral("Stop warning about the checked ones"))->isEnabled());
}

void SharedAirportsDialogTest::OnlyTheCheckedPairsComeBack()
{
    SharedAirportsDialog dialog({Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")}),
                                 Sharing(QStringLiteral("other"), QStringLiteral("hers"), {QStringLiteral("LPMA")}),
                                 Sharing(QStringLiteral("third"), QStringLiteral("yours"), {QStringLiteral("LEBL")})});

    const std::vector<QCheckBox*> boxes = PairBoxesOf(dialog);
    QCOMPARE(boxes.size(), std::size_t{3});

    boxes.at(1)->setChecked(true);

    const std::vector<CoexistingPair> chosen = dialog.Chosen();

    QCOMPARE(chosen.size(), std::size_t{1});
    QVERIFY2(chosen.front().one.folderName == "other" && chosen.front().other.folderName == "hers",
             "enabling a category raises many pairs at once, and the user settles them one by one");
}

void SharedAirportsDialogTest::NeitherSideOfTheOverlapIsCalledAProblem()
{
    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("flytampa-airport-eham-amsterdam"), QStringLiteral("asobo-airport-eham-amsterdam"),
                 {QStringLiteral("EHAM")})});

    const QStringList said = TextsOf(dialog);

    QVERIFY2(!said.filter(QStringLiteral("Disable the ")).isEmpty(),
             "the texts walked below include the buttons, and the third one is among them");

    for (const QString& line : said)
    {
        for (const QString& blame : {QStringLiteral("problem"), QStringLiteral("broken"), QStringLiteral("wrong"),
                                     QStringLiteral("conflict"), QStringLiteral("error")})
        {
            QVERIFY2(!line.contains(blame, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("the rule of 2026-07-31 forbids calling either side a problem, and "
                                               "this line says \"%1\": %2")
                                    .arg(blame, line)));
        }
    }
}

void SharedAirportsDialogTest::TheWarningSaysTheCodeWasReadFromTheScenery()
{
    const SharedAirportsDialog dialog(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});

    const QString said = TextsOf(dialog).join(QLatin1Char(' '));

    QVERIFY2(said.contains(QStringLiteral("scenery")) && said.contains(QStringLiteral("already been scanned")),
             "the claim and where it came from travel together, and here the second half is also the limit: the app "
             "compares against the scenery it has read and not against the whole library");
}

void SharedAirportsDialogTest::TheHeadingsLeaveTheScrollAndHoldTheFourCatalogueHeadings()
{
    Window window(kRoomForAnyList);
    SharedAirportsDialog dialog(ManyPairs(3), &window.parent);
    ShowItSettled(dialog);

    VerifyTheHeadingsAreOutsideTheScroll(dialog,
                                         {QStringLiteral("Can coexist"), QStringLiteral("You enabled"),
                                          QStringLiteral("Already enabled"), QStringLiteral("Airports")});
}

void SharedAirportsDialogTest::EachHeadingStandsOverItsColumn_data()
{
    AddEveryLanguageAndSize();
}

void SharedAirportsDialogTest::EachHeadingStandsOverItsColumn()
{
    QFETCH(QString, language);
    QFETCH(int, pairs);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Window window(kTallestWindow);
    SharedAirportsDialog dialog(ManyPairs(pairs), &window.parent);
    ShowItSettled(dialog);

    VerifyEachHeadingStandsOverItsColumn(dialog);
}

void SharedAirportsDialogTest::ScrollingTheListMovesTheRowsAndLeavesTheHeadingsAndCheckAllWhereTheyAre()
{
    Window window(kTallestWindow);
    SharedAirportsDialog dialog(ManyPairs(40), &window.parent);
    ShowItSettled(dialog);

    const QCheckBox* all = CheckAllOf(dialog);
    QVERIFY(all != nullptr && all->isVisible());

    const int checkAllBefore = PlaceIn(dialog, *all).y();

    VerifyTheListScrollsForRealAndTheHeadingsStay(dialog);

    QCOMPARE(PlaceIn(dialog, *all).y(), checkAllBefore);
    QVERIFY2(PlaceIn(dialog, *all).y() + all->height() <= PlaceIn(dialog, *HeadingsOf(dialog)).y(),
             "Check all belongs above the headings, outside what scrolls");
}

void SharedAirportsDialogTest::NamesThatCannotWrapScrollAcrossAndTheHeadingsFollowAtBothEnds()
{
    const QString yours = QStringLiteral("flytampa_airport_eham_amsterdam_schiphol_v2_final_x");
    const QString theirs = QStringLiteral("asobo_airport_eham_amsterdam_schiphol_v2_final_x");

    Window window(kRoomForAnyList);
    SharedAirportsDialog dialog({Sharing(yours, theirs, {QStringLiteral("EHAM")})}, &window.parent);
    ShowItSettled(dialog);

    QScrollBar* across = ListOf(dialog)->horizontalScrollBar();
    QVERIFY2(across->isVisible() && across->maximum() > 0, "two names this long cannot fit across 700 px");

    VerifyEachHeadingStandsOverItsColumn(dialog);

    across->setValue(across->maximum());
    QCoreApplication::processEvents();

    QVERIFY(across->value() > 0);
    VerifyEachHeadingStandsOverItsColumn(dialog);

    across->setValue(across->minimum());
    QCoreApplication::processEvents();

    VerifyEachHeadingStandsOverItsColumn(dialog);
}

void SharedAirportsDialogTest::TheButtonRowFitsTheDialogInPortugueseWithFortyAddons()
{
    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, QStringLiteral("pt_BR")),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Window window(kTallestWindow);
    SharedAirportsDialog dialog(ManyPairs(40), &window.parent);
    ShowItSettled(dialog);

    QCOMPARE(dialog.width(), kDialogWidth);

    const auto* box = dialog.findChild<QDialogButtonBox*>();
    QList<QAbstractButton*> inOrder = box->buttons();
    std::ranges::sort(inOrder,
                      [](const QAbstractButton* left, const QAbstractButton* right)
                      {
                          return left->x() < right->x();
                      });

    QCOMPARE(inOrder.size(), qsizetype{3});
    QVERIFY(box->minimumSizeHint().width() + 2 * kPageGutter <= kDialogWidth);

    for (qsizetype at = 0; at < inOrder.size(); ++at)
    {
        QVERIFY2(inOrder.at(at)->width() >= inOrder.at(at)->sizeHint().width(),
                 qPrintable(QStringLiteral("the button \"%1\" is cut").arg(inOrder.at(at)->text())));

        if (at > 0)
        {
            QVERIFY(inOrder.at(at - 1)->x() + inOrder.at(at - 1)->width() <= inOrder.at(at)->x());
        }
    }

    QVERIFY(inOrder.back()->x() + inOrder.back()->width() <= box->width());
}

void SharedAirportsDialogTest::NothingIsCutInEitherLanguage_data()
{
    AddEveryLanguageAndSize();
}

void SharedAirportsDialogTest::NothingIsCutInEitherLanguage()
{
    QFETCH(QString, language);
    QFETCH(int, pairs);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Window window(kTallestWindow);
    SharedAirportsDialog dialog(ManyPairs(pairs), &window.parent);
    ShowItSettled(dialog);

    for (const QLabel* label : dialog.findChildren<QLabel*>())
    {
        if (!label->isVisible())
        {
            continue;
        }

        if (label->wordWrap())
        {
            QVERIFY2(label->height() >= label->heightForWidth(label->width()),
                     qPrintable(QStringLiteral("this wrapped text is cut: %1").arg(label->text())));
        }
        else
        {
            QVERIFY2(label->width() >= label->sizeHint().width(),
                     qPrintable(QStringLiteral("this text is cut: %1").arg(label->text())));
        }
    }
}

void SharedAirportsDialogTest::ShortListsNeverShowTheBarAndOneRowIsAsTallAsItself()
{
    for (const int pairs : {1, 3, 12})
    {
        Window window(kRoomForAnyList);
        SharedAirportsDialog dialog(ManyPairs(pairs), &window.parent);
        ShowItSettled(dialog);

        QVERIFY2(!ListOf(dialog)->verticalScrollBar()->isVisible(),
                 qPrintable(QStringLiteral("%1 pairs fit and still show a bar").arg(pairs)));
    }

    Window window(kRoomForAnyList);
    SharedAirportsDialog dialog({Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})},
                                &window.parent);
    ShowItSettled(dialog);

    const QScrollArea* list = ListOf(dialog);

    QCOMPARE(list->viewport()->height(), list->widget()->height());
    QVERIFY2(list->height() < 68, "a single row is as tall as itself and not as the 68 px floor of a scroll area");
}

void SharedAirportsDialogTest::ExactlyTwoRulesFrameTheListAndTheListHasNoFrame()
{
    Window window(kRoomForAnyList);
    SharedAirportsDialog dialog(ManyPairs(3), &window.parent);
    ShowItSettled(dialog);

    VerifyTwoRulesFrameTheList(dialog);
}

void SharedAirportsDialogTest::TheTwoNameColumnsTakeTheSpareWidthAndNotTheAirportsColumn()
{
    Window window(kRoomForAnyList);
    SharedAirportsDialog dialog({Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})},
                                &window.parent);
    ShowItSettled(dialog);

    const std::vector<QWidget*> cells = FirstRowOf(dialog);

    QVERIFY2(cells[1]->width() > cells[3]->width() && cells[2]->width() > cells[3]->width(),
             "with room to spare, the names are what grows and the airports stay as wide as their text");
    QCOMPARE(cells[1]->width(), cells[2]->width());
}

void SharedAirportsDialogTest::TheThirdButtonIsEnabledWithNothingChecked()
{
    const SharedAirportsDialog dialog(ManyPairs(3));

    QVERIFY(PairBoxesOf(dialog).size() == 3 && dialog.Chosen().empty());
    QVERIFY2(TurnBackOffOf(dialog)->isEnabled(), "turning back off needs no pair to be checked");
}

void SharedAirportsDialogTest::TheThirdButtonIsPlainSoOnlyKeepingBothIsPrimary()
{
    const SharedAirportsDialog dialog(ManyPairs(3));

    QVERIFY2(!TurnBackOffOf(dialog)->property("role").isValid(), "only Keep both enabled is primary");
    QVERIFY(!ButtonSaying(dialog, QStringLiteral("Stop warning about the checked ones"))->property("role").isValid());
}

void SharedAirportsDialogTest::TheThirdButtonCountsTheAddonsYouEnabled()
{
    QTranslator catalogue;
    QVERIFY(LoadedTheShippedCatalogue(catalogue, QStringLiteral("en")));
    const Installed installed(catalogue);

    const SharedAirportsDialog one(ManyPairs(1));
    QCOMPARE(TurnBackOffOf(one)->text(), QStringLiteral("Disable what you enabled"));

    const SharedAirportsDialog three(ManyPairs(3));
    QCOMPARE(TurnBackOffOf(three)->text(), QStringLiteral("Disable the 3 you enabled"));
}

void SharedAirportsDialogTest::ALineForEachPairCountsTheAddonOnceInTheButtonAndInTheExplanation()
{
    QTranslator catalogue;
    QVERIFY(LoadedTheShippedCatalogue(catalogue, QStringLiteral("en")));
    const Installed installed(catalogue);

    const SharedAirportsDialog dialog({Sharing(QStringLiteral("a"), QStringLiteral("x"), {QStringLiteral("EHAM")}),
                                       Sharing(QStringLiteral("a"), QStringLiteral("y"), {QStringLiteral("EHAM")}),
                                       Sharing(QStringLiteral("b"), QStringLiteral("x"), {QStringLiteral("LFPG")})});

    QCOMPARE(TurnBackOffOf(dialog)->text(), QStringLiteral("Disable the 2 you enabled"));
    QCOMPARE(TextsOf(dialog).filter(QStringLiteral("2 addons you just enabled")).size(), qsizetype{1});
    QCOMPARE(dialog.WhatWasEnabled().size(), std::size_t{2});
}

void SharedAirportsDialogTest::TheThirdButtonEndsTheDialogWithItsAnswerAndHandsOverWhatWasEnabled()
{
    SharedAirportsDialog dialog({Sharing(QStringLiteral("a"), QStringLiteral("x"), {QStringLiteral("EHAM")}),
                                 Sharing(QStringLiteral("a"), QStringLiteral("y"), {QStringLiteral("EHAM")}),
                                 Sharing(QStringLiteral("b"), QStringLiteral("x"), {QStringLiteral("LFPG")})});
    const QSignalSpy finished(&dialog, &QDialog::finished);

    TurnBackOffOf(dialog)->click();

    QCOMPARE(finished.size(), 1);
    QCOMPARE(dialog.result(), static_cast<int>(SharedAirportsAnswer::DisableWhatWasEnabled));
    QCOMPARE(dialog.WhatWasEnabled().size(), std::size_t{2});
    QCOMPARE(dialog.WhatWasEnabled().front().folderName, std::string("a"));
    QCOMPARE(dialog.WhatWasEnabled().back().folderName, std::string("b"));
    QVERIFY(dialog.Chosen().empty());
}

void SharedAirportsDialogTest::KeepingBothStaysThePrimaryAnswerAndEnterAndEscapeKeepThem()
{
    SharedAirportsDialog entering(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});
    const QPushButton* keep = ButtonSaying(entering, QStringLiteral("Keep both enabled"));

    QCOMPARE(keep->property("role").toString(), QStringLiteral("primary"));
    QVERIFY(keep->isDefault());

    ShowItSettled(entering);
    QTest::keyClick(&entering, Qt::Key_Return);

    QCOMPARE(entering.result(), static_cast<int>(SharedAirportsAnswer::KeepBoth));
    QVERIFY(!entering.isVisible());

    SharedAirportsDialog escaping(
        {Sharing(QStringLiteral("mine"), QStringLiteral("theirs"), {QStringLiteral("EHAM")})});

    ShowItSettled(escaping);
    QTest::keyClick(&escaping, Qt::Key_Escape);

    QCOMPARE(escaping.result(), static_cast<int>(SharedAirportsAnswer::KeepBoth));
    QVERIFY(!escaping.isVisible());
}

QTEST_MAIN(SharedAirportsDialogTest)

#include "tst_shared_airports_dialog.moc"
