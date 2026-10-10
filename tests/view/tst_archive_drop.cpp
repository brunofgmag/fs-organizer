#include <QtTest/QtTest>

#include <functional>
#include <optional>

#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QUrl>
#include <QtGui/QDropEvent>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#include "support/SizeText.h"
#include "tests/support/ArchiveInstallFixture.h"
#include "tests/support/CatalogueBesideTheBuild.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/InstalledCatalogue.h"
#include "tests/support/PathPrinting.h"
#include "view/library/InstallDialog.h"
#include "view/library/InstallReportDialog.h"
#include "view/shell/ArchiveDrop.h"
#include "view/shell/ArchiveDropLibrary.h"
#include "view/shell/ArchiveDropSign.h"
#include "view/shell/StepProgressDialog.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class ArchiveDropTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void initTestCase();

        static void EachKindOfArchiveIsInvitedWhateverTheCase_data();
        static void EachKindOfArchiveIsInvitedWhateverTheCase();
        static void ATextFileAloneIsRefusedWithItsSentenceAndTheDragIsNotAccepted();
        static void ALinkThatIsNotALocalFileIsNotAnArchive();
        static void ATextFileDraggedWithAnArchiveIsInvited();
        static void OverACategoryTheOutlineIsItsRowAndTheSentenceNamesIt();
        static void ElsewhereThereIsNoOutline();
        static void DraggingToAnotherPointMovesThePlate();
        static void RefusingPutsThePlateAtThePointerToo();
        static void WithoutALibraryTheDragIsRefusedAndNotAccepted();
        static void WhileTheViewModelIsBusyTheDragIsNotAcceptedAndNothingShows();
        static void BecomingBusyDuringADragHidesTheSignThatWasShowing();
        static void LeavingHidesTheSign();
        static void DroppingHidesTheSign();
        static void NoSentenceOfTheSignIsCutInAWindowOfTenTwentyFourPixels_data();
        static void NoSentenceOfTheSignIsCutInAWindowOfTenTwentyFourPixels();

        static void AFolderIsInvitedLikeAnArchive();
        static void AFolderThatIsALinkIsNotInvited();
        static void AFolderDraggedWithATextFileIsInvited();
        static void TheDiskIsAskedOncePerPathForTheWholeDragAndNeverOnAMove();
        static void ADragAskedAboutAgainAfterLeavingAsksTheDiskAgain();
        static void AnArchiveByItsSuffixNeverAsksTheDisk();
        static void DroppingHandsEveryDroppedFileAndTheCategoryToTheReading();
        static void DroppingHandsTheArchivesAndTheFoldersAndLeavesTheRestOut();
        static void ADropOfNothingButAFileThatIsNeitherIsRefused();
        static void AFolderInstallNamesItsStepsAsACopy();
        static void TheStepsFollowTheKindOfSourceBeingInstalled();
        static void DroppingWithoutACategoryHandsAnEmptyPath();
        static void ADropThatIsRefusedReadsNothing();
        static void TheReadingDialogAppearsOnlyPastTheDelay();
        static void TheReadingDialogFollowsWhichArchiveIsBeingRead();
        static void CancellingTheReadingStopsItAndEndsTheFlow();

        static void AnOfferWithNothingInstallableShowsTheReportAndNoInstallDialog();
        static void AnOfferWithAddonsShowsTheInstallDialog();
        static void CancellingTheInstallDialogInstallsNothing();
        static void CheckingAgainBringsAFreshOfferIntoTheOpenDialog();

        static void InstallingHandsTheViewModelWhatTheDialogChose();
        static void TheProgressDialogFollowsEachSignalInOrder();
        static void CancellingAsksTheViewModelToStopAndTheDialogSaysItIsCancelling();

        static void WhenEverythingLandsThereIsNoReport();
        static void WhenSomethingLandsTheLibraryComesForward();
        static void WhenSomethingLandsTheLandedFoldersAreSelected();
        static void WhenSomethingLandsTheUndoStateIsRefreshed();
        static void WhenSomethingLandsTheStatusCountsThemInTheRightPlural_data();
        static void WhenSomethingLandsTheStatusCountsThemInTheRightPlural();
        static void EachKindOfReportIsShown_data();
        static void EachKindOfReportIsShown();
        static void WhenNothingLandsNothingHappensInTheLibrary();
        static void ReadableLandedFoldersRunTheEnableGestureOnExactlyThoseFolders();
        static void NothingEnablesAnythingUntilTheViewModelSaysTheyAreReadable();
    };

    constexpr int kWindowWide = 1024;
    constexpr int kWindowTall = 720;
    constexpr int kLooksBeforeGivingUp = 200;
    constexpr int kPollEvery = 10;

    const std::filesystem::path kAlpha = kUtils / "alpha";
    const std::filesystem::path kBeta = kUtils / "beta";

    class LibraryDouble final : public ArchiveDropLibrary
    {
    public:
        [[nodiscard]] bool HasALibrary() const override
        {
            return hasALibrary;
        }

        [[nodiscard]] std::optional<DropCategory> CategoryUnder(const QWidget& over, const QPoint& point) const override
        {
            askedOver = &over;
            askedAt = point;

            return category;
        }

        void SelectOnceShown(const std::vector<std::filesystem::path>& folders) override
        {
            selected.push_back(folders);
        }

        void EnableAddons(const std::vector<std::filesystem::path>& folders) override
        {
            enabled.push_back(folders);
        }

        void RefreshUndoState() const override
        {
            ++undoRefreshed;
        }

        bool hasALibrary = true;
        std::optional<DropCategory> category{};
        mutable const QWidget* askedOver = nullptr;
        mutable QPoint askedAt{};
        mutable int undoRefreshed = 0;
        std::vector<std::vector<std::filesystem::path>> selected{};
        std::vector<std::vector<std::filesystem::path>> enabled{};
    };

    template<class Dialog>
    class Answers final
    {
    public:
        Answers(QWidget& over, std::function<void(Dialog&)> act, const bool mustAppear = true)
            : over_(over), act_(std::move(act)), mustAppear_(mustAppear)
        {
            timer_.setInterval(kPollEvery);
            QObject::connect(&timer_, &QTimer::timeout, &timer_,
                             [this]
                             {
                                 Look();
                             });
            timer_.start();
        }

        [[nodiscard]] bool Appeared() const
        {
            return appeared_;
        }

        [[nodiscard]] bool Answered() const
        {
            return appeared_ && !stuck_;
        }

        [[nodiscard]] const char* Said() const
        {
            return appeared_ ? "the dialog stayed open after the test acted on it" : "the dialog did not appear";
        }

    private:
        void Look()
        {
            if (acted_ != nullptr)
            {
                WatchItClose();
                return;
            }

            for (Dialog* dialog : over_.findChildren<Dialog*>())
            {
                if (dialog->isVisible())
                {
                    appeared_ = true;
                    acted_ = dialog;
                    looks_ = 0;
                    act_(*dialog);
                    return;
                }
            }

            GiveUpWaitingFor(kLooksBeforeGivingUp);
        }

        void WatchItClose()
        {
            if (!acted_->isVisible())
            {
                timer_.stop();
                return;
            }

            if (GiveUpWaitingFor(kLooksBeforeGivingUp))
            {
                stuck_ = true;
            }
        }

        bool GiveUpWaitingFor(const int looks)
        {
            if (++looks_ < looks)
            {
                return false;
            }

            timer_.stop();

            if (QWidget* modal = QApplication::activeModalWidget(); mustAppear_ && modal != nullptr)
            {
                modal->close();
            }

            return true;
        }

        QWidget& over_;
        std::function<void(Dialog&)> act_;
        bool mustAppear_ = true;
        QTimer timer_;
        QPointer<Dialog> acted_;
        bool appeared_ = false;
        bool stuck_ = false;
        int looks_ = 0;
    };

    class NoReportExpected final
    {
    public:
        explicit NoReportExpected(QWidget& over)
            : answers_(
                  over,
                  [](InstallReportDialog& report)
                  {
                      report.reject();
                  },
                  false)
        {
        }

        [[nodiscard]] bool Appeared() const
        {
            return answers_.Appeared();
        }

    private:
        Answers<InstallReportDialog> answers_;
    };

    struct Dragged
    {
        explicit Dragged(const QStringList& files)
        {
            QList<QUrl> urls;

            for (const QString& file : files)
            {
                urls.append(file.contains(QStringLiteral("://")) ? QUrl(file) : QUrl::fromLocalFile(file));
            }

            mime.setUrls(urls);
        }

        QMimeData mime;
    };

    [[nodiscard]] bool DragsInto(QWidget& window, const Dragged& dragged, const QPoint& at = QPoint(20, 20))
    {
        QDragEnterEvent enter(at, Qt::CopyAction, &dragged.mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &enter);

        return enter.isAccepted();
    }

    [[nodiscard]] bool DragsOver(QWidget& window, const Dragged& dragged, const QPoint& at)
    {
        QDragMoveEvent move(at, Qt::CopyAction, &dragged.mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &move);

        return move.isAccepted();
    }

    void DragLeaves(QWidget& window)
    {
        QDragLeaveEvent leave;
        QCoreApplication::sendEvent(&window, &leave);
    }

    [[nodiscard]] bool DropsOn(QWidget& window, const Dragged& dragged, const QPoint& at = QPoint(20, 20))
    {
        static_cast<void>(DragsInto(window, dragged, at));

        QDropEvent drop(QPointF(at), Qt::CopyAction, &dragged.mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &drop);

        return drop.isAccepted();
    }

    template<class Dialog>
    [[nodiscard]] QList<Dialog*> Visible(const QWidget& over)
    {
        QList<Dialog*> shown;

        for (Dialog* dialog : over.findChildren<Dialog*>())
        {
            if (dialog->isVisible())
            {
                shown.append(dialog);
            }
        }

        return shown;
    }

    [[nodiscard]] ArchiveDropSign* SignOf(const QWidget& window)
    {
        return window.findChild<ArchiveDropSign*>();
    }

    [[nodiscard]] QTimer* SlowReadingTimerOf(const ArchiveDrop& drop)
    {
        return drop.findChild<QTimer*>(QStringLiteral("ReadingIsSlow"));
    }

    void TheDelayPasses(QTimer& timer)
    {
        timer.start(0);
        QCoreApplication::processEvents();
    }

    [[nodiscard]] QString First()
    {
        return QStringLiteral("D:/Downloads/first.zip");
    }

    [[nodiscard]] QString Second()
    {
        return QStringLiteral("D:/Downloads/second.rar");
    }

    [[nodiscard]] QString Text()
    {
        return QStringLiteral("D:/Downloads/readme.txt");
    }

    [[nodiscard]] DropCategory UtilsRow()
    {
        return DropCategory{.path = kUtils, .shown = QStringLiteral("Utils"), .row = QRect(30, 200, 600, 24)};
    }

    struct Flow
    {
        Flow()
        {
            window.resize(kWindowWide, kWindowTall);
            window.show();
        }

        QWidget window;
        Fixture f;
        LibraryDouble library;
        ArchiveDrop drop{window, f.viewModel, library};
    };

    [[nodiscard]] QPushButton* ButtonNamed(const QWidget& over, const QString& name)
    {
        return over.findChild<QPushButton*>(name);
    }

    [[nodiscard]] QStringList StageOfEachStep(const StepProgressDialog& dialog)
    {
        QStringList stages;

        for (const QLabel* name : dialog.findChildren<QLabel*>(QStringLiteral("StepName")))
        {
            stages.append(name->property("stage").toString());
        }

        return stages;
    }

    [[nodiscard]] QStringList StepNamesOf(const StepProgressDialog& dialog)
    {
        QStringList names;

        for (const QLabel* name : dialog.findChildren<QLabel*>(QStringLiteral("StepName")))
        {
            names.append(name->text());
        }

        return names;
    }

    [[nodiscard]] QString SubjectOf(const StepProgressDialog& dialog)
    {
        return dialog.findChildren<QLabel*>(QStringLiteral("ImportQuiet")).front()->text();
    }

    [[nodiscard]] QString DetailOf(const StepProgressDialog& dialog)
    {
        return dialog.findChildren<QLabel*>(QStringLiteral("ImportQuiet")).back()->text();
    }

    [[nodiscard]] QTranslator& TheEnglishOfTheApp()
    {
        static QTranslator english;

        return english;
    }

    [[nodiscard]] InstallOutcome Landed(const std::vector<std::filesystem::path>& folders,
                                        std::optional<InstallReport> report = std::nullopt)
    {
        return InstallOutcome{.report = std::move(report),
                              .installed = static_cast<int>(folders.size()),
                              .landed = folders,
                              .enableAfter = false};
    }

    [[nodiscard]] std::vector<std::filesystem::path> FoldersLanded(const int landed)
    {
        std::vector<std::filesystem::path> folders{kAlpha, kBeta};
        folders.resize(static_cast<std::size_t>(landed));

        return folders;
    }

    [[nodiscard]] InstallReport ReportOf(const InstallReportKind kind)
    {
        return InstallReport{
            .kind = kind,
            .archives = {InstallReportArchive{
                .fileName = QStringLiteral("first.zip"),
                .lines = {InstallReportLine{.addon = QStringLiteral("beta"), .reason = QStringLiteral("A reason.")}}}},
            .installed = 1,
            .notInstalled = 1};
    }

    void ChooseTheUtilsCategory(const InstallDialog& dialog)
    {
        auto* category = dialog.findChild<QComboBox*>(QStringLiteral("InstallCategory"));
        category->setCurrentIndex(category->findData(AsText(kUtils)));
    }

    void TheLibraryHasUtils(Flow& flow)
    {
        flow.f.TheLibraryHoldsNodes(
            {}, {TreeNode{.kind = TreeNodeKind::Category, .path = kUtils, .declaredAsCategory = true}});
    }

    void LetTheInstallRunLater(Flow& flow)
    {
        flow.f.runner.defer = true;
    }

    const std::filesystem::path kDroppedFolder = "D:/Downloads/Meu Addon";

    [[nodiscard]] QString DroppedFolder()
    {
        return QStringLiteral("D:/Downloads/Meu Addon");
    }

    void ThereIsAFolderDropped(Flow& flow)
    {
        flow.f.fileSystem.AddDirectory(kDroppedFolder.parent_path());
        flow.f.fileSystem.AddDirectory(kDroppedFolder);
    }

    [[nodiscard]] bool OnlyDrops(QWidget& window, const Dragged& dragged, const QPoint& at = QPoint(20, 20))
    {
        QDropEvent drop(QPointF(at), Qt::CopyAction, &dragged.mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &drop);

        return drop.isAccepted();
    }
}

