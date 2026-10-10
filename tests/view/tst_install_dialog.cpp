#include <QtTest/QtTest>

#include <QtCore/QTranslator>
#include <QtGui/QImage>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeView>

#include <functional>
#include <utility>
#include <vector>

#include "support/SizeText.h"
#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/InstalledCatalogue.h"
#include "view/library/InstallDialog.h"
#include "view/theme/ModernistTheme.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    class InstallDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void initTestCase();
        static void TheDialogIsEightHundredPixelsWide();
        static void TheDialogIsNeverTallerThanTheWindowItOpensOver();
        static void TheListStopsAtTwelveRowsAndScrollsBeyondThem();
        static void NoCellAndNoSentenceRowIsCutInEitherLanguage_data();
        static void NoCellAndNoSentenceRowIsCutInEitherLanguage();
        static void InstallNeedsACategoryWhileANewAddonIsChecked();
        static void InstallIsDisabledWithNothingChecked();
        static void InstallIsDisabledWhileTheSimulatorNoticeShows();
        static void TheInstallLabelCarriesTheCountInEitherLanguage_data();
        static void TheInstallLabelCarriesTheCountInEitherLanguage();
        static void TheTotalSaysHowManyAreCheckedAndHowBig();
        static void CheckAllIsTriStateAndFollowsWhatIsChecked();
        static void CheckAllNeverChecksARowWaitingForTheDiscard();
        static void TheDiscardBoxAndItsAddonMoveTogetherInBothDirections();
        static void ChangingTheLibraryChangesTheInstalledColumnTheCategoryColumnAndTheSentenceRows();
        static void ChangingTheCategoryMovesTheFolderInTheWayToTheCategoryThatHasIt();
        static void TheDiscardTheUserTickedSurvivesAChangeOfCategoryButNotOfLibrary();
        static void ChosenCarriesTheOccupantOfTheCopyARowReplaces();
        static void ARowThatReplacesShowsTheCategoryOfTheInstalledCopyAndNotTheChosenOne();
        static void TheCategoryComboIsDisabledWhenNoCheckedAddonIsNew();
        static void AnOfferOfReplacementsAloneIsSentWithTheRootOfTheChosenLibraryWhenNoCategoryIsChosen();
        static void ACategoryTheUserChoseIsSentInsteadOfTheRootOfTheLibrary();
        static void TheDisabledCategoryFieldNamesTheCategoryOfTheOnlyReplacedAddon();
        static void TheDisabledCategoryFieldSaysEachAddonStaysWhenTheReplacedOnesAreInDifferentCategories();
        static void TheCategoryTheUserChoseComesBackWhenANewAddonIsCheckedAgain();
        static void TheParagraphCountsTheAddonsThatAreAlreadyInstalled();
        static void TheCategoryFieldPaintsWhatItWasToldToSayInstead();
        static void EnableAfterInstallingStartsUnchecked();
        static void DroppedOnACategoryThatCategoryIsPreselected();
        static void ChosenReturnsTheArchiveTheAddonAndTheDiscardOfEachCheckedAddon();
        static void ChosenCarriesEachFieldTheFlowNeedsFromTheLineItCameFrom_data();
        static void ChosenCarriesEachFieldTheFlowNeedsFromTheLineItCameFrom();
        static void ChosenCarriesWhetherTheSourceWasAFolder();
        static void AFreshOfferKeepsTheChecksAndUpdatesTheStandingsAndTheNotice();
        static void AFreshOfferKeepsTheLibraryAndTheCategoryTheUserChose();
        static void AFreshOfferNeverChecksARowTheUserHadNotChecked();
        static void AFreshOfferKeepsNoCategoryWhenTheUserChoseNone();
        static void AFreshOfferKeepsTheLibraryRootWhenTheUserChoseIt();
        static void AFreshOfferWithoutTheChosenCategoryLeavesNoCategoryChosen();
        static void CheckAgainRequestedComesFromTheButtonOfTheNotice();
    };

    constexpr int kWide = 800;
    constexpr int kRowsBeforeItScrolls = 12;
    constexpr qulonglong kFiveMebibytes = 5ULL * 1024 * 1024;
    const QString kSimulator = QStringLiteral("FlightSimulator2024.exe");
    const QString kAircraft = QStringLiteral("C:/Libs/A/Aircraft");
    const QString kUtilities = QStringLiteral("C:/Libs/B/Utilities");
    const QString kRootA = QStringLiteral("C:/Libs/A");
    const QString kRootB = QStringLiteral("C:/Libs/B");

    struct Window
    {
        explicit Window(const int tall)
        {
            parent.resize(1000, tall);
        }

        QWidget parent;
    };

    [[nodiscard]] QTranslator& TheEnglishOfTheApp()
    {
        static QTranslator english;

        return english;
    }

    void ShowItSettled(QDialog& dialog)
    {
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));

        for (int round = 0; round < 3; ++round)
        {
            QCoreApplication::processEvents();
        }
    }

    [[nodiscard]] InstallStandingInALibrary Replaces(const QString& version, const QString& category)
    {
        return {.standing = InstallStanding::Replaces, .installedVersion = version, .installedIn = category};
    }

    [[nodiscard]] InstallStandingInALibrary
    TheQuarantineHoldsOne(const QString& version, const QString& category, const QString& quarantined)
    {
        return {.standing = InstallStanding::ReplacesAndTheQuarantineHoldsOne,
                .installedVersion = version,
                .installedIn = category,
                .quarantinedVersion = quarantined};
    }

    [[nodiscard]] InstallStandingInALibrary AFolderIsInTheWay()
    {
        return {.standing = InstallStanding::AFolderIsInTheWay};
    }

    [[nodiscard]] InstallAddonLine Addon(const QString& name, std::vector<InstallStandingInALibrary> standings)
    {
        return {.folderName = name,
                .version = QStringLiteral("1.0.0"),
                .bytes = kFiveMebibytes,
                .inEachLibrary = std::move(standings),
                .root = QStringLiteral("pack/") + name};
    }

    [[nodiscard]] InstallAddonLine NewAddon(const QString& name)
    {
        return Addon(name, {{}, {}});
    }

    [[nodiscard]] InstallStandingInALibrary Occupying(InstallStandingInALibrary standing, const QString& occupant)
    {
        standing.occupant = occupant;

        return standing;
    }

    [[nodiscard]] InstallAddonLine AddonByCategory(const QString& name,
                                                   std::vector<InstallStandingInALibrary> inEachLibrary,
                                                   std::vector<std::vector<InstallStandingInALibrary>> inEachCategory)
    {
        InstallAddonLine line = Addon(name, std::move(inEachLibrary));
        line.inEachCategory = std::move(inEachCategory);

        return line;
    }

    [[nodiscard]] InstallArchiveLine Archive(const QString& name, std::vector<InstallAddonLine> addons)
    {
        return {.fileName = name, .addons = std::move(addons), .path = QStringLiteral("C:/Drops/") + name};
    }

    [[nodiscard]] InstallArchiveLine Refused(const QString& name, const QString& why)
    {
        return {.fileName = name, .refusal = why, .path = QStringLiteral("C:/Drops/") + name};
    }

    [[nodiscard]] std::vector<InstallLibraryChoice> TwoLibraries()
    {
        return {{.shown = QStringLiteral("Community folder"),
                 .path = QStringLiteral("C:/Libs/A"),
                 .categories = {{.shown = QStringLiteral("Aircraft"), .path = kAircraft},
                                {.shown = QStringLiteral("Scenery · Airports"),
                                 .path = QStringLiteral("C:/Libs/A/Airports")},
                                {.shown = QStringLiteral("(library root)"), .path = kRootA}}},
                {.shown = QStringLiteral("Second library"),
                 .path = QStringLiteral("C:/Libs/B"),
                 .categories = {{.shown = QStringLiteral("Utilities"), .path = kUtilities},
                                {.shown = QStringLiteral("(library root)"), .path = kRootB}}}};
    }

    [[nodiscard]] InstallOffer OfferOf(std::vector<InstallArchiveLine> archives)
    {
        return {.archives = std::move(archives), .libraries = TwoLibraries()};
    }

    [[nodiscard]] InstallOffer ThreeNewAddons()
    {
        return OfferOf({Archive(QStringLiteral("pack.zip"),
                                {NewAddon(QStringLiteral("alpha")), NewAddon(QStringLiteral("bravo")),
                                 NewAddon(QStringLiteral("charlie"))})});
    }

    [[nodiscard]] InstallOffer ManyAddons(const int count)
    {
        std::vector<InstallAddonLine> addons;
        addons.reserve(static_cast<std::size_t>(count));

        for (int at = 0; at < count; ++at)
        {
            addons.push_back(NewAddon(QStringLiteral("addon-%1").arg(at)));
        }

        return OfferOf({Archive(QStringLiteral("many.zip"), std::move(addons))});
    }

    [[nodiscard]] QTreeView* TreeOf(const QDialog& dialog)
    {
        return dialog.findChild<QTreeView*>(QStringLiteral("InstallList"));
    }

    [[nodiscard]] QStandardItemModel* ModelOf(const QDialog& dialog)
    {
        return qobject_cast<QStandardItemModel*>(TreeOf(dialog)->model());
    }

    [[nodiscard]] QStandardItem*
    CellOf(const QDialog& dialog, const int archive, const int addon, const int column = InstallDialog::AddonColumn)
    {
        return ModelOf(dialog)->item(archive)->child(addon, column);
    }

    [[nodiscard]] QStandardItem* DiscardBoxOf(const QDialog& dialog, const int archive, const int addon)
    {
        return CellOf(dialog, archive, addon)->child(0);
    }

    [[nodiscard]] QComboBox* LibraryOf(const QDialog& dialog)
    {
        return dialog.findChild<QComboBox*>(QStringLiteral("InstallLibrary"));
    }

    [[nodiscard]] CategoryField* CategoryOf(const QDialog& dialog)
    {
        return dialog.findChild<CategoryField*>(QStringLiteral("InstallCategory"));
    }

    [[nodiscard]] QCheckBox* CheckAllOf(const QDialog& dialog)
    {
        return dialog.findChild<QCheckBox*>(QStringLiteral("CheckAll"));
    }

    [[nodiscard]] QPushButton* InstallOf(const QDialog& dialog)
    {
        return dialog.findChild<QPushButton*>(QStringLiteral("InstallChecked"));
    }

    [[nodiscard]] QLabel* LabelNamed(const QDialog& dialog, const QString& name)
    {
        return dialog.findChild<QLabel*>(name);
    }

    [[nodiscard]] QStringList Picks(const InstallDialog& dialog)
    {
        QStringList picked;

        for (const ChosenAddon& chosen : dialog.Chosen())
        {
            picked.append(QStringLiteral("%1:%2").arg(chosen.archive).arg(chosen.addon));
        }

        return picked;
    }

    [[nodiscard]] QStringList WhatIsCut(const QDialog& root)
    {
        QStringList cut;

        for (const QLabel* label : root.findChildren<QLabel*>())
        {
            if (!label->isVisible() || label->text().isEmpty())
            {
                continue;
            }

            if (label->wordWrap() && label->height() < label->heightForWidth(label->width()))
            {
                cut.append(QStringLiteral("wrapped label: ") + label->text().left(40));
            }

            if (!label->wordWrap() && label->fontMetrics().horizontalAdvance(label->text()) > label->width())
            {
                cut.append(QStringLiteral("label: ") + label->text().left(40));
            }
        }

        for (const QAbstractButton* button : root.findChildren<QAbstractButton*>())
        {
            if (button->isVisible() && !button->text().isEmpty() && button->sizeHint().width() > button->width())
            {
                cut.append(QStringLiteral("button: ") + button->text().left(40));
            }
        }

        const QTreeView* tree = TreeOf(root);
        const QAbstractItemModel* model = tree->model();

        const std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& parent)
        {
            for (int row = 0; row < model->rowCount(parent); ++row)
            {
                const bool spanned = tree->isFirstColumnSpanned(row, parent);

                for (int column = 0; column < (spanned ? 1 : model->columnCount(parent)); ++column)
                {
                    const QModelIndex index = model->index(row, column, parent);
                    const QRect cell = tree->visualRect(index);
                    const int room = spanned ? tree->viewport()->width() - cell.x() : cell.width();

                    QStyleOptionViewItem option;
                    option.initFrom(tree);
                    option.widget = tree;
                    option.rect = cell;

                    const int asks = tree->itemDelegate()->sizeHint(option, index).width();

                    if (asks > room + 6)
                    {
                        cut.append(QStringLiteral("cell: %1 asks %2 has %3")
                                       .arg(index.data().toString().left(40))
                                       .arg(asks)
                                       .arg(room));
                    }
                }

                walk(model->index(row, 0, parent));
            }
        };

        walk({});

        return cut;
    }

    [[nodiscard]] QString TheLongestRefusal(const bool english)
    {
        return english ? QStringLiteral("Windows could not open it: it is damaged, or this Windows does not read it. "
                                        "Windows 10 does not read .7z or older .rar.")
                       : QStringLiteral("O Windows não abriu: está danificado, ou este Windows não lê o formato. O "
                                        "Windows 10 não lê .7z nem .rar antigo.");
    }

    [[nodiscard]] InstallOffer TheOfferThatSaysEverything(const bool english, const bool scrolls)
    {
        const QString longCategory = QStringLiteral("Scenery · Europe");
        std::vector<InstallAddonLine> many;

        for (int at = 0; scrolls && at < 14; ++at)
        {
            many.push_back(Addon(QStringLiteral("navigraph-navdata-cycle-%1").arg(at),
                                 {Replaces(QStringLiteral("2410.1"), longCategory), {}}));
        }

        const InstallStandingInALibrary holdingOne =
            TheQuarantineHoldsOne(QStringLiteral("2.3.0"), longCategory, QStringLiteral("2.3.0-b7"));
        const InstallStandingInALibrary holdingOneUnversioned =
            TheQuarantineHoldsOne(QString(), longCategory, QString());
        const InstallStandingInALibrary inTheWay = AFolderIsInTheWay();

        InstallOffer offer = OfferOf(
            {Archive(QStringLiteral("flytampa-amsterdam-schiphol-airport-and-surroundings-v2.4.1-final.zip"),
                     {Addon(QStringLiteral("flytampa-airport-eham"), {holdingOne, inTheWay}),
                      Addon(QStringLiteral("asobo-landmarks"), {Replaces(QStringLiteral("1.1"), longCategory), {}}),
                      Addon(QStringLiteral("a-new-one"), {{}, holdingOneUnversioned})}),
             Refused(QStringLiteral("broken-with-a-long-name-so-it-wraps-nowhere.7z"), TheLongestRefusal(english)),
             Archive(QStringLiteral("navigraph.zip"), std::move(many))});

        if (!scrolls)
        {
            offer.archives.pop_back();
        }

        offer.libraries.front().categories.push_back({.shown = longCategory, .path = QStringLiteral("C:/Libs/A/Long")});

        return offer;
    }
}

