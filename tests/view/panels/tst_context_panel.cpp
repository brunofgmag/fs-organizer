#include <functional>

#include <QtCore/QSettings>
#include <QtGui/QStandardItemModel>
#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "view/panels/ContextPanel.h"
#include "view/panels/ModelRowDetail.h"
#include "view/panels/PanelRail.h"
#include "view/shell/TriageStrip.h"
#include "view/theme/ModernistPaint.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/ModernistTones.h"

namespace
{
    class ContextPanelTest : public QObject
    {
        Q_OBJECT

    private slots:
        void initTestCase();
        static void CollapsingHidesTheContentAndExpandingBringsItBack();
        static void ACollapsedPanelBecomesARailThatReachesTheBottom();
        static void TheRailBringsThePanelBack();
        static void TheSpineCarriesTheNameOfWhatIsSelected();
        static void ARowThatNeedsAttentionPutsADotOnTheSpine();
        static void TheSpineAndTheDotShareTheSameAxis();
        static void TheCollapsedStateSurvivesANewPanelWithTheSameName();
        static void TheDetailShowsEveryColumnOfTheSelectedRow();
        static void AnInvalidIndexClearsTheDetailToItsPlaceholder();
        static void TheStripKeepsQuietUntilSomethingBreaks();
        static void ADuplicatedAddonGetsItsOwnItemAndAsksToBeSeen();
        static void ClosingThePanelAsksForIt();
        static void ContentTallerThanThePanelScrollsInsteadOfBeingSquashed();
        static void APathWiderThanThePanelNeverAsksForMoreRoomThanItHas();
        static void AFieldHandsOutExactlyTheTextItShows();
        static void SelectingAPathPaintsItOnAGroundThatIsNotTheSurfaceUnderIt();
    };
}

void ContextPanelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("fs-organizer-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("context-panel"));
    QSettings().clear();

    ApplyModernistTheme(*qobject_cast<QApplication*>(QCoreApplication::instance()));
}

void ContextPanelTest::CollapsingHidesTheContentAndExpandingBringsItBack()
{
    ContextPanel panel(QStringLiteral("Attention"));
    panel.setObjectName(QStringLiteral("collapse-test"));

    auto* body = new QLabel(QStringLiteral("content"));
    panel.Add(body);

    QVERIFY(body->isVisibleTo(&panel));

    auto* toggle = panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"));
    QVERIFY(toggle != nullptr);

    toggle->click();
    QVERIFY(!body->isVisibleTo(&panel));

    toggle->click();
    QVERIFY(body->isVisibleTo(&panel));
}

void ContextPanelTest::ACollapsedPanelBecomesARailThatReachesTheBottom()
{
    ContextPanel panel(QStringLiteral("Addon selected"));
    panel.setObjectName(QStringLiteral("rail-height-test"));
    panel.Add(new QLabel(QStringLiteral("content")));
    panel.resize(380, 400);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    auto* rail = panel.findChild<PanelRail*>();
    QVERIFY(rail != nullptr);
    QVERIFY(!rail->isVisibleTo(&panel));

    panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"))->click();
    QVERIFY(QTest::qWaitFor(
        [&panel]
        {
            return panel.width() == PanelRail::Width();
        }));

    QVERIFY(rail->isVisibleTo(&panel));
    QCOMPARE(rail->width(), PanelRail::Width());

    const QWidget* header = panel.findChild<QWidget*>(QStringLiteral("PanelHeader"));
    QVERIFY(header != nullptr);
    QVERIFY(!header->isVisibleTo(&panel));

    QCOMPARE(rail->geometry().top(), 0);
    QCOMPARE(rail->geometry().bottom(), panel.height() - 1);
}

void ContextPanelTest::TheRailBringsThePanelBack()
{
    ContextPanel panel(QStringLiteral("Addon selected"));
    panel.setObjectName(QStringLiteral("rail-expand-test"));

    auto* body = new QLabel(QStringLiteral("content"));
    panel.Add(body);

    panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"))->click();
    QVERIFY(!body->isVisibleTo(&panel));

    panel.findChild<QToolButton*>(QStringLiteral("PanelExpand"))->click();

    QVERIFY(body->isVisibleTo(&panel));
    QCOMPARE(panel.width(), 380);
}

namespace
{
    struct RailShot
    {
        QImage painted;
        QRect arrow;
    };