void ArchiveDropTest::initTestCase()
{
    ApplyModernistTheme(*qApp);

    QVERIFY2(LoadedTheShippedCatalogue(TheEnglishOfTheApp(), QStringLiteral("en")),
             "app_en.qm is not beside the build: build the release_translations target");
    QCoreApplication::installTranslator(&TheEnglishOfTheApp());
}

void ArchiveDropTest::EachKindOfArchiveIsInvitedWhateverTheCase_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("zip") << QStringLiteral("D:/Downloads/pack.zip");
    QTest::newRow("capital RAR") << QStringLiteral("D:/Downloads/pack.RAR");
    QTest::newRow("7z") << QStringLiteral("D:/Downloads/pack.7z");
    QTest::newRow("mixed case") << QStringLiteral("D:/Downloads/Pack.Zip");
}

void ArchiveDropTest::EachKindOfArchiveIsInvitedWhateverTheCase()
{
    QFETCH(QString, file);

    Flow flow;
    const Dragged dragged({file});

    QVERIFY(DragsInto(flow.window, dragged));

    const ArchiveDropSign* sign = SignOf(flow.window);
    QVERIFY(sign->isVisible());
    QVERIFY(!sign->Refuses());
    QCOMPARE(sign->Sentence(), QStringLiteral("Drop to install into the library. You choose the category next."));
}