void InstallDialogTest::initTestCase()
{
    ApplyModernistTheme(*qApp);

    QVERIFY2(LoadedTheShippedCatalogue(TheEnglishOfTheApp(), QStringLiteral("en")),
             "app_en.qm is not beside the build: build the release_translations target");
    QCoreApplication::installTranslator(&TheEnglishOfTheApp());
}

void InstallDialogTest::TheDialogIsEightHundredPixelsWide()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    QCOMPARE(dialog.width(), kWide);

    dialog.TakeAFreshOffer(ThreeNewAddons());

    QCOMPARE(dialog.width(), kWide);
}

void InstallDialogTest::TheDialogIsNeverTallerThanTheWindowItOpensOver()
{
    Window window(400);
    InstallDialog dialog(ManyAddons(25), &window.parent);
    ShowItSettled(dialog);

    QVERIFY2(dialog.height() <= window.parent.height(),
             qPrintable(QStringLiteral("the dialog is %1 px tall over a window of %2")
                            .arg(dialog.height())
                            .arg(window.parent.height())));
}

void InstallDialogTest::TheListStopsAtTwelveRowsAndScrollsBeyondThem()
{
    Window window(2400);

    InstallDialog twelve(ManyAddons(kRowsBeforeItScrolls - 1), &window.parent);
    ShowItSettled(twelve);

    QVERIFY2(!TreeOf(twelve)->verticalScrollBar()->isVisible(), "an archive row and eleven addons are twelve rows");

    InstallDialog thirteen(ManyAddons(kRowsBeforeItScrolls), &window.parent);
    ShowItSettled(thirteen);

    const QTreeView* tree = TreeOf(thirteen);
    const int oneRow = tree->visualRect(ModelOf(thirteen)->index(0, 0, ModelOf(thirteen)->index(0, 0))).height();

    QVERIFY2(tree->verticalScrollBar()->isVisible(), "thirteen rows scroll");
    QCOMPARE(tree->height(), tree->header()->height() + kRowsBeforeItScrolls * oneRow + 2 * tree->frameWidth());

    InstallDialog few(ManyAddons(3), &window.parent);
    ShowItSettled(few);

    QVERIFY(!TreeOf(few)->verticalScrollBar()->isVisible());
    QVERIFY(TreeOf(few)->height() < tree->height());
}