    RailShot RailShotOf(const QString& title, const bool alarming)
    {
        ContextPanel panel(QStringLiteral("Addon selected"));
        panel.setObjectName(QStringLiteral("spine-test"));
        panel.Add(new QLabel(QStringLiteral("content")));
        panel.resize(380, 400);
        panel.show();

        if (!QTest::qWaitForWindowExposed(&panel))
        {
            return {};
        }

        panel.ShowTitle(title, alarming);
        panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"))->click();

        if (!QTest::qWaitFor(
                [&panel]
                {
                    return panel.width() == PanelRail::Width();
                }))
        {
            return {};
        }

        auto* rail = panel.findChild<PanelRail*>();

        QImage painted(rail->size(), QImage::Format_ARGB32);
        painted.fill(Qt::transparent);
        rail->render(&painted);

        return {.painted = painted, .arrow = rail->findChild<QToolButton*>(QStringLiteral("PanelExpand"))->geometry()};
    }

    QImage RailPainted(const QString& title, const bool alarming)
    {
        return RailShotOf(title, alarming).painted;
    }

    int PixelsOf(const QImage& painted, const QColor& ink)
    {
        int found = 0;

        for (int y = 0; y < painted.height(); ++y)
        {
            for (int x = 0; x < painted.width(); ++x)
            {
                found += painted.pixelColor(x, y) == ink ? 1 : 0;
            }
        }

        return found;
    }
}

void ContextPanelTest::TheSpineCarriesTheNameOfWhatIsSelected()
{
    ContextPanel panel(QStringLiteral("Addon selected"));
    panel.setObjectName(QStringLiteral("spine-name-test"));
    panel.Add(new QLabel(QStringLiteral("content")));

    const auto* rail = panel.findChild<PanelRail*>();

    panel.ShowTitle(QStringLiteral("aerosoft-crj"));
    QCOMPARE(rail->toolTip(), QStringLiteral("aerosoft-crj"));

    panel.ShowTitle(QStringLiteral("pmdg-aircraft-738"));
    QCOMPARE(rail->toolTip(), QStringLiteral("pmdg-aircraft-738"));

    panel.ShowTitle(QString());
    QCOMPARE(rail->toolTip(), QStringLiteral("ADDON SELECTED"));
}

void ContextPanelTest::ARowThatNeedsAttentionPutsADotOnTheSpine()
{
    const QImage quiet = RailPainted(QStringLiteral("aerosoft-crj"), false);
    const QImage alarmed = RailPainted(QStringLiteral("aerosoft-crj"), true);

    QCOMPARE(PixelsOf(quiet, AlertInk()), 0);
    QCOMPARE(PixelsOf(alarmed, AlertInk()), 25);
}

namespace
{
    struct Span
    {
        int left = 0;
        int right = -1;
        int top = -1;
        int bottom = -1;
    };

    Span SpanOf(const QImage& painted,
                const int from,
                const int until,
                const std::function<bool(const QColor&, int)>& belongs)
    {
        Span found{.left = painted.width(), .right = -1, .top = -1, .bottom = -1};

        for (int y = from; y < until; ++y)
        {
            for (int x = 0; x < painted.width(); ++x)
            {
                if (!belongs(painted.pixelColor(x, y), x))
                {
                    continue;
                }

                found.left = qMin(found.left, x);
                found.right = qMax(found.right, x);
                found.top = found.top < 0 ? y : found.top;
                found.bottom = y;
            }
        }

        return found;
    }
}

void ContextPanelTest::TheSpineAndTheDotShareTheSameAxis()
{
    const auto [painted, arrow] = RailShotOf(QStringLiteral("Aircrafts"), true);

    QList<QColor> ground;
    for (int x = 0; x < painted.width(); ++x)
    {
        ground.append(painted.pixelColor(x, painted.height() - 2));
    }

    const Span dot = SpanOf(painted, 0, painted.height(),
                            [](const QColor& here, int)
                            {
                                return here == AlertInk();
                            });
    QCOMPARE(dot.right - dot.left + 1, 5);

    const auto paintedOver = [&ground](const QColor& here, const int column)
    {
        return here != ground.at(column);
    };

    const Span chevron = SpanOf(painted, 0, dot.top, paintedOver);
    const Span spine = SpanOf(painted, dot.bottom + 1, painted.height(), paintedOver);

    const int axis = painted.width() - 1;

    QVERIFY(chevron.right > chevron.left);
    QVERIFY(qAbs(chevron.left + chevron.right - axis) <= 1);
    QVERIFY(qAbs(dot.left + dot.right - axis) <= 1);
    QVERIFY(spine.right > spine.left);
    QCOMPARE(spine.left + spine.right, axis);

    QCOMPARE(arrow.left() + arrow.right(), chevron.left + chevron.right);
    QVERIFY(arrow.top() > 0);
}