void ArchiveDropTest::ATextFileAloneIsRefusedWithItsSentenceAndTheDragIsNotAccepted()
{
    Flow flow;
    const Dragged dragged({Text()});

    QVERIFY(!DragsInto(flow.window, dragged));

    const ArchiveDropSign* sign = SignOf(flow.window);
    QVERIFY(sign->isVisible());
    QVERIFY(sign->Refuses());
    QCOMPARE(sign->Sentence(), QStringLiteral("Only .zip, .rar and .7z archives and addon folders can be installed."));
}

void ArchiveDropTest::ALinkThatIsNotALocalFileIsNotAnArchive()
{
    Flow flow;
    const Dragged dragged({QStringLiteral("https://example.com/pack.zip")});

    QVERIFY(!DragsInto(flow.window, dragged));
    QVERIFY(SignOf(flow.window)->Refuses());
}

void ArchiveDropTest::ATextFileDraggedWithAnArchiveIsInvited()
{
    Flow flow;
    const Dragged dragged({Text(), First()});

    QVERIFY(DragsInto(flow.window, dragged));
    QVERIFY(!SignOf(flow.window)->Refuses());
    QVERIFY(SignOf(flow.window)->isVisible());
}

void ArchiveDropTest::OverACategoryTheOutlineIsItsRowAndTheSentenceNamesIt()
{
    Flow flow;
    flow.library.category = UtilsRow();
    const Dragged dragged({First()});
    const QPoint over(80, 210);

    QVERIFY(DragsInto(flow.window, dragged, over));

    const ArchiveDropSign* sign = SignOf(flow.window);
    QCOMPARE(sign->Target(), UtilsRow().row);
    QCOMPARE(sign->Sentence(), QStringLiteral("Drop to install into Utils"));
    QCOMPARE(flow.library.askedOver, &flow.window);
    QCOMPARE(flow.library.askedAt, over);

    flow.library.category = std::nullopt;
    QVERIFY(DragsOver(flow.window, dragged, QPoint(80, 400)));

    QVERIFY(sign->Target().isEmpty());
    QCOMPARE(sign->Sentence(), QStringLiteral("Drop to install into the library. You choose the category next."));
}

void ArchiveDropTest::ElsewhereThereIsNoOutline()
{
    Flow flow;
    const Dragged dragged({First()});

    QVERIFY(DragsInto(flow.window, dragged));

    QVERIFY(SignOf(flow.window)->Target().isEmpty());
}

void ArchiveDropTest::DraggingToAnotherPointMovesThePlate()
{
    Flow flow;
    const Dragged dragged({First()});
    const QPoint first(100, 150);
    const QPoint second(300, 250);

    QVERIFY(DragsInto(flow.window, dragged, first));

    const ArchiveDropSign* sign = SignOf(flow.window);
    const QRect before = sign->PlateBox();

    QCOMPARE(before.topLeft(), first + QPoint(kDropPlateRightOfThePointer, kDropPlateBelowThePointer));

    QVERIFY(DragsOver(flow.window, dragged, second));

    QCOMPARE(sign->PlateBox().topLeft(), second + QPoint(kDropPlateRightOfThePointer, kDropPlateBelowThePointer));
    QCOMPARE(sign->PlateBox().size(), before.size());
}

void ArchiveDropTest::RefusingPutsThePlateAtThePointerToo()
{
    Flow flow;
    const Dragged notAnArchive({Text()});
    const QPoint at(200, 120);

    QVERIFY(!DragsInto(flow.window, notAnArchive, at));

    const ArchiveDropSign* sign = SignOf(flow.window);

    QVERIFY(sign->Refuses());
    QVERIFY(sign->Target().isEmpty());
    QCOMPARE(sign->PlateBox().topLeft(), at + QPoint(kDropPlateRightOfThePointer, kDropPlateBelowThePointer));
}

void ArchiveDropTest::WithoutALibraryTheDragIsRefusedAndNotAccepted()
{
    Flow flow;
    flow.library.hasALibrary = false;
    const Dragged dragged({First()});

    QVERIFY(!DragsInto(flow.window, dragged));

    const ArchiveDropSign* sign = SignOf(flow.window);
    QVERIFY(sign->Refuses());
    QCOMPARE(sign->Sentence(), QStringLiteral("Add a library before installing an addon."));
}

