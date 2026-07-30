#include <QtCore/QSettings>
#include <QtGui/QStandardItemModel>
#include <QtTest/QtTest>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "view/panels/ContextPanel.h"
#include "view/panels/ModelRowDetail.h"
#include "view/panels/PanelRail.h"
#include "view/shell/TriageStrip.h"

class ContextPanelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    static void CollapsingHidesTheContentAndExpandingBringsItBack();
    static void ACollapsedPanelBecomesARailThatReachesTheBottom();
    static void TheRailBringsThePanelBack();
    static void TheCollapsedStateSurvivesANewPanelWithTheSameName();
    static void TheDetailShowsEveryColumnOfTheSelectedRow();
    static void AnInvalidIndexClearsTheDetailToItsPlaceholder();
    static void TheStripKeepsQuietUntilSomethingBreaks();
    static void ADuplicatedAddonGetsItsOwnItemAndAsksToBeSeen();
    static void ClosingThePanelAsksForIt();
};

void ContextPanelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("fs-organizer-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("context-panel"));
    QSettings().clear();
}

void ContextPanelTest::CollapsingHidesTheContentAndExpandingBringsItBack()
{
    ContextPanel panel(QStringLiteral("Atenção"));
    panel.setObjectName(QStringLiteral("collapse-test"));

    auto* body = new QLabel(QStringLiteral("conteúdo"));
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
    ContextPanel panel(QStringLiteral("Addon selecionado"));
    panel.setObjectName(QStringLiteral("rail-height-test"));
    panel.Add(new QLabel(QStringLiteral("conteúdo")));
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
    ContextPanel panel(QStringLiteral("Addon selecionado"));
    panel.setObjectName(QStringLiteral("rail-expand-test"));

    auto* body = new QLabel(QStringLiteral("conteúdo"));
    panel.Add(body);

    panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"))->click();
    QVERIFY(!body->isVisibleTo(&panel));

    panel.findChild<QToolButton*>(QStringLiteral("PanelExpand"))->click();

    QVERIFY(body->isVisibleTo(&panel));
    QCOMPARE(panel.width(), 380);
}

void ContextPanelTest::TheCollapsedStateSurvivesANewPanelWithTheSameName()
{
    {
        ContextPanel panel(QStringLiteral("Atenção"));
        panel.setObjectName(QStringLiteral("persist-test"));
        panel.findChild<QToolButton*>(QStringLiteral("PanelToggle"))->click();
    }

    ContextPanel reborn(QStringLiteral("Atenção"));
    reborn.setObjectName(QStringLiteral("persist-test"));
    reborn.RestoreCollapsedState();

    auto* body = new QLabel(QStringLiteral("conteúdo"));
    reborn.Add(body);

    QVERIFY(!body->isVisibleTo(&reborn));
}

void ContextPanelTest::TheDetailShowsEveryColumnOfTheSelectedRow()
{
    QStandardItemModel model(2, 2);
    model.setHorizontalHeaderLabels({QStringLiteral("Nome"), QStringLiteral("Destino")});
    model.setItem(1, 0, new QStandardItem(QStringLiteral("airport-bgqq-qaanaaq")));
    model.setItem(1, 1, new QStandardItem(QStringLiteral("Community")));

    ModelRowDetail detail;
    detail.Show(model.index(1, 0));

    const QList<QLabel*> labels = detail.findChildren<QLabel*>();
    QStringList texts;
    for (const QLabel* label : labels)
    {
        texts.append(label->text());
    }

    QVERIFY(texts.contains(QStringLiteral("Nome")));
    QVERIFY(texts.contains(QStringLiteral("airport-bgqq-qaanaaq")));
    QVERIFY(texts.contains(QStringLiteral("Destino")));
    QVERIFY(texts.contains(QStringLiteral("Community")));
}

void ContextPanelTest::AnInvalidIndexClearsTheDetailToItsPlaceholder()
{
    QStandardItemModel model(1, 1);
    model.setItem(0, 0, new QStandardItem(QStringLiteral("linha")));

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

    strip.ShowBreakdown(0, 0, 0, 0);
    QVERIFY(!strip.HasAnythingToSay());

    strip.ShowBreakdown(0, 0, 0, 178);
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

    strip.ShowBreakdown(0, 0, 2, 0);
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
    ContextPanel panel(QStringLiteral("Entrada selecionada"));
    panel.setObjectName(QStringLiteral("close-test"));

    QSignalSpy asked(&panel, &ContextPanel::CloseRequested);

    panel.findChild<QToolButton*>(QStringLiteral("PanelClose"))->click();

    QCOMPARE(asked.count(), 1);

    panel.Summon(false);
    QVERIFY(panel.isHidden());

    panel.Summon(true);
    QVERIFY(!panel.isHidden());
}

QTEST_MAIN(ContextPanelTest)

#include "tst_context_panel.moc"