void InstallDialogTest::NoCellAndNoSentenceRowIsCutInEitherLanguage_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<bool>("scrolls");

    QTest::newRow("English, a list that fits") << QStringLiteral("en") << false;
    QTest::newRow("English, a list that scrolls") << QStringLiteral("en") << true;
    QTest::newRow("Brazilian Portuguese, a list that fits") << QStringLiteral("pt_BR") << false;
    QTest::newRow("Brazilian Portuguese, a list that scrolls") << QStringLiteral("pt_BR") << true;
}

void InstallDialogTest::NoCellAndNoSentenceRowIsCutInEitherLanguage()
{
    QFETCH(QString, language);
    QFETCH(bool, scrolls);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Window window(2400);
    InstallDialog dialog(TheOfferThatSaysEverything(language == QLatin1String("en"), scrolls), &window.parent);
    ShowItSettled(dialog);

    QCOMPARE(TreeOf(dialog)->verticalScrollBar()->isVisible(), scrolls);

    QCOMPARE(WhatIsCut(dialog).join(QLatin1Char('\n')), QString());

    LibraryOf(dialog)->setCurrentIndex(1);
    QCoreApplication::processEvents();

    QCOMPARE(WhatIsCut(dialog).join(QLatin1Char('\n')), QString());

    LibraryOf(dialog)->setCurrentIndex(0);
    CategoryOf(dialog)->setCurrentIndex(CategoryOf(dialog)->count() - 1);
    QCoreApplication::processEvents();

    QCOMPARE(WhatIsCut(dialog).join(QLatin1Char('\n')), QString());

    InstallOffer noisy = TheOfferThatSaysEverything(language == QLatin1String("en"), scrolls);
    noisy.runningSimulator = kSimulator;
    dialog.TakeAFreshOffer(noisy);
    QCoreApplication::processEvents();

    QCOMPARE(WhatIsCut(dialog).join(QLatin1Char('\n')), QString());
}

void InstallDialogTest::InstallNeedsACategoryWhileANewAddonIsChecked()
{
    Window window(2400);
    InstallDialog dialog(OfferOf({Archive(QStringLiteral("one.zip"), {NewAddon(QStringLiteral("alpha"))})}),
                         &window.parent);
    ShowItSettled(dialog);

    QVERIFY2(!InstallOf(dialog)->isEnabled(), "a new addon is checked and no category is chosen");
    QVERIFY(LabelNamed(dialog, QStringLiteral("InstallHint"))->isVisible());

    CategoryOf(dialog)->setCurrentIndex(0);

    QVERIFY(InstallOf(dialog)->isEnabled());
    QVERIFY(!LabelNamed(dialog, QStringLiteral("InstallHint"))->isVisible());
}

void InstallDialogTest::InstallIsDisabledWithNothingChecked()
{
    Window window(2400);
    InstallOffer offer = OfferOf({Archive(QStringLiteral("one.zip"), {NewAddon(QStringLiteral("alpha"))})});
    offer.droppedOn = kAircraft;
    InstallDialog dialog(offer, &window.parent);
    ShowItSettled(dialog);

    QVERIFY(InstallOf(dialog)->isEnabled());

    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);

    QVERIFY2(!InstallOf(dialog)->isEnabled(), "nothing is checked");

    CellOf(dialog, 0, 0)->setCheckState(Qt::Checked);

    QVERIFY(InstallOf(dialog)->isEnabled());
}