void ArchiveDropTest::WhileTheViewModelIsBusyTheDragIsNotAcceptedAndNothingShows()
{
    Flow flow;
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    flow.f.runner.defer = true;
    flow.f.viewModel.Read({kFirst}, {});
    QVERIFY(flow.f.viewModel.Busy());

    const Dragged dragged({First()});

    QVERIFY(!DragsInto(flow.window, dragged));
    QVERIFY(!SignOf(flow.window)->isVisible());

    const Dragged notAnArchive({Text()});

    QVERIFY(!DragsInto(flow.window, notAnArchive));
    QVERIFY(!SignOf(flow.window)->isVisible());

    flow.f.runner.defer = false;
    flow.f.viewModel.Cancel();
    flow.f.runner.Finish();
}

void ArchiveDropTest::BecomingBusyDuringADragHidesTheSignThatWasShowing()
{
    Flow flow;
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    flow.f.runner.defer = true;
    const Dragged dragged({First()});

    QVERIFY(DragsInto(flow.window, dragged));
    QVERIFY(SignOf(flow.window)->isVisible());

    flow.f.viewModel.Read({kFirst}, {});
    QVERIFY(flow.f.viewModel.Busy());

    QVERIFY(!DragsOver(flow.window, dragged, QPoint(40, 40)));
    QVERIFY(!SignOf(flow.window)->isVisible());

    flow.f.runner.defer = false;
    flow.f.viewModel.Cancel();
    flow.f.runner.Finish();
}

void ArchiveDropTest::LeavingHidesTheSign()
{
    Flow flow;
    const Dragged dragged({First()});

    QVERIFY(DragsInto(flow.window, dragged));
    QVERIFY(SignOf(flow.window)->isVisible());

    DragLeaves(flow.window);

    QVERIFY(!SignOf(flow.window)->isVisible());
}

void ArchiveDropTest::DroppingHidesTheSign()
{
    Flow flow;
    flow.f.runner.defer = true;
    const Dragged dragged({First()});

    QVERIFY(DragsInto(flow.window, dragged));
    QVERIFY(DropsOn(flow.window, dragged));

    QVERIFY(!SignOf(flow.window)->isVisible());

    flow.f.viewModel.Cancel();
    flow.f.runner.Finish();
}

void ArchiveDropTest::NoSentenceOfTheSignIsCutInAWindowOfTenTwentyFourPixels_data()
{
    LanguageChoices();
}

void ArchiveDropTest::NoSentenceOfTheSignIsCutInAWindowOfTenTwentyFourPixels()
{
    QFETCH(QString, language);

    QTranslator catalogue;
    QVERIFY2(LoadedTheShippedCatalogue(catalogue, language),
             "app_pt_BR.qm is not beside the build: build the release_translations target");
    const Installed installed(catalogue);

    Flow flow;
    const ArchiveDropSign* sign = SignOf(flow.window);
    const Dragged archive({First()});
    const Dragged text({Text()});

    flow.library.category = DropCategory{
        .path = kUtils, .shown = QStringLiteral("Cenários · Aeroportos e cidades da Europa"), .row = UtilsRow().row};
    QVERIFY(DragsInto(flow.window, archive));
    QVERIFY2(!sign->IsCut(), qPrintable(sign->Sentence()));

    flow.library.category = std::nullopt;
    QVERIFY(DragsOver(flow.window, archive, QPoint(20, 20)));
    QVERIFY2(!sign->IsCut(), qPrintable(sign->Sentence()));

    QVERIFY(!DragsOver(flow.window, text, QPoint(20, 20)));
    QVERIFY2(!sign->IsCut(), qPrintable(sign->Sentence()));

    flow.library.hasALibrary = false;
    QVERIFY(!DragsOver(flow.window, archive, QPoint(20, 20)));
    QVERIFY2(!sign->IsCut(), qPrintable(sign->Sentence()));

    const QString refusal = language == QLatin1String("pt_BR")
        ? QStringLiteral("Adicione uma biblioteca antes de instalar um addon.")
        : QStringLiteral("Add a library before installing an addon.");

    QCOMPARE(sign->Sentence(), refusal);
}

void ArchiveDropTest::DroppingHandsEveryDroppedFileAndTheCategoryToTheReading()
{
    Flow flow;
    flow.library.category = UtilsRow();
    flow.f.runner.defer = true;
    const QSignalSpy reading(&flow.f.viewModel, &ArchiveInstallViewModel::ReadingArchive);
    const QSignalSpy offers(&flow.f.viewModel, &ArchiveInstallViewModel::OfferReady);
    const Dragged dragged({First(), Text(), Second()});

    QVERIFY(DropsOn(flow.window, dragged, QPoint(80, 210)));
    QCOMPARE(flow.library.askedAt, QPoint(80, 210));

    flow.f.runner.RunPendingWork();

    QCOMPARE(reading.size(), 2);
    QCOMPARE(reading[0].at(2).toString(), QStringLiteral("first.zip"));
    QCOMPARE(reading[1].at(2).toString(), QStringLiteral("second.rar"));

    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QCOMPARE(offers.size(), 1);
    QCOMPARE(offers.front().front().value<InstallOffer>().droppedOn, AsText(kUtils));
    QVERIFY2(closes.Answered(), closes.Said());
}

void ArchiveDropTest::DroppingWithoutACategoryHandsAnEmptyPath()
{
    Flow flow;
    flow.f.runner.defer = true;
    const QSignalSpy offers(&flow.f.viewModel, &ArchiveInstallViewModel::OfferReady);
    const Dragged dragged({First()});

    QVERIFY(DropsOn(flow.window, dragged));

    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QCOMPARE(offers.size(), 1);
    QVERIFY(offers.front().front().value<InstallOffer>().droppedOn.isEmpty());
    QVERIFY2(closes.Answered(), closes.Said());
}

void ArchiveDropTest::ADropThatIsRefusedReadsNothing()
{
    Flow flow;
    flow.f.runner.defer = true;
    const int runsBefore = flow.f.runner.runs;

    QVERIFY(!DropsOn(flow.window, Dragged({Text()})));
    QCOMPARE(flow.f.runner.runs, runsBefore);

    flow.library.hasALibrary = false;

    QVERIFY(!DropsOn(flow.window, Dragged({First()})));
    QCOMPARE(flow.f.runner.runs, runsBefore);
    QVERIFY(!flow.f.viewModel.Busy());
}

void ArchiveDropTest::TheReadingDialogAppearsOnlyPastTheDelay()
{
    Flow flow;
    flow.f.runner.defer = true;

    QVERIFY(DropsOn(flow.window, Dragged({First()})));
    QVERIFY(flow.f.viewModel.Busy());

    QTimer* slow = SlowReadingTimerOf(flow.drop);
    QVERIFY(slow != nullptr);
    QVERIFY(slow->isActive());
    QCOMPARE(slow->interval(), kReadingIsSlowAfter);
    QVERIFY(Visible<StepProgressDialog>(flow.window).isEmpty());

    TheDelayPasses(*slow);

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);
    QCOMPARE(shown.front()->windowTitle(), QStringLiteral("Install into the library"));
    QCOMPARE(StepNamesOf(*shown.front()), QStringList{QStringLiteral("Reading what it holds")});

    const auto meters = shown.front()->findChildren<QProgressBar*>(QStringLiteral("ImportMeter"));
    QCOMPARE(meters.size(), 1);
    QCOMPARE(meters.front()->minimum(), 0);
    QCOMPARE(meters.front()->maximum(), 0);

    flow.f.viewModel.Cancel();
    flow.f.runner.Finish();
}