void ContextPanelTest::TheCollapsedStateSurvivesANewPanelWithTheSameName()
{
    {
        ContextPanel panel(QStringLiteral("Attention"));
        panel.setObjectName(QStringLiteral("persist-test"));
        panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"))->click();
    }

    ContextPanel reborn(QStringLiteral("Attention"));
    reborn.setObjectName(QStringLiteral("persist-test"));
    reborn.RestoreCollapsedState();

    auto* body = new QLabel(QStringLiteral("content"));
    reborn.Add(body);

    QVERIFY(!body->isVisibleTo(&reborn));
}

void ContextPanelTest::TheDetailShowsEveryColumnOfTheSelectedRow()
{
    QStandardItemModel model(2, 2);
    model.setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Destination")});
    model.setItem(1, 0, new QStandardItem(QStringLiteral("airport-bgqq-qaanaaq")));
    model.setItem(1, 1, new QStandardItem(QStringLiteral("Community")));

    ModelRowDetail detail;
    detail.Show(model.index(1, 0));

    QStringList texts;
    for (const QLabel* label : detail.findChildren<QLabel*>())
    {
        texts.append(label->text());
    }
    for (const QTextEdit* value : detail.findChildren<QTextEdit*>())
    {
        texts.append(value->toPlainText());
    }

    QVERIFY(texts.contains(QStringLiteral("Name")));
    QVERIFY(texts.contains(QStringLiteral("airport-bgqq-qaanaaq")));
    QVERIFY(texts.contains(QStringLiteral("Destination")));
    QVERIFY(texts.contains(QStringLiteral("Community")));
}

void ContextPanelTest::AnInvalidIndexClearsTheDetailToItsPlaceholder()
{
    QStandardItemModel model(1, 1);
    model.setItem(0, 0, new QStandardItem(QStringLiteral("row")));

    ModelRowDetail detail;
    detail.Show(model.index(0, 0));
    detail.Show({});

    const QList<QLabel*> labels = detail.findChildren<QLabel*>();
    QCOMPARE(labels.size(), 1);
    QVERIFY(!labels.front()->text().isEmpty());
}

void ContextPanelTest::TheStripKeepsQuietUntilSomethingBreaks()
{
    TriageStrip strip;

    strip.ShowBreakdown({});
    QVERIFY(!strip.HasAnythingToSay());

    strip.ShowBreakdown({.unmanaged = 178});
    QVERIFY(strip.HasAnythingToSay());

    strip.show();
    QVERIFY(QTest::qWaitForWindowExposed(&strip));

    int shown = 0;
    for (const QPushButton* action : strip.findChildren<QPushButton*>())
    {
        shown += action->isVisibleTo(&strip) ? 1 : 0;
    }

    QCOMPARE(shown, 1);
}

void ContextPanelTest::ADuplicatedAddonGetsItsOwnItemAndAsksToBeSeen()
{
    TriageStrip strip;

    strip.ShowBreakdown({.duplicated = 2});
    QVERIFY(strip.HasAnythingToSay());

    strip.show();
    QVERIFY(QTest::qWaitForWindowExposed(&strip));

    const QSignalSpy asked(&strip, &TriageStrip::DuplicatesRequested);

    QPushButton* shown = nullptr;
    for (QPushButton* action : strip.findChildren<QPushButton*>())
    {
        if (action->isVisibleTo(&strip))
        {
            QVERIFY(shown == nullptr);
            shown = action;
        }
    }

    QVERIFY(shown != nullptr);
    shown->click();
    QCOMPARE(asked.size(), 1);
}

void ContextPanelTest::ClosingThePanelAsksForIt()
{
    ContextPanel panel(QStringLiteral("Entry selected"));
    panel.setObjectName(QStringLiteral("close-test"));

    const QSignalSpy asked(&panel, &ContextPanel::CloseRequested);

    panel.findChild<QToolButton*>(QStringLiteral("PanelClose"))->click();

    QCOMPARE(asked.count(), 1);

    panel.Summon(false);
    QVERIFY(panel.isHidden());

    panel.Summon(true);
    QVERIFY(!panel.isHidden());
}