void InstallDialogTest::InstallIsDisabledWhileTheSimulatorNoticeShows()
{
    Window window(2400);
    InstallOffer offer = OfferOf({Archive(QStringLiteral("one.zip"), {NewAddon(QStringLiteral("alpha"))})});
    offer.droppedOn = kAircraft;
    InstallDialog dialog(offer, &window.parent);
    ShowItSettled(dialog);

    QVERIFY(InstallOf(dialog)->isEnabled());

    offer.runningSimulator = kSimulator;
    dialog.TakeAFreshOffer(offer);

    QVERIFY(LabelNamed(dialog, QStringLiteral("InstallNotice"))->isVisible());
    QVERIFY2(!InstallOf(dialog)->isEnabled(), "the simulator notice is showing");

    offer.runningSimulator.clear();
    dialog.TakeAFreshOffer(offer);

    QVERIFY(!LabelNamed(dialog, QStringLiteral("InstallNotice"))->isVisible());
    QVERIFY(InstallOf(dialog)->isEnabled());
}

void InstallDialogTest::TheInstallLabelCarriesTheCountInEitherLanguage_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("checked");
    QTest::addColumn<QString>("label");

    QTest::newRow("en, none") << "en" << 0 << "Install";
    QTest::newRow("en, one") << "en" << 1 << "Install 1 addon";
    QTest::newRow("en, three") << "en" << 3 << "Install 3 addons";
    QTest::newRow("pt_BR, none") << "pt_BR" << 0 << "Instalar";
    QTest::newRow("pt_BR, one") << "pt_BR" << 1 << "Instalar 1 addon";
    QTest::newRow("pt_BR, three") << "pt_BR" << 3 << "Instalar 3 addons";
}

void InstallDialogTest::TheInstallLabelCarriesTheCountInEitherLanguage()
{
    QFETCH(QString, language);
    QFETCH(int, checked);
    QFETCH(QString, label);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    for (int at = checked; at < 3; ++at)
    {
        CellOf(dialog, 0, at)->setCheckState(Qt::Unchecked);
    }

    QCOMPARE(InstallOf(dialog)->text(), label);
}

void InstallDialogTest::TheTotalSaysHowManyAreCheckedAndHowBig()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    const QLabel* total = LabelNamed(dialog, QStringLiteral("InstallTotal"));

    QCOMPARE(total->text(), QStringLiteral("3 addons checked · %1").arg(AsSize(3 * kFiveMebibytes)));

    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);
    CellOf(dialog, 0, 1)->setCheckState(Qt::Unchecked);

    QCOMPARE(total->text(), QStringLiteral("1 addon checked · %1").arg(AsSize(kFiveMebibytes)));

    CellOf(dialog, 0, 2)->setCheckState(Qt::Unchecked);

    QCOMPARE(total->text(), QStringLiteral("No addon checked"));
}

void InstallDialogTest::CheckAllIsTriStateAndFollowsWhatIsChecked()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    QCheckBox* all = CheckAllOf(dialog);

    QVERIFY(all->isTristate());
    QVERIFY(all->isVisible());
    QCOMPARE(all->checkState(), Qt::Checked);

    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);

    QCOMPARE(all->checkState(), Qt::PartiallyChecked);

    all->click();

    QCOMPARE(all->checkState(), Qt::Checked);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1"), QStringLiteral("0:2")}));

    all->click();

    QCOMPARE(all->checkState(), Qt::Unchecked);
    QCOMPARE(Picks(dialog), QStringList());

    all->click();

    QCOMPARE(all->checkState(), Qt::Checked);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1"), QStringLiteral("0:2")}));
}

void InstallDialogTest::CheckAllNeverChecksARowWaitingForTheDiscard()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {NewAddon(QStringLiteral("alpha")), NewAddon(QStringLiteral("bravo")),
             Addon(QStringLiteral("charlie"),
                   {TheQuarantineHoldsOne(QStringLiteral("1.0"), QStringLiteral("Aircraft"), QStringLiteral("0.9")),
                    {}})})}),
        &window.parent);
    ShowItSettled(dialog);

    QCheckBox* all = CheckAllOf(dialog);

    QCOMPARE(all->checkState(), Qt::Checked);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1")}));

    all->click();
    all->click();

    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1")}));
    QCOMPARE(CellOf(dialog, 0, 2)->checkState(), Qt::Unchecked);
    QVERIFY(DiscardBoxOf(dialog, 0, 2)->isCheckable());
    QCOMPARE(DiscardBoxOf(dialog, 0, 2)->checkState(), Qt::Unchecked);
}

void InstallDialogTest::TheDiscardBoxAndItsAddonMoveTogetherInBothDirections()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"),
                   {TheQuarantineHoldsOne(QStringLiteral("1.0"), QStringLiteral("Aircraft"), QStringLiteral("0.9")),
                    {}})})}),
        &window.parent);
    ShowItSettled(dialog);

    QStandardItem* addon = CellOf(dialog, 0, 0);
    QStandardItem* discard = DiscardBoxOf(dialog, 0, 0);

    QVERIFY(discard->isCheckable());
    QCOMPARE(addon->checkState(), Qt::Unchecked);
    QVERIFY(!addon->isEnabled());
    QCOMPARE(Picks(dialog), QStringList());

    discard->setCheckState(Qt::Checked);

    QCOMPARE(addon->checkState(), Qt::Checked);
    QVERIFY(addon->isEnabled());

    const std::vector<ChosenAddon> chosen = dialog.Chosen();

    QCOMPARE(chosen.size(), std::size_t{1});
    QVERIFY(chosen.front().discardsTheQuarantinedCopy);

    addon->setCheckState(Qt::Unchecked);

    QCOMPARE(discard->checkState(), Qt::Unchecked);
    QVERIFY(!addon->isEnabled());
    QCOMPARE(Picks(dialog), QStringList());

    discard->setCheckState(Qt::Checked);
    discard->setCheckState(Qt::Unchecked);

    QCOMPARE(addon->checkState(), Qt::Unchecked);
    QCOMPARE(Picks(dialog), QStringList());
}