void ArchiveDropTest::TheReadingDialogFollowsWhichArchiveIsBeingRead()
{
    Flow flow;
    flow.f.runner.defer = true;

    QVERIFY(DropsOn(flow.window, Dragged({First(), Second()})));

    emit flow.f.viewModel.ReadingArchive(1, 2, QStringLiteral("first.zip"));
    TheDelayPasses(*SlowReadingTimerOf(flow.drop));

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);
    QCOMPARE(SubjectOf(*shown.front()), QStringLiteral("Item 1 of 2 · first.zip"));

    emit flow.f.viewModel.ReadingArchive(2, 2, QStringLiteral("second.rar"));

    QCOMPARE(SubjectOf(*shown.front()), QStringLiteral("Item 2 of 2 · second.rar"));

    emit flow.f.viewModel.ReadingArchive(1, 1, QStringLiteral("only.zip"));

    QCOMPARE(SubjectOf(*shown.front()), QStringLiteral("only.zip"));

    flow.f.viewModel.Cancel();
    flow.f.runner.Finish();
}

void ArchiveDropTest::CancellingTheReadingStopsItAndEndsTheFlow()
{
    Flow flow;
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    flow.f.runner.defer = true;
    const QSignalSpy stopped(&flow.f.viewModel, &ArchiveInstallViewModel::ReadingStopped);
    const QSignalSpy offers(&flow.f.viewModel, &ArchiveInstallViewModel::OfferReady);
    const Answers<InstallDialog> offered(
        flow.window,
        [](InstallDialog& dialog)
        {
            dialog.reject();
        },
        false);

    QVERIFY(DropsOn(flow.window, Dragged({First()})));
    TheDelayPasses(*SlowReadingTimerOf(flow.drop));

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);

    ButtonNamed(*shown.front(), QStringLiteral("CancelTheSteps"))->click();
    flow.f.runner.Finish();

    QVERIFY2(!offered.Appeared(), "the install dialog appeared for a reading that was cancelled");
    QCOMPARE(stopped.size(), 1);
    QCOMPARE(offers.size(), 0);
    QVERIFY(Visible<StepProgressDialog>(flow.window).isEmpty());
    QVERIFY(Visible<InstallDialog>(flow.window).isEmpty());
    QVERIFY(!SlowReadingTimerOf(flow.drop)->isActive());
    QVERIFY(!flow.f.viewModel.Busy());
}

void ArchiveDropTest::AnOfferWithNothingInstallableShowsTheReportAndNoInstallDialog()
{
    Flow flow;
    const Answers<InstallReportDialog> closesTheReport(
        flow.window,
        [](InstallReportDialog& report)
        {
            ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
        });
    const Answers<InstallDialog> installs(
        flow.window,
        [](InstallDialog& dialog)
        {
            dialog.reject();
        },
        false);

    QVERIFY(DropsOn(flow.window, Dragged({First()})));

    QVERIFY2(closesTheReport.Answered(), closesTheReport.Said());
    QVERIFY2(!installs.Appeared(), "the install dialog appeared for an offer with nothing to install");
    QVERIFY(Visible<InstallReportDialog>(flow.window).isEmpty());
    QVERIFY(!flow.f.viewModel.Busy());
}

void ArchiveDropTest::AnOfferWithAddonsShowsTheInstallDialog()
{
    Flow flow;
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const Answers<InstallDialog> cancels(flow.window,
                                         [](InstallDialog& dialog)
                                         {
                                             dialog.reject();
                                         });
    const NoReportExpected reports(flow.window);

    QVERIFY(DropsOn(flow.window, Dragged({First()})));

    QVERIFY2(cancels.Answered(), cancels.Said());
    QVERIFY2(!reports.Appeared(), "a report appeared for an offer with addons");
}

void ArchiveDropTest::CancellingTheInstallDialogInstallsNothing()
{
    Flow flow;
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const int runsBefore = flow.f.runner.runs;
    const QSignalSpy finished(&flow.f.viewModel, &ArchiveInstallViewModel::Finished);
    const Answers<InstallDialog> cancels(flow.window,
                                         [](InstallDialog& dialog)
                                         {
                                             dialog.reject();
                                         });
    const NoReportExpected reports(flow.window);

    QVERIFY(DropsOn(flow.window, Dragged({First()})));

    QVERIFY2(cancels.Answered(), cancels.Said());
    QVERIFY2(!reports.Appeared(), "an install ran and reported after the dialog was cancelled");
    QCOMPARE(flow.f.runner.runs, runsBefore + 1);
    QCOMPARE(finished.size(), 0);
    QVERIFY(!flow.f.fileSystem.Exists(kAlpha));
    QVERIFY(Visible<StepProgressDialog>(flow.window).isEmpty());
    QVERIFY(flow.library.selected.empty());
}

void ArchiveDropTest::CheckingAgainBringsAFreshOfferIntoTheOpenDialog()
{
    Flow flow;
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    bool simulatorNoticeBefore = true;
    bool simulatorNoticeAfter = false;
    int categoryBefore = 0;
    int categoryAfter = 0;
    int dialogsOpen = 0;
    const NoReportExpected noReport(flow.window);
    const Answers<InstallDialog> checks(flow.window,
                                        [&](InstallDialog& dialog)
                                        {
                                            const QLabel* notice =
                                                dialog.findChild<QLabel*>(QStringLiteral("InstallNotice"));
                                            const auto* category =
                                                dialog.findChild<QComboBox*>(QStringLiteral("InstallCategory"));
                                            simulatorNoticeBefore = notice->isVisible();
                                            categoryBefore = category->currentIndex();

                                            flow.f.processProbe.ReportTheSimulatorAsRunning();
                                            emit dialog.CheckAgainRequested();

                                            simulatorNoticeAfter = notice->isVisible();
                                            categoryAfter = category->currentIndex();
                                            dialogsOpen = static_cast<int>(Visible<InstallDialog>(flow.window).size());
                                            dialog.reject();
                                        });

    QVERIFY(DropsOn(flow.window, Dragged({First()})));

    QVERIFY2(checks.Answered(), checks.Said());
    QVERIFY2(!simulatorNoticeBefore, "the notice showed before the simulator was running");
    QVERIFY2(simulatorNoticeAfter, "the open dialog did not take the fresh standings");
    QCOMPARE(categoryBefore, -1);
    QCOMPARE(categoryAfter, -1);
    QCOMPARE(dialogsOpen, 1);
    QVERIFY2(!noReport.Appeared(), "a report appeared although nothing was installed");
}