void ContextPanelTest::ContentTallerThanThePanelScrollsInsteadOfBeingSquashed()
{
    constexpr int kTallerThanAnyWindow = 2000;

    QWidget window;
    auto* beside = new QHBoxLayout(&window);
    beside->setContentsMargins(0, 0, 0, 0);

    auto* panel = new ContextPanel(QStringLiteral("Addon selected"), 380, &window);
    panel->setObjectName(QStringLiteral("scroll-test"));
    beside->addWidget(panel);

    auto* tall = new QLabel(QStringLiteral("a block with more dependencies than the panel is high"));
    tall->setMinimumHeight(kTallerThanAnyWindow);
    panel->Add(tall);

    window.setFixedHeight(300);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QCOMPARE(window.height(), 300);
    QCOMPARE(tall->height(), kTallerThanAnyWindow);

    const auto* scrolled = panel->findChild<QScrollArea*>();
    QVERIFY2(scrolled != nullptr, "content taller than the panel would be clipped with no way to reach it");
    QVERIFY(scrolled->verticalScrollBar()->maximum() > 0);
}

namespace
{
    QString ADeepPath()
    {
        return QStringLiteral("C:\\Users\\bruno\\AppData\\Roaming\\SayIntentionsAI\\SayIntentionsAI\\si-flowpro\\"
                              "p42-util-flow-SayIntentionsAI-widget");
    }

    struct PanelShowingAPath
    {
        QWidget window;
        ContextPanel* panel = nullptr;
        ModelRowDetail* detail = nullptr;

        PanelShowingAPath()
        {
            auto* beside = new QHBoxLayout(&window);
            beside->setContentsMargins(0, 0, 0, 0);

            panel = new ContextPanel(QStringLiteral("Entry selected"), 380, &window);
            panel->setObjectName(QStringLiteral("deep-path-test"));

            detail = new ModelRowDetail(panel);
            panel->Add(detail);
            beside->addWidget(panel);

            window.setFixedSize(380, 380);
            window.show();
            static_cast<void>(QTest::qWaitForWindowExposed(&window));

            detail->ShowFields({{QStringLiteral("Came from"), ADeepPath()}});
            QCoreApplication::processEvents();
        }
    };
}

void ContextPanelTest::APathWiderThanThePanelNeverAsksForMoreRoomThanItHas()
{
    const PanelShowingAPath shown;

    const auto* value = shown.detail->findChild<QTextEdit*>(QStringLiteral("UncutText"));
    QVERIFY(value != nullptr);
    QVERIFY(value->viewport()->width() > 0);
    QVERIFY2(value->minimumSizeHint().width() <= value->width(),
             "the path asks the layout for more room than the panel has");
    QVERIFY2(value->document()->size().width() <= value->viewport()->width(),
             "a line of the path is wider than the room it was given, so its far end is cut at the border");
}

void ContextPanelTest::AFieldHandsOutExactlyTheTextItShows()
{
    const PanelShowingAPath shown;

    auto* value = shown.detail->findChild<QTextEdit*>(QStringLiteral("UncutText"));
    QVERIFY(value != nullptr);

    value->selectAll();

    QCOMPARE(value->textCursor().selectedText(), ADeepPath());
}

void ContextPanelTest::SelectingAPathPaintsItOnAGroundThatIsNotTheSurfaceUnderIt()
{
    const PanelShowingAPath shown;

    auto* value = shown.detail->findChild<QTextEdit*>(QStringLiteral("UncutText"));
    QVERIFY(value != nullptr);

    const auto painted = [value]
    {
        QPixmap shot(value->size());
        value->render(&shot);

        return shot.toImage();
    };

    const QColor marking = TonesOf(CurrentColorScheme()).accent;

    QCOMPARE(PixelsOf(painted(), marking), 0);

    value->selectAll();
    QCoreApplication::processEvents();

    QVERIFY2(PixelsOf(painted(), marking) > 0,
             "selected text is grounded in a surface tone, so on a dark scheme nothing tells it apart");
}

QTEST_MAIN(ContextPanelTest)

#include "tst_context_panel.moc"