void InstallDialogTest::ChangingTheLibraryChangesTheInstalledColumnTheCategoryColumnAndTheSentenceRows()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"), {{}, Replaces(QStringLiteral("0.8"), QStringLiteral("Scenery · Old"))}),
             Addon(QStringLiteral("bravo"), {{}, AFolderIsInTheWay()}),
             Addon(
                 QStringLiteral("charlie"),
                 {{},
                  TheQuarantineHoldsOne(QStringLiteral("0.5"), QStringLiteral("Aircraft"), QStringLiteral("0.4"))})})}),
        &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(0);

    const QLabel* promise = LabelNamed(dialog, QStringLiteral("PanelPromise"));

    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::InstalledColumn)->text(), QString());
    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::CategoryColumn)->text(), QStringLiteral("Aircraft"));
    QCOMPARE(CellOf(dialog, 0, 1)->rowCount(), 0);
    QCOMPARE(CellOf(dialog, 0, 2)->rowCount(), 0);
    QVERIFY(!promise->isVisible());

    LibraryOf(dialog)->setCurrentIndex(1);

    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::InstalledColumn)->text(), QStringLiteral("0.8"));
    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::CategoryColumn)->text(), QStringLiteral("Scenery · Old"));
    QCOMPARE(CellOf(dialog, 0, 1)->rowCount(), 1);
    QVERIFY(CellOf(dialog, 0, 1)->child(0)->data(QuietRole).toBool());
    QVERIFY(CellOf(dialog, 0, 1)->child(0)->text().contains(QStringLiteral("manifest.json")));
    QCOMPARE(CellOf(dialog, 0, 2)->rowCount(), 1);
    QVERIFY(CellOf(dialog, 0, 2)->child(0)->isCheckable());
    QVERIFY(CellOf(dialog, 0, 2)->child(0)->text().contains(QStringLiteral("0.4")));
    QVERIFY(promise->isVisible());
    QVERIFY(promise->text().contains(QStringLiteral("quarantine")));

    LibraryOf(dialog)->setCurrentIndex(0);

    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::InstalledColumn)->text(), QString());
    QCOMPARE(CellOf(dialog, 0, 1)->rowCount(), 0);
    QCOMPARE(CellOf(dialog, 0, 2)->rowCount(), 0);
    QVERIFY(!promise->isVisible());
}

void InstallDialogTest::ChangingTheCategoryMovesTheFolderInTheWayToTheCategoryThatHasIt()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(QStringLiteral("pack.zip"),
                         {AddonByCategory(QStringLiteral("alpha"), {{}, {}}, {{{}, AFolderIsInTheWay()}, {{}}}),
                          NewAddon(QStringLiteral("bravo"))})}),
        &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(0);

    QCOMPARE(CellOf(dialog, 0, 0)->rowCount(), 0);
    QVERIFY(CellOf(dialog, 0, 0)->isEnabled());
    QCOMPARE(CellOf(dialog, 0, 0)->checkState(), Qt::Checked);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1")}));

    CategoryOf(dialog)->setCurrentIndex(1);

    QCOMPARE(CellOf(dialog, 0, 0)->rowCount(), 1);
    QVERIFY(CellOf(dialog, 0, 0)->child(0)->data(QuietRole).toBool());
    QVERIFY(CellOf(dialog, 0, 0)->child(0)->text().contains(QStringLiteral("manifest.json")));
    QVERIFY(!CellOf(dialog, 0, 0)->isEnabled());
    QCOMPARE(CellOf(dialog, 0, 0)->checkState(), Qt::Unchecked);
    QCOMPARE(CellOf(dialog, 0, 1)->rowCount(), 0);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:1")}));

    CategoryOf(dialog)->setCurrentIndex(0);

    QCOMPARE(CellOf(dialog, 0, 0)->rowCount(), 0);
    QVERIFY(CellOf(dialog, 0, 0)->isEnabled());
    QCOMPARE(CellOf(dialog, 0, 0)->checkState(), Qt::Checked);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1")}));
}

void InstallDialogTest::TheDiscardTheUserTickedSurvivesAChangeOfCategoryButNotOfLibrary()
{
    const InstallStandingInALibrary held =
        TheQuarantineHoldsOne(QStringLiteral("0.5"), QStringLiteral("Aircraft"), QStringLiteral("0.4"));
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(QStringLiteral("pack.zip"),
                         {AddonByCategory(QStringLiteral("alpha"), {held, {}}, {{held, held}, {{}}})})}),
        &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(0);

    QVERIFY(!CellOf(dialog, 0, 0)->isEnabled());

    DiscardBoxOf(dialog, 0, 0)->setCheckState(Qt::Checked);

    QVERIFY(CellOf(dialog, 0, 0)->isEnabled());
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0")}));

    CategoryOf(dialog)->setCurrentIndex(1);

    QCOMPARE(DiscardBoxOf(dialog, 0, 0)->checkState(), Qt::Checked);
    QVERIFY(CellOf(dialog, 0, 0)->isEnabled());
    QCOMPARE(CellOf(dialog, 0, 0)->checkState(), Qt::Checked);
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0")}));
    QVERIFY(dialog.Chosen().front().discardsTheQuarantinedCopy);

    LibraryOf(dialog)->setCurrentIndex(1);
    LibraryOf(dialog)->setCurrentIndex(0);
    CategoryOf(dialog)->setCurrentIndex(0);

    QCOMPARE(DiscardBoxOf(dialog, 0, 0)->checkState(), Qt::Unchecked);
    QVERIFY(!CellOf(dialog, 0, 0)->isEnabled());
    QVERIFY(dialog.Chosen().empty());
}

void InstallDialogTest::ChosenCarriesTheOccupantOfTheCopyARowReplaces()
{
    const QString occupant = QStringLiteral("C:/Libs/A/Old/alpha");
    const InstallStandingInALibrary replaces =
        Occupying(Replaces(QStringLiteral("0.8"), QStringLiteral("Old")), occupant);
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(QStringLiteral("pack.zip"),
                         {AddonByCategory(QStringLiteral("alpha"), {replaces, {}}, {{replaces, replaces}, {{}}}),
                          NewAddon(QStringLiteral("bravo"))})}),
        &window.parent);
    ShowItSettled(dialog);

    const std::vector<ChosenAddon> withoutACategory = dialog.Chosen();

    QCOMPARE(withoutACategory.size(), std::size_t{2});
    QCOMPARE(withoutACategory[0].replaces, occupant);
    QVERIFY(withoutACategory[1].replaces.isEmpty());

    CategoryOf(dialog)->setCurrentIndex(1);

    QCOMPARE(dialog.Chosen()[0].replaces, occupant);
    QVERIFY(dialog.Chosen()[1].replaces.isEmpty());

    LibraryOf(dialog)->setCurrentIndex(1);

    QVERIFY(dialog.Chosen()[0].replaces.isEmpty());
}

void InstallDialogTest::ARowThatReplacesShowsTheCategoryOfTheInstalledCopyAndNotTheChosenOne()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"), {Replaces(QStringLiteral("0.8"), QStringLiteral("Scenery · Old")), {}}),
             Addon(QStringLiteral("bravo"), {{}, {}})})}),
        &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(1);

    QCOMPARE(CategoryOf(dialog)->currentText(), QStringLiteral("Scenery · Airports"));
    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::CategoryColumn)->text(), QStringLiteral("Scenery · Old"));
    QCOMPARE(CellOf(dialog, 0, 1, InstallDialog::CategoryColumn)->text(), QStringLiteral("Scenery · Airports"));
}