void ArchiveDropTest::InstallingHandsTheViewModelWhatTheDialogChose()
{
    Flow flow;
    TheLibraryHasUtils(flow);
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}, {"beta", "1.0.0"}});
    const QSignalSpy landedAndReadable(&flow.f.viewModel, &ArchiveInstallViewModel::LandedAndReadable);
    const Answers<InstallDialog> chooses(
        flow.window,
        [](InstallDialog& dialog)
        {
            auto* list = dialog.findChild<QTreeView*>(QStringLiteral("InstallList"));
            auto* model = qobject_cast<QStandardItemModel*>(list->model());
            model->item(0)->child(1, InstallDialog::AddonColumn)->setCheckState(Qt::Unchecked);

            ChooseTheUtilsCategory(dialog);
            dialog.findChild<QCheckBox*>(QStringLiteral("EnableAfterInstalling"))->setChecked(true);
            ButtonNamed(dialog, QStringLiteral("InstallChecked"))->click();
        });
    const NoReportExpected reports(flow.window);

    QVERIFY(DropsOn(flow.window, Dragged({First()})));

    QVERIFY2(chooses.Answered(), chooses.Said());
    QVERIFY2(!reports.Appeared(), "a report appeared for an install that was meant to land");
    QVERIFY2(flow.f.fileSystem.Exists(kAlpha), "the chosen addon did not land in the chosen category");
    QVERIFY2(!flow.f.fileSystem.Exists(kBeta), "an addon that was unchecked was installed");
    QCOMPARE(landedAndReadable.size(), 1);
    QCOMPARE(flow.library.enabled, (std::vector<std::vector<std::filesystem::path>>{{kAlpha}}));
}

void ArchiveDropTest::TheProgressDialogFollowsEachSignalInOrder()
{
    Flow flow;
    TheLibraryHasUtils(flow);
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const Answers<InstallDialog> chooses(flow.window,
                                         [&flow](InstallDialog& dialog)
                                         {
                                             ChooseTheUtilsCategory(dialog);
                                             LetTheInstallRunLater(flow);
                                             ButtonNamed(dialog, QStringLiteral("InstallChecked"))->click();
                                         });

    QVERIFY(DropsOn(flow.window, Dragged({First()})));

    QVERIFY2(chooses.Answered(), chooses.Said());
    QVERIFY(flow.f.viewModel.Busy());

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);

    const StepProgressDialog& dialog = *shown.front();
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Installing into the library"));
    QCOMPARE(StepNamesOf(dialog),
             (QStringList{QStringLiteral("Extracting"), QStringLiteral("Checking what was extracted"),
                          QStringLiteral("Putting into place")}));
    QCOMPARE(StageOfEachStep(dialog),
             (QStringList{QStringLiteral("now"), QStringLiteral("next"), QStringLiteral("next")}));

    emit flow.f.viewModel.InstallingArchive(1, 1, QStringLiteral("first.zip"));
    QCOMPARE(SubjectOf(dialog), QStringLiteral("first.zip"));

    emit flow.f.viewModel.InstallingArchive(2, 3, QStringLiteral("second.rar"));
    QCOMPARE(SubjectOf(dialog), QStringLiteral("Item 2 of 3 · second.rar"));

    emit flow.f.viewModel.InstallingAddon(2, 4, QStringLiteral("beta"));
    QCOMPARE(DetailOf(dialog), QStringLiteral("Addon 2 of 4 · beta"));

    emit flow.f.viewModel.StepChanged(InstallStep::Extracting);
    emit flow.f.viewModel.Progressed(512 * 1024, 2 * 1024 * 1024);
    QCOMPARE(DetailOf(dialog), QStringLiteral("%1 of %2").arg(AsSize(512 * 1024), AsSize(2 * 1024 * 1024)));
    QCOMPARE(dialog.findChildren<QProgressBar*>(QStringLiteral("ImportMeter")).front()->value(), 250);

    emit flow.f.viewModel.StepChanged(InstallStep::Checking);
    QCOMPARE(StageOfEachStep(dialog),
             (QStringList{QStringLiteral("done"), QStringLiteral("now"), QStringLiteral("next")}));

    emit flow.f.viewModel.StepChanged(InstallStep::PuttingIntoPlace);
    QCOMPARE(StageOfEachStep(dialog),
             (QStringList{QStringLiteral("done"), QStringLiteral("done"), QStringLiteral("now")}));

    flow.f.viewModel.Cancel();
    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QVERIFY2(closes.Answered(), closes.Said());
}

void ArchiveDropTest::CancellingAsksTheViewModelToStopAndTheDialogSaysItIsCancelling()
{
    Flow flow;
    TheLibraryHasUtils(flow);
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const Answers<InstallDialog> chooses(flow.window,
                                         [&flow](InstallDialog& dialog)
                                         {
                                             ChooseTheUtilsCategory(dialog);
                                             LetTheInstallRunLater(flow);
                                             ButtonNamed(dialog, QStringLiteral("InstallChecked"))->click();
                                         });

    QVERIFY(DropsOn(flow.window, Dragged({First()})));
    QVERIFY2(chooses.Answered(), chooses.Said());

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);

    const QSignalSpy cancelling(&flow.f.viewModel, &ArchiveInstallViewModel::Cancelling);
    QPushButton* cancel = ButtonNamed(*shown.front(), QStringLiteral("CancelTheSteps"));
    cancel->click();

    QCOMPARE(cancelling.size(), 1);
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(cancel->text(), QStringLiteral("Cancelling…"));
    QCOMPARE(DetailOf(*shown.front()), QStringLiteral("Deleting the temporary copy…"));

    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QVERIFY2(closes.Answered(), closes.Said());
    QVERIFY(Visible<StepProgressDialog>(flow.window).isEmpty());
    QVERIFY2(!flow.f.fileSystem.Exists(kAlpha), "a cancelled install left the addon behind");
}

void ArchiveDropTest::WhenEverythingLandsThereIsNoReport()
{
    Flow flow;
    const NoReportExpected reports(flow.window);

    emit flow.f.viewModel.Finished(Landed(FoldersLanded(2)));

    QVERIFY2(!reports.Appeared(), "a report appeared although everything landed");
}

void ArchiveDropTest::WhenSomethingLandsTheLibraryComesForward()
{
    Flow flow;
    const NoReportExpected noReport(flow.window);
    const QSignalSpy forward(&flow.drop, &ArchiveDrop::LibraryRequested);

    emit flow.f.viewModel.Finished(Landed(FoldersLanded(2)));

    QCOMPARE(forward.size(), 1);
    QVERIFY2(!noReport.Appeared(), "a report appeared for what had nothing to report");
}

void ArchiveDropTest::WhenSomethingLandsTheLandedFoldersAreSelected()
{
    Flow flow;
    const NoReportExpected noReport(flow.window);
    const std::vector<std::filesystem::path> folders = FoldersLanded(2);

    emit flow.f.viewModel.Finished(Landed(folders));

    QCOMPARE(flow.library.selected, (std::vector<std::vector<std::filesystem::path>>{folders}));
    QVERIFY2(!noReport.Appeared(), "a report appeared for what had nothing to report");
}

