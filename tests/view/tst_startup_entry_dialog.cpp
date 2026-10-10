#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <filesystem>
#include <vector>

#include "support/PathText.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/FixedHeadings.h"
#include "view/theme/ModernistTheme.h"
#include "tests/support/PathPrinting.h"
#include "view/library/StartupEntryDialog.h"

namespace
{
    class StartupEntryDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void EveryEntryAtRiskIsNamedWithTheProgramAndWhatItLaunches();
        static void TurningBothOffAcceptsAndTurningOnlyTheAddonOffRejects();
        static void TheButtonCountsTheEntriesInsteadOfAlwaysSayingTwo();
        static void TheHeadingsLeaveTheScrollAndStandOverTheirColumns();
        static void ScrollingTheListLeavesTheHeadingsWhereTheyAre();
        static void TwoRulesFrameTheListAndTheListHasNoFrame();
    };

    const std::filesystem::path kLoader = "E:/Sim/Community/pmdg-aircraft-77w/bin/loader.exe";
    const std::filesystem::path kUpdater = "E:/Sim/Community/pmdg-aircraft-77w/tools/updater.exe";

    StartupLine Line(const std::string& label, const std::filesystem::path& path)
    {
        return StartupLine{.label = label,
                           .path = path,
                           .enabled = true,
                           .reach = StartupReach::InsideAnAddon,
                           .addonFolder = "E:/Sim/Community/pmdg-aircraft-77w"};
    }

    std::vector<StartupLine> ManyEntries(const int count)
    {
        std::vector<StartupLine> lines;
        lines.reserve(static_cast<std::size_t>(count));

        for (int at = 0; at < count; ++at)
        {
            lines.push_back(Line("PMDG Operations Center " + std::to_string(at), kLoader));
        }

        return lines;
    }

    struct Window
    {
        explicit Window(const int tall)
        {
            parent.resize(900, tall);
        }

        QWidget parent;
    };

    QStringList TextsOf(const QDialog& dialog)
    {
        QStringList said;
        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said;
    }
}

void StartupEntryDialogTest::EveryEntryAtRiskIsNamedWithTheProgramAndWhatItLaunches()
{
    const StartupEntryDialog dialog({Line("PMDG Operations Center", kLoader), Line("PMDG Updater", kUpdater)});

    const QStringList said = TextsOf(dialog);

    QVERIFY(said.contains(QStringLiteral("PMDG Operations Center")));
    QVERIFY(said.contains(QStringLiteral("PMDG Updater")));
    QVERIFY(said.contains(AsText(kLoader)));
    QVERIFY(said.contains(AsText(kUpdater)));
}

void StartupEntryDialogTest::TurningBothOffAcceptsAndTurningOnlyTheAddonOffRejects()
{
    StartupEntryDialog agreeing({Line("PMDG Operations Center", kLoader)});
    QSignalSpy accepted(&agreeing, &QDialog::accepted);
    ButtonSaying(agreeing, QStringLiteral("Disable the addon and the entry"))->click();
    QCOMPARE(accepted.count(), 1);
    QCOMPARE(agreeing.result(), static_cast<int>(QDialog::Accepted));

    StartupEntryDialog refusing({Line("PMDG Operations Center", kLoader)});
    QSignalSpy rejected(&refusing, &QDialog::rejected);
    ButtonSaying(refusing, QStringLiteral("Disable only the addon"))->click();
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(refusing.result(), static_cast<int>(QDialog::Rejected));
}

void StartupEntryDialogTest::TheButtonCountsTheEntriesInsteadOfAlwaysSayingTwo()
{
    const StartupEntryDialog alone({Line("PMDG Operations Center", kLoader)});
    QVERIFY(ButtonSaying(alone, QStringLiteral("Disable the addon and the entry")) != nullptr);

    const StartupEntryDialog several({Line("PMDG Operations Center", kLoader), Line("PMDG Updater", kUpdater)});
    QVERIFY(ButtonSaying(several, QStringLiteral("Disable the addon and the entries")) != nullptr);
}

void StartupEntryDialogTest::TheHeadingsLeaveTheScrollAndStandOverTheirColumns()
{
    ApplyModernistTheme(*qApp);
    Window window(1600);
    StartupEntryDialog dialog(ManyEntries(3), &window.parent);
    ShowItSettled(dialog);

    VerifyTheHeadingsAreOutsideTheScroll(dialog, {QStringLiteral("Program"), QStringLiteral("Launches")});
    VerifyEachHeadingStandsOverItsColumn(dialog);
}

void StartupEntryDialogTest::ScrollingTheListLeavesTheHeadingsWhereTheyAre()
{
    ApplyModernistTheme(*qApp);
    Window window(400);
    StartupEntryDialog dialog(ManyEntries(30), &window.parent);
    ShowItSettled(dialog);

    VerifyTheListScrollsForRealAndTheHeadingsStay(dialog);
}

void StartupEntryDialogTest::TwoRulesFrameTheListAndTheListHasNoFrame()
{
    ApplyModernistTheme(*qApp);
    Window window(1600);
    StartupEntryDialog dialog(ManyEntries(3), &window.parent);
    ShowItSettled(dialog);

    VerifyTwoRulesFrameTheList(dialog);
}

QTEST_MAIN(StartupEntryDialogTest)

#include "tst_startup_entry_dialog.moc"