void InstallDialogTest::TheCategoryComboIsDisabledWhenNoCheckedAddonIsNew()
{
    Window window(2400);
    InstallDialog dialog(OfferOf({Archive(QStringLiteral("pack.zip"),
                                          {Addon(QStringLiteral("alpha"),
                                                 {{}, Replaces(QStringLiteral("0.8"), QStringLiteral("Aircraft"))})})}),
                         &window.parent);
    ShowItSettled(dialog);

    QVERIFY(CategoryOf(dialog)->isEnabled());

    LibraryOf(dialog)->setCurrentIndex(1);

    QVERIFY2(!CategoryOf(dialog)->isEnabled(), "the only addon replaces an installed copy");
    QVERIFY2(InstallOf(dialog)->isEnabled(), "a replacement needs no category");

    LibraryOf(dialog)->setCurrentIndex(0);
    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);

    QVERIFY2(!CategoryOf(dialog)->isEnabled(), "the new addon is not checked");

    CellOf(dialog, 0, 0)->setCheckState(Qt::Checked);

    QVERIFY(CategoryOf(dialog)->isEnabled());
}

void InstallDialogTest::AnOfferOfReplacementsAloneIsSentWithTheRootOfTheChosenLibraryWhenNoCategoryIsChosen()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(QStringLiteral("pack.zip"),
                         {Addon(QStringLiteral("alpha"),
                                {Occupying(Replaces(QStringLiteral("0.8"), QStringLiteral("Aircraft")),
                                           QStringLiteral("C:/Libs/A/Aircraft/alpha")),
                                 Occupying(Replaces(QStringLiteral("0.9"), QStringLiteral("Utilities")),
                                           QStringLiteral("C:/Libs/B/Utilities/alpha"))})})}),
        &window.parent);
    ShowItSettled(dialog);

    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
    QVERIFY(!CategoryOf(dialog)->isEnabled());
    QVERIFY(InstallOf(dialog)->isEnabled());
    QCOMPARE(dialog.ChosenCategory(), QStringLiteral("C:/Libs/A"));

    LibraryOf(dialog)->setCurrentIndex(1);

    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
    QVERIFY(InstallOf(dialog)->isEnabled());
    QCOMPARE(dialog.ChosenCategory(), QStringLiteral("C:/Libs/B"));
}

void InstallDialogTest::ACategoryTheUserChoseIsSentInsteadOfTheRootOfTheLibrary()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    LibraryOf(dialog)->setCurrentIndex(1);
    CategoryOf(dialog)->setCurrentIndex(0);

    QCOMPARE(dialog.ChosenCategory(), kUtilities);
}

void InstallDialogTest::TheDisabledCategoryFieldNamesTheCategoryOfTheOnlyReplacedAddon()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"), {Replaces(QStringLiteral("0.8"), QStringLiteral("Liveries")), {}}),
             Addon(QStringLiteral("bravo"), {Replaces(QStringLiteral("0.9"), QStringLiteral("Liveries")), {}})})}),
        &window.parent);
    ShowItSettled(dialog);

    QVERIFY(!CategoryOf(dialog)->isEnabled());
    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
    QCOMPARE(CategoryOf(dialog)->Shown(), QStringLiteral("Liveries"));

    CellOf(dialog, 0, 1)->setCheckState(Qt::Unchecked);

    QCOMPARE(CategoryOf(dialog)->Shown(), QStringLiteral("Liveries"));
    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
}

void InstallDialogTest::TheDisabledCategoryFieldSaysEachAddonStaysWhenTheReplacedOnesAreInDifferentCategories()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"), {Replaces(QStringLiteral("0.8"), QStringLiteral("Liveries")), {}}),
             Addon(QStringLiteral("bravo"), {Replaces(QStringLiteral("0.9"), QStringLiteral("Aircraft")), {}})})}),
        &window.parent);
    ShowItSettled(dialog);

    const QString each = QCoreApplication::translate("InstallDialog", "Each addon stays where it is");

    QVERIFY(!CategoryOf(dialog)->isEnabled());
    QCOMPARE(CategoryOf(dialog)->Shown(), each);
    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);

    CellOf(dialog, 0, 1)->setCheckState(Qt::Unchecked);

    QCOMPARE(CategoryOf(dialog)->Shown(), QStringLiteral("Liveries"));

    CellOf(dialog, 0, 1)->setCheckState(Qt::Checked);

    QCOMPARE(CategoryOf(dialog)->Shown(), each);
}

void InstallDialogTest::TheCategoryTheUserChoseComesBackWhenANewAddonIsCheckedAgain()
{
    Window window(2400);
    InstallDialog dialog(OfferOf({Archive(QStringLiteral("pack.zip"),
                                          {NewAddon(QStringLiteral("alpha")),
                                           Addon(QStringLiteral("bravo"),
                                                 {Replaces(QStringLiteral("0.9"), QStringLiteral("Liveries")), {}})})}),
                         &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(1);
    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);

    QVERIFY(!CategoryOf(dialog)->isEnabled());
    QCOMPARE(CategoryOf(dialog)->Shown(), QStringLiteral("Liveries"));
    QCOMPARE(CategoryOf(dialog)->currentIndex(), 1);

    CellOf(dialog, 0, 0)->setCheckState(Qt::Checked);

    QVERIFY(CategoryOf(dialog)->isEnabled());
    QCOMPARE(CategoryOf(dialog)->currentIndex(), 1);
    QCOMPARE(CategoryOf(dialog)->Shown(), CategoryOf(dialog)->currentText());
    QCOMPARE(dialog.ChosenCategory(), QStringLiteral("C:/Libs/A/Airports"));
}

void InstallDialogTest::TheParagraphCountsTheAddonsThatAreAlreadyInstalled()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"),
                   {Replaces(QStringLiteral("0.8"), QStringLiteral("Aircraft")),
                    Replaces(QStringLiteral("0.8"), QStringLiteral("Utilities"))}),
             Addon(QStringLiteral("bravo"), {{}, Replaces(QStringLiteral("0.9"), QStringLiteral("Utilities"))})})}),
        &window.parent);
    ShowItSettled(dialog);

    const QLabel* promise = LabelNamed(dialog, QStringLiteral("PanelPromise"));

    QVERIFY(promise->isVisible());
    QVERIFY2(promise->text().startsWith(QStringLiteral("1 addon in this list is already installed")),
             qPrintable(promise->text()));

    LibraryOf(dialog)->setCurrentIndex(1);

    QVERIFY(promise->isVisible());
    QVERIFY2(promise->text().startsWith(QStringLiteral("2 addons in this list are already installed")),
             qPrintable(promise->text()));
    QVERIFY(promise->text().contains(QStringLiteral("quarantine")));
}