void ArchiveDropTest::WhenSomethingLandsTheUndoStateIsRefreshed()
{
    Flow flow;
    const NoReportExpected noReport(flow.window);

    emit flow.f.viewModel.Finished(Landed(FoldersLanded(2)));

    QCOMPARE(flow.library.undoRefreshed, 1);
    QVERIFY2(!noReport.Appeared(), "a report appeared for what had nothing to report");
}

void ArchiveDropTest::WhenSomethingLandsTheStatusCountsThemInTheRightPlural_data()
{
    QTest::addColumn<int>("landed");
    QTest::addColumn<QString>("status");

    QTest::newRow("one") << 1 << QStringLiteral("1 addon installed into the library.");
    QTest::newRow("two") << 2 << QStringLiteral("2 addons installed into the library.");
}

void ArchiveDropTest::WhenSomethingLandsTheStatusCountsThemInTheRightPlural()
{
    QFETCH(int, landed);
    QFETCH(QString, status);

    Flow flow;
    const NoReportExpected noReport(flow.window);
    const QSignalSpy said(&flow.drop, &ArchiveDrop::StatusChanged);

    emit flow.f.viewModel.Finished(Landed(FoldersLanded(landed)));

    QCOMPARE(said.size(), 1);
    QCOMPARE(said.front().front().toString(), status);
    QVERIFY2(!noReport.Appeared(), "a report appeared for what had nothing to report");
}

void ArchiveDropTest::EachKindOfReportIsShown_data()
{
    QTest::addColumn<InstallReportKind>("kind");
    QTest::addColumn<int>("landed");

    QTest::newRow("some were not installed") << InstallReportKind::SomeWereNotInstalled << 1;
    QTest::newRow("nothing was installed") << InstallReportKind::NothingWasInstalled << 0;
    QTest::newRow("installed but not linked") << InstallReportKind::InstalledButNotLinked << 1;
    QTest::newRow("installed but a folder stayed") << InstallReportKind::InstalledButAFolderStayed << 1;
    QTest::newRow("nothing to install") << InstallReportKind::NothingToInstall << 0;
}

void ArchiveDropTest::EachKindOfReportIsShown()
{
    QFETCH(InstallReportKind, kind);
    QFETCH(int, landed);

    Flow flow;
    std::vector<std::filesystem::path> folders{kAlpha};
    folders.resize(static_cast<std::size_t>(landed));
    const QSignalSpy forward(&flow.drop, &ArchiveDrop::LibraryRequested);
    const QSignalSpy said(&flow.drop, &ArchiveDrop::StatusChanged);
    const Answers<InstallReportDialog> reports(flow.window,
                                               [](InstallReportDialog& report)
                                               {
                                                   ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                               });

    emit flow.f.viewModel.Finished(Landed(folders, ReportOf(kind)));

    QVERIFY2(reports.Answered(), reports.Said());
    QCOMPARE(forward.size(), landed);
    QCOMPARE(said.size(), landed);
}

void ArchiveDropTest::WhenNothingLandsNothingHappensInTheLibrary()
{
    Flow flow;
    const QSignalSpy forward(&flow.drop, &ArchiveDrop::LibraryRequested);
    const QSignalSpy said(&flow.drop, &ArchiveDrop::StatusChanged);
    const Answers<InstallReportDialog> reports(flow.window,
                                               [](InstallReportDialog& report)
                                               {
                                                   ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                               });

    emit flow.f.viewModel.Finished(Landed({}, ReportOf(InstallReportKind::NothingWasInstalled)));

    QVERIFY2(reports.Answered(), reports.Said());
    QCOMPARE(forward.size(), 0);
    QCOMPARE(said.size(), 0);
    QVERIFY(flow.library.selected.empty());
    QCOMPARE(flow.library.undoRefreshed, 0);
}

void ArchiveDropTest::ReadableLandedFoldersRunTheEnableGestureOnExactlyThoseFolders()
{
    Flow flow;
    const std::vector<std::filesystem::path> folders{kAlpha, kBeta};

    emit flow.f.viewModel.LandedAndReadable(folders);

    QCOMPARE(flow.library.enabled, (std::vector<std::vector<std::filesystem::path>>{folders}));
}

void ArchiveDropTest::NothingEnablesAnythingUntilTheViewModelSaysTheyAreReadable()
{
    Flow flow;
    const NoReportExpected noReport(flow.window);
    InstallOutcome outcome = Landed({kAlpha});
    outcome.enableAfter = true;

    emit flow.f.viewModel.Finished(outcome);

    QVERIFY(flow.library.enabled.empty());
    QVERIFY(!flow.library.selected.empty());
    QVERIFY2(!noReport.Appeared(), "a report appeared for what had nothing to report");
}

void ArchiveDropTest::AFolderIsInvitedLikeAnArchive()
{
    Flow flow;
    ThereIsAFolderDropped(flow);
    const Dragged dragged({DroppedFolder()});

    QVERIFY(DragsInto(flow.window, dragged));

    const ArchiveDropSign* sign = SignOf(flow.window);
    QVERIFY(sign->isVisible());
    QVERIFY(!sign->Refuses());
    QCOMPARE(sign->Sentence(), QStringLiteral("Drop to install into the library. You choose the category next."));

    flow.library.category = UtilsRow();

    QVERIFY(DragsOver(flow.window, dragged, QPoint(80, 210)));
    QCOMPARE(sign->Target(), UtilsRow().row);
    QCOMPARE(sign->Sentence(), QStringLiteral("Drop to install into Utils"));
}

void ArchiveDropTest::AFolderThatIsALinkIsNotInvited()
{
    Flow flow;
    ThereIsAFolderDropped(flow);
    flow.f.fileSystem.AddLink("D:/Downloads/Shortcut", kDroppedFolder);

    QVERIFY(!DragsInto(flow.window, Dragged({QStringLiteral("D:/Downloads/Shortcut")})));
    QVERIFY(SignOf(flow.window)->Refuses());
    QCOMPARE(SignOf(flow.window)->Sentence(),
             QStringLiteral("Only .zip, .rar and .7z archives and addon folders can be installed."));
}

void ArchiveDropTest::AFolderDraggedWithATextFileIsInvited()
{
    Flow flow;
    ThereIsAFolderDropped(flow);

    QVERIFY(DragsInto(flow.window, Dragged({Text(), DroppedFolder()})));
    QVERIFY(!SignOf(flow.window)->Refuses());
}

