#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <filesystem>
#include <vector>

#include "support/PathText.h"
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
    };

    const std::filesystem::path kLoader = "E:/Sim/Community/pmdg-aircraft-77w/bin/loader.exe";
    const std::filesystem::path kUpdater = "E:/Sim/Community/pmdg-aircraft-77w/tools/updater.exe";

    StartupLine Line(const std::string& label, const std::filesystem::path& path)
    {
        return StartupLine{.label = label,
                           .path = path,
                           .enabled = true,
                           .reach = StartupReach::InsideAnAddon,
                           .alarm = StartupAlarm::None,
                           .addonFolder = "E:/Sim/Community/pmdg-aircraft-77w"};
    }

    QStringList TextsOf(const QDialog& dialog)
    {
        QStringList said;
        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        return said;
    }

    QPushButton* ButtonSaying(const QDialog& dialog, const QString& text)
    {
        for (QPushButton* button : dialog.findChildren<QPushButton*>())
        {
            if (button->text().remove(QLatin1Char('&')) == text)
            {
                return button;
            }
        }

        return nullptr;
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
    ButtonSaying(agreeing, QStringLiteral("Turn the addon and the entry off"))->click();
    QCOMPARE(accepted.count(), 1);
    QCOMPARE(agreeing.result(), static_cast<int>(QDialog::Accepted));

    StartupEntryDialog refusing({Line("PMDG Operations Center", kLoader)});
    QSignalSpy rejected(&refusing, &QDialog::rejected);
    ButtonSaying(refusing, QStringLiteral("Only turn the addon off"))->click();
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(refusing.result(), static_cast<int>(QDialog::Rejected));
}

void StartupEntryDialogTest::TheButtonCountsTheEntriesInsteadOfAlwaysSayingTwo()
{
    const StartupEntryDialog alone({Line("PMDG Operations Center", kLoader)});
    QVERIFY(ButtonSaying(alone, QStringLiteral("Turn the addon and the entry off")) != nullptr);

    const StartupEntryDialog several({Line("PMDG Operations Center", kLoader), Line("PMDG Updater", kUpdater)});
    QVERIFY(ButtonSaying(several, QStringLiteral("Turn the addon and the entries off")) != nullptr);
}

QTEST_MAIN(StartupEntryDialogTest)

#include "tst_startup_entry_dialog.moc"