void InstallDialogTest::TheCategoryFieldPaintsWhatItWasToldToSayInstead()
{
    Window window(2400);
    InstallDialog dialog(OfferOf({Archive(QStringLiteral("pack.zip"),
                                          {Addon(QStringLiteral("alpha"),
                                                 {Replaces(QStringLiteral("0.8"), QStringLiteral("Liveries")), {}})})}),
                         &window.parent);
    ShowItSettled(dialog);

    CategoryField* field = CategoryOf(dialog);

    field->SayInstead({});
    const QImage plain = field->grab().toImage();

    field->SayInstead(QStringLiteral("Liveries"));
    const QImage said = field->grab().toImage();

    QVERIFY2(plain != said, "the field painted the same pixels with and without the text it was told to say");

    field->SayInstead({});

    QCOMPARE(field->grab().toImage(), plain);
}

void InstallDialogTest::EnableAfterInstallingStartsUnchecked()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    auto* enable = dialog.findChild<QCheckBox*>(QStringLiteral("EnableAfterInstalling"));

    QVERIFY(enable != nullptr);
    QVERIFY(!enable->isChecked());
    QVERIFY(!dialog.EnablesAfterInstalling());

    enable->click();

    QVERIFY(dialog.EnablesAfterInstalling());

    dialog.TakeAFreshOffer(ThreeNewAddons());

    QVERIFY2(dialog.EnablesAfterInstalling(), "a fresh offer does not undo what the user chose");
}

void InstallDialogTest::DroppedOnACategoryThatCategoryIsPreselected()
{
    Window window(2400);
    InstallOffer offer = ThreeNewAddons();
    offer.droppedOn = kUtilities;

    InstallDialog dialog(offer, &window.parent);
    ShowItSettled(dialog);

    QCOMPARE(LibraryOf(dialog)->currentIndex(), 1);
    QCOMPARE(CategoryOf(dialog)->currentIndex(), 0);
    QCOMPARE(dialog.ChosenCategory(), kUtilities);
    QVERIFY(InstallOf(dialog)->isEnabled());
    QVERIFY(!LabelNamed(dialog, QStringLiteral("InstallHint"))->isVisible());

    InstallDialog elsewhere(ThreeNewAddons(), &window.parent);
    ShowItSettled(elsewhere);

    QCOMPARE(CategoryOf(elsewhere)->currentIndex(), -1);
    QCOMPARE(elsewhere.ChosenCategory(), QStringLiteral("C:/Libs/A"));
}

void InstallDialogTest::ChosenReturnsTheArchiveTheAddonAndTheDiscardOfEachCheckedAddon()
{
    Window window(2400);
    InstallOffer offer = OfferOf(
        {Archive(QStringLiteral("first.zip"), {NewAddon(QStringLiteral("alpha")), NewAddon(QStringLiteral("bravo"))}),
         Refused(QStringLiteral("broken.zip"), QStringLiteral("It needs a password.")),
         Archive(
             QStringLiteral("second.zip"),
             {Addon(QStringLiteral("charlie"),
                    {TheQuarantineHoldsOne(QStringLiteral("1.0"), QStringLiteral("Aircraft"), QStringLiteral("0.9")),
                     {}})})});
    InstallDialog dialog(offer, &window.parent);
    ShowItSettled(dialog);

    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);
    DiscardBoxOf(dialog, 2, 0)->setCheckState(Qt::Checked);

    const std::vector<ChosenAddon> chosen = dialog.Chosen();

    QCOMPARE(chosen.size(), std::size_t{2});

    QCOMPARE(chosen.at(0).archive, 0);
    QCOMPARE(chosen.at(0).addon, 1);
    QVERIFY(!chosen.at(0).discardsTheQuarantinedCopy);

    QCOMPARE(chosen.at(1).archive, 2);
    QCOMPARE(chosen.at(1).addon, 0);
    QVERIFY(chosen.at(1).discardsTheQuarantinedCopy);
}

void InstallDialogTest::ChosenCarriesEachFieldTheFlowNeedsFromTheLineItCameFrom_data()
{
    QTest::addColumn<int>("which");
    QTest::addColumn<QString>("first");
    QTest::addColumn<QString>("second");

    QTest::newRow("archive path") << 0 << "C:/Drops/first.zip" << "C:/Drops/second.zip";
    QTest::newRow("root") << 1 << "pack/bravo" << "pack/charlie";
    QTest::newRow("folder name") << 2 << "bravo" << "charlie";
}

void InstallDialogTest::ChosenCarriesEachFieldTheFlowNeedsFromTheLineItCameFrom()
{
    QFETCH(int, which);
    QFETCH(QString, first);
    QFETCH(QString, second);

    Window window(2400);
    InstallDialog dialog(OfferOf({Archive(QStringLiteral("first.zip"),
                                          {NewAddon(QStringLiteral("alpha")), NewAddon(QStringLiteral("bravo"))}),
                                  Refused(QStringLiteral("broken.zip"), QStringLiteral("It needs a password.")),
                                  Archive(QStringLiteral("second.zip"), {NewAddon(QStringLiteral("charlie"))})}),
                         &window.parent);
    ShowItSettled(dialog);

    CellOf(dialog, 0, 0)->setCheckState(Qt::Unchecked);

    const std::vector<ChosenAddon> chosen = dialog.Chosen();

    QCOMPARE(chosen.size(), std::size_t{2});

    const auto fieldOf = [which](const ChosenAddon& one)
    {
        switch (which)
        {
        case 0: return one.archivePath;
        case 1: return one.root;
        default: return one.folderName;
        }
    };

    QCOMPARE(fieldOf(chosen.at(0)), first);
    QCOMPARE(fieldOf(chosen.at(1)), second);
}

void InstallDialogTest::ChosenCarriesWhetherTheSourceWasAFolder()
{
    Window window(2400);
    InstallArchiveLine folder = Archive(QStringLiteral("Meu Addon"), {NewAddon(QStringLiteral("Meu Addon"))});
    folder.source = SourceKind::Folder;
    InstallDialog dialog(
        OfferOf({Archive(QStringLiteral("first.zip"), {NewAddon(QStringLiteral("alpha"))}), std::move(folder)}),
        &window.parent);
    ShowItSettled(dialog);

    const std::vector<ChosenAddon> chosen = dialog.Chosen();

    QCOMPARE(chosen.size(), std::size_t{2});
    QCOMPARE(chosen.at(0).source, SourceKind::Archive);
    QCOMPARE(chosen.at(1).source, SourceKind::Folder);
}