void ArchiveDropTest::TheDiskIsAskedOncePerPathForTheWholeDragAndNeverOnAMove()
{
    Flow flow;
    ThereIsAFolderDropped(flow);
    flow.f.runner.defer = true;
    const Dragged dragged({DroppedFolder(), Text()});

    QVERIFY(DragsInto(flow.window, dragged));

    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt(kDroppedFolder), std::size_t{1});
    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt("D:/Downloads/readme.txt"), std::size_t{1});

    for (int step = 0; step < 5; ++step)
    {
        QVERIFY(DragsOver(flow.window, dragged, QPoint(30 + step * 10, 40)));
    }

    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt(kDroppedFolder), std::size_t{1});
    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt("D:/Downloads/readme.txt"), std::size_t{1});

    QVERIFY(OnlyDrops(flow.window, dragged));

    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt(kDroppedFolder), std::size_t{1});
    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt("D:/Downloads/readme.txt"), std::size_t{1});

    flow.f.viewModel.Cancel();
    flow.f.runner.Finish();
}

void ArchiveDropTest::ADragAskedAboutAgainAfterLeavingAsksTheDiskAgain()
{
    Flow flow;
    ThereIsAFolderDropped(flow);
    const Dragged dragged({DroppedFolder()});

    QVERIFY(DragsInto(flow.window, dragged));
    DragLeaves(flow.window);
    QVERIFY(DragsInto(flow.window, dragged));
    QVERIFY(DragsOver(flow.window, dragged, QPoint(50, 50)));

    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt(kDroppedFolder), std::size_t{2});
}

void ArchiveDropTest::AnArchiveByItsSuffixNeverAsksTheDisk()
{
    Flow flow;
    const Dragged dragged({First(), Second()});

    QVERIFY(DragsInto(flow.window, dragged));
    QVERIFY(DragsOver(flow.window, dragged, QPoint(50, 50)));

    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt(kFirst), std::size_t{0});
    QCOMPARE(flow.f.filesystemProbe.TimesLookedAt(kSecond), std::size_t{0});
}

void ArchiveDropTest::DroppingHandsTheArchivesAndTheFoldersAndLeavesTheRestOut()
{
    Flow flow;
    ThereIsAFolderDropped(flow);
    flow.f.runner.defer = true;
    const QSignalSpy reading(&flow.f.viewModel, &ArchiveInstallViewModel::ReadingArchive);
    const Dragged dragged({First(), Text(), DroppedFolder(), Second()});

    QVERIFY(DropsOn(flow.window, dragged));

    flow.f.runner.RunPendingWork();

    QCOMPARE(reading.size(), 3);
    QCOMPARE(reading[0].at(2).toString(), QStringLiteral("first.zip"));
    QCOMPARE(reading[1].at(2).toString(), QStringLiteral("Meu Addon"));
    QCOMPARE(reading[2].at(2).toString(), QStringLiteral("second.rar"));

    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QVERIFY2(closes.Answered(), closes.Said());
}

void ArchiveDropTest::ADropOfNothingButAFileThatIsNeitherIsRefused()
{
    Flow flow;
    flow.f.runner.defer = true;
    const int runsBefore = flow.f.runner.runs;

    QVERIFY(!DropsOn(flow.window, Dragged({Text(), QStringLiteral("D:/Downloads/missing")})));
    QCOMPARE(flow.f.runner.runs, runsBefore);
    QVERIFY(!flow.f.viewModel.Busy());
}

void ArchiveDropTest::AFolderInstallNamesItsStepsAsACopy()
{
    Flow flow;
    TheLibraryHasUtils(flow);
    ThereIsAFolderDropped(flow);
    flow.f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    const Answers<InstallDialog> chooses(flow.window,
                                         [&flow](InstallDialog& dialog)
                                         {
                                             ChooseTheUtilsCategory(dialog);
                                             LetTheInstallRunLater(flow);
                                             ButtonNamed(dialog, QStringLiteral("InstallChecked"))->click();
                                         });

    QVERIFY(DropsOn(flow.window, Dragged({DroppedFolder()})));
    QVERIFY2(chooses.Answered(), chooses.Said());

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);
    QCOMPARE(StepNamesOf(*shown.front()),
             (QStringList{QStringLiteral("Copying"), QStringLiteral("Checking what was copied"),
                          QStringLiteral("Putting into place")}));

    flow.f.viewModel.Cancel();
    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QVERIFY2(closes.Answered(), closes.Said());
}

void ArchiveDropTest::TheStepsFollowTheKindOfSourceBeingInstalled()
{
    Flow flow;
    TheLibraryHasUtils(flow);
    ThereIsAFolderDropped(flow);
    flow.f.ArchiveHolds(kDroppedFolder, {{"Meu Addon", "1.0.0"}});
    flow.f.ArchiveHolds(kFirst, {{"alpha", "1.0.0"}});
    const Answers<InstallDialog> chooses(flow.window,
                                         [&flow](InstallDialog& dialog)
                                         {
                                             ChooseTheUtilsCategory(dialog);
                                             LetTheInstallRunLater(flow);
                                             ButtonNamed(dialog, QStringLiteral("InstallChecked"))->click();
                                         });

    QVERIFY(DropsOn(flow.window, Dragged({First(), DroppedFolder()})));
    QVERIFY2(chooses.Answered(), chooses.Said());

    const QList<StepProgressDialog*> shown = Visible<StepProgressDialog>(flow.window);
    QCOMPARE(shown.size(), 1);

    const StepProgressDialog& dialog = *shown.front();
    const QStringList extracting{QStringLiteral("Extracting"), QStringLiteral("Checking what was extracted"),
                                 QStringLiteral("Putting into place")};
    const QStringList copying{QStringLiteral("Copying"), QStringLiteral("Checking what was copied"),
                              QStringLiteral("Putting into place")};

    QCOMPARE(StepNamesOf(dialog), extracting);

    emit flow.f.viewModel.StepChanged(InstallStep::Copying);
    QCOMPARE(StepNamesOf(dialog), copying);
    QCOMPARE(StageOfEachStep(dialog),
             (QStringList{QStringLiteral("now"), QStringLiteral("next"), QStringLiteral("next")}));

    emit flow.f.viewModel.StepChanged(InstallStep::Checking);
    QCOMPARE(StepNamesOf(dialog), copying);
    QCOMPARE(StageOfEachStep(dialog),
             (QStringList{QStringLiteral("done"), QStringLiteral("now"), QStringLiteral("next")}));

    emit flow.f.viewModel.StepChanged(InstallStep::Extracting);
    QCOMPARE(StepNamesOf(dialog), extracting);
    QCOMPARE(StageOfEachStep(dialog),
             (QStringList{QStringLiteral("now"), QStringLiteral("next"), QStringLiteral("next")}));

    flow.f.viewModel.Cancel();
    const Answers<InstallReportDialog> closes(flow.window,
                                              [](InstallReportDialog& report)
                                              {
                                                  ButtonNamed(report, QStringLiteral("CloseTheReport"))->click();
                                              });
    flow.f.runner.Finish();

    QVERIFY2(closes.Answered(), closes.Said());
}

QTEST_MAIN(ArchiveDropTest)
#include "tst_archive_drop.moc"