void InstallDialogTest::AFreshOfferKeepsTheChecksAndUpdatesTheStandingsAndTheNotice()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(1);
    CellOf(dialog, 0, 1)->setCheckState(Qt::Unchecked);

    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:2")}));

    InstallOffer fresh = OfferOf({Archive(
        QStringLiteral("pack.zip"),
        {NewAddon(QStringLiteral("alpha")), NewAddon(QStringLiteral("bravo")),
         Addon(QStringLiteral("charlie"), {Replaces(QStringLiteral("3.0"), QStringLiteral("Scenery · Old")), {}})})});
    fresh.runningSimulator = kSimulator;
    dialog.TakeAFreshOffer(fresh);

    const QLabel* notice = LabelNamed(dialog, QStringLiteral("InstallNotice"));

    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:2")}));
    QCOMPARE(CellOf(dialog, 0, 2, InstallDialog::InstalledColumn)->text(), QStringLiteral("3.0"));
    QCOMPARE(CellOf(dialog, 0, 2, InstallDialog::CategoryColumn)->text(), QStringLiteral("Scenery · Old"));
    QCOMPARE(CellOf(dialog, 0, 0, InstallDialog::CategoryColumn)->text(), QStringLiteral("Scenery · Airports"));
    QVERIFY(notice->isVisible());
    QVERIFY(notice->text().contains(kSimulator));
    QVERIFY(!InstallOf(dialog)->isEnabled());

    fresh.runningSimulator.clear();
    dialog.TakeAFreshOffer(fresh);

    QVERIFY(!notice->isVisible());
    QVERIFY(InstallOf(dialog)->isEnabled());
    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:2")}));
}

void InstallDialogTest::AFreshOfferKeepsTheLibraryAndTheCategoryTheUserChose()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    LibraryOf(dialog)->setCurrentIndex(1);
    CategoryOf(dialog)->setCurrentIndex(0);

    InstallOffer fresh = ThreeNewAddons();
    fresh.droppedOn = kAircraft;
    fresh.runningSimulator = kSimulator;
    dialog.TakeAFreshOffer(fresh);

    QCOMPARE(LibraryOf(dialog)->currentIndex(), 1);
    QCOMPARE(dialog.ChosenCategory(), kUtilities);

    LibraryOf(dialog)->setCurrentIndex(0);
    CategoryOf(dialog)->setCurrentIndex(-1);
    dialog.TakeAFreshOffer(ThreeNewAddons());

    QCOMPARE(LibraryOf(dialog)->currentIndex(), 0);
    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
}

void InstallDialogTest::AFreshOfferKeepsNoCategoryWhenTheUserChoseNone()
{
    Window window(2400);
    InstallOffer first = ThreeNewAddons();
    first.runningSimulator = kSimulator;

    InstallDialog dialog(first, &window.parent);
    ShowItSettled(dialog);

    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);

    dialog.TakeAFreshOffer(ThreeNewAddons());

    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
    QVERIFY2(!InstallOf(dialog)->isEnabled(), "a new addon is checked and the user chose no category");
    QVERIFY(LabelNamed(dialog, QStringLiteral("InstallHint"))->isVisible());
}

void InstallDialogTest::AFreshOfferKeepsTheLibraryRootWhenTheUserChoseIt()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    const int root = CategoryOf(dialog)->findData(kRootA);

    QVERIFY(root >= 0);

    CategoryOf(dialog)->setCurrentIndex(root);
    dialog.TakeAFreshOffer(ThreeNewAddons());

    QCOMPARE(CategoryOf(dialog)->currentIndex(), root);
    QCOMPARE(dialog.ChosenCategory(), kRootA);
    QVERIFY(InstallOf(dialog)->isEnabled());
}

void InstallDialogTest::AFreshOfferWithoutTheChosenCategoryLeavesNoCategoryChosen()
{
    Window window(2400);
    InstallDialog dialog(ThreeNewAddons(), &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(0);

    QVERIFY(InstallOf(dialog)->isEnabled());

    InstallOffer fresh = ThreeNewAddons();
    fresh.libraries.front().categories.erase(fresh.libraries.front().categories.begin());
    dialog.TakeAFreshOffer(fresh);

    QCOMPARE(CategoryOf(dialog)->currentIndex(), -1);
    QVERIFY2(!InstallOf(dialog)->isEnabled(),
             "the category the user chose is gone and the library root was not chosen");
}

void InstallDialogTest::AFreshOfferNeverChecksARowTheUserHadNotChecked()
{
    Window window(2400);
    InstallDialog dialog(
        OfferOf({Archive(
            QStringLiteral("pack.zip"),
            {Addon(QStringLiteral("alpha"), {Replaces(QStringLiteral("1.0"), QStringLiteral("Aircraft")), {}}),
             NewAddon(QStringLiteral("bravo")),
             Addon(QStringLiteral("charlie"),
                   {TheQuarantineHoldsOne(QStringLiteral("1.0"), QStringLiteral("Aircraft"), QStringLiteral("0.9")),
                    {}})})}),
        &window.parent);
    ShowItSettled(dialog);

    CategoryOf(dialog)->setCurrentIndex(0);
    DiscardBoxOf(dialog, 0, 2)->setCheckState(Qt::Checked);

    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:0"), QStringLiteral("0:1"), QStringLiteral("0:2")}));

    InstallOffer fresh = OfferOf({Archive(
        QStringLiteral("pack.zip"),
        {Addon(QStringLiteral("alpha"),
               {TheQuarantineHoldsOne(QStringLiteral("1.0"), QStringLiteral("Aircraft"), QStringLiteral("0.9")), {}}),
         NewAddon(QStringLiteral("bravo")),
         Addon(
             QStringLiteral("charlie"),
             {TheQuarantineHoldsOne(QStringLiteral("1.0"), QStringLiteral("Aircraft"), QStringLiteral("0.9")), {}})})});
    dialog.TakeAFreshOffer(fresh);

    QCOMPARE(Picks(dialog), QStringList({QStringLiteral("0:1"), QStringLiteral("0:2")}));
    QVERIFY2(!CellOf(dialog, 0, 0)->isEnabled(), "alpha now waits for a discard it was never given");
    QCOMPARE(DiscardBoxOf(dialog, 0, 0)->checkState(), Qt::Unchecked);
    QCOMPARE(DiscardBoxOf(dialog, 0, 2)->checkState(), Qt::Checked);
    QVERIFY(dialog.Chosen().back().discardsTheQuarantinedCopy);
}

void InstallDialogTest::CheckAgainRequestedComesFromTheButtonOfTheNotice()
{
    Window window(2400);
    InstallOffer offer = ThreeNewAddons();
    offer.runningSimulator = kSimulator;

    InstallDialog dialog(offer, &window.parent);
    ShowItSettled(dialog);

    QSignalSpy asked(&dialog, &InstallDialog::CheckAgainRequested);
    auto* again = dialog.findChild<QPushButton*>(QStringLiteral("CheckAgain"));

    QVERIFY(again != nullptr);
    QVERIFY(again->isVisible());
    QCOMPARE(asked.count(), 0);

    again->click();

    QCOMPARE(asked.count(), 1);
    QVERIFY2(dialog.isVisible(), "asking to check again does not close the dialog");
}

QTEST_MAIN(InstallDialogTest)

#include "tst_install_dialog.moc"
