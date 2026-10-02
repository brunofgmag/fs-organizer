#include <QtTest/QtTest>

#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>

#include <filesystem>
#include <optional>
#include <string>

#include "support/PathText.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/PathPrinting.h"
#include "view/simulator/StartupDraftDialog.h"
#include "view/theme/ModernistTheme.h"

namespace
{
    class StartupDraftDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void WithNoProgramChosenTheDialogSaysSoAndWillNotAdd();
        static void AProgramOutsideTheLibraryFillsTheNameAndSaysNothingMore();
        static void TheNameFollowsTheFileUntilTheUserTypesOne();
        static void AProgramInsideAnAddonSaysTheLinkPathOnALineOfItsOwn();
        static void AnAddonThatIsOffNowIsSaidOnItsOwnLineAndStillAllowsTheAdd();
        static void AProgramTheFileAlreadyListsIsRefusedWithTheNameOfTheOccupant();
        static void AProgramThatDoesNotExistIsRefused();
        static void ARefusalHidesEveryOtherLine();
        static void AnEditThatChangesThePathSaysWhatHappensToThePresetsAndToTheOldEntry();
        static void AnEditThatKeepsThePathSaysNothingMore();
        static void TheDialogOpensOnWhatIsBeingEdited();
        static void TheDraftCarriesTheChosenFileTheNameAndTheCommandLineTrimmed();
        static void ThePathOnlyEntersThroughTheChooserAndTheDialogIsSixHundredTwentyWide();
    };

    const std::filesystem::path kProgram = "D:/Tools/SimToolkitPro/SimToolkitPro.exe";
    const std::filesystem::path kInTheLibrary = "D:/MSFS Library/Utilities/p42-util-flow-pro/Flow for MSFS2024.exe";
    const std::filesystem::path kByTheLink =
        "E:/Flight Simulator 2024/Community/p42-util-flow-pro/Flow for MSFS2024.exe";

    StartupDraftCheck Outside(const std::filesystem::path& file)
    {
        return StartupDraftCheck{.pathToWrite = file};
    }

    StartupDraftCheck Inside(const bool addonIsOff)
    {
        return StartupDraftCheck{.pathToWrite = kByTheLink,
                                 .insideAnAddon = true,
                                 .addonFolderName = "p42-util-flow-pro",
                                 .addonIsOff = addonIsOff};
    }

    StartupDraftCheck Refused(const FileResult refusal, const std::string& occupiedBy = {})
    {
        return StartupDraftCheck{.pathToWrite = kProgram, .refusal = refusal, .occupiedBy = occupiedBy};
    }

    struct Scene
    {
        explicit Scene(const std::optional<StartupDraft>& editing = std::nullopt)
            : dialog(
                  [this](const std::filesystem::path&)
                  {
                      return check;
                  },
                  editing)
        {
            ApplyModernistTheme(*qApp);
        }

        StartupDraftCheck check{};
        StartupDraftDialog dialog;
    };

    QStringList WhatItSays(const QDialog& dialog)
    {
        QStringList said;

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            const bool aLine = label->objectName() == QLatin1String("PanelPromise")
                || label->objectName() == QLatin1String("EntryRefusal");

            if (aLine && !label->isHidden() && !label->text().isEmpty())
            {
                said << label->text();
            }
        }

        return said;
    }

    QStringList UncutTexts(const QDialog& dialog)
    {
        QStringList texts;

        for (const QTextEdit* text : dialog.findChildren<QTextEdit*>(QStringLiteral("UncutText")))
        {
            texts << text->toPlainText();
        }

        return texts;
    }

    QPushButton* Confirm(const QDialog& dialog, const QString& text)
    {
        return ButtonSaying(dialog, text);
    }
}

void StartupDraftDialogTest::WithNoProgramChosenTheDialogSaysSoAndWillNotAdd()
{
    Scene scene;

    QCOMPARE(scene.dialog.windowTitle(), QStringLiteral("Add startup entry"));
    QCOMPARE(WhatItSays(scene.dialog), (QStringList{"No program chosen"}));
    QVERIFY(UncutTexts(scene.dialog).isEmpty());
    QVERIFY(!Confirm(scene.dialog, "Add")->isEnabled());
    QVERIFY(Confirm(scene.dialog, "Cancel")->isEnabled());
    QVERIFY(scene.dialog.Draft().file.empty());
    QCOMPARE(scene.dialog.findChild<QLineEdit*>(QStringLiteral("EntryCommandLine"))->placeholderText(),
             QStringLiteral("Optional"));
}

void StartupDraftDialogTest::AProgramOutsideTheLibraryFillsTheNameAndSaysNothingMore()
{
    Scene scene;
    scene.check = Outside(kProgram);

    scene.dialog.TakeTheProgram(kProgram);

    QVERIFY(WhatItSays(scene.dialog).isEmpty());
    QCOMPARE(UncutTexts(scene.dialog), (QStringList{AsText(kProgram)}));
    QCOMPARE(scene.dialog.findChild<QLineEdit*>(QStringLiteral("EntryName"))->text(), QStringLiteral("SimToolkitPro"));
    QVERIFY(Confirm(scene.dialog, "Add")->isEnabled());
}

void StartupDraftDialogTest::TheNameFollowsTheFileUntilTheUserTypesOne()
{
    Scene scene;
    auto* name = scene.dialog.findChild<QLineEdit*>(QStringLiteral("EntryName"));

    scene.check = Outside(kProgram);
    scene.dialog.TakeTheProgram(kProgram);
    scene.dialog.TakeTheProgram("D:/Tools/Other/Other.exe");

    QCOMPARE(name->text(), QStringLiteral("Other"));

    QTest::keyClicks(name, "!");
    scene.dialog.TakeTheProgram("D:/Tools/Third/Third.exe");

    QCOMPARE(name->text(), QStringLiteral("Other!"));

    name->clear();

    QVERIFY2(!Confirm(scene.dialog, "Add")->isEnabled(), "an entry with no name would be a blank row");
}

void StartupDraftDialogTest::AProgramInsideAnAddonSaysTheLinkPathOnALineOfItsOwn()
{
    Scene scene;
    scene.check = Inside(false);

    scene.dialog.TakeTheProgram(kInTheLibrary);

    QCOMPARE(WhatItSays(scene.dialog),
             (QStringList{"Inside the addon p42-util-flow-pro. The entry is written with the path of the addon's "
                          "link:"}));
    QCOMPARE(UncutTexts(scene.dialog), (QStringList{AsText(kInTheLibrary), AsText(kByTheLink)}));
    QVERIFY(Confirm(scene.dialog, "Add")->isEnabled());
}

void StartupDraftDialogTest::AnAddonThatIsOffNowIsSaidOnItsOwnLineAndStillAllowsTheAdd()
{
    Scene scene;
    scene.check = Inside(true);

    scene.dialog.TakeTheProgram(kInTheLibrary);

    const QStringList said = WhatItSays(scene.dialog);

    QCOMPARE(said.size(), 2);
    QVERIFY(said.at(0).startsWith(QStringLiteral("Inside the addon p42-util-flow-pro.")));
    QCOMPARE(said.at(1),
             QStringLiteral("The addon is disabled now, so the program will not start until you enable it."));
    QVERIFY(Confirm(scene.dialog, "Add")->isEnabled());
}

void StartupDraftDialogTest::AProgramTheFileAlreadyListsIsRefusedWithTheNameOfTheOccupant()
{
    Scene scene;
    scene.check = Refused(FileResult::TheStartupEntryIsAlreadyThere, "Couatl");

    scene.dialog.TakeTheProgram(kProgram);

    QCOMPARE(WhatItSays(scene.dialog), (QStringList{"The startup file already lists this program, as Couatl."}));

    const auto* refusal = scene.dialog.findChild<QLabel*>(QStringLiteral("EntryRefusal"));

    QVERIFY(refusal != nullptr);
    QVERIFY(!refusal->isHidden());
    QVERIFY(!Confirm(scene.dialog, "Add")->isEnabled());
}

void StartupDraftDialogTest::AProgramThatDoesNotExistIsRefused()
{
    Scene scene;
    scene.check = Refused(FileResult::TheProgramDoesNotExist);

    scene.dialog.TakeTheProgram(kProgram);

    QCOMPARE(WhatItSays(scene.dialog), (QStringList{"That file does not exist."}));
    QVERIFY(!Confirm(scene.dialog, "Add")->isEnabled());

    scene.check = Outside(kProgram);
    scene.dialog.TakeTheProgram(kProgram);

    QVERIFY2(Confirm(scene.dialog, "Add")->isEnabled(), "choosing a file that exists lifts the refusal");
    QVERIFY(WhatItSays(scene.dialog).isEmpty());
}

void StartupDraftDialogTest::ARefusalHidesEveryOtherLine()
{
    Scene scene;
    scene.check = Inside(true);
    scene.check.refusal = FileResult::TheStartupEntryIsAlreadyThere;
    scene.check.occupiedBy = "Flow";

    scene.dialog.TakeTheProgram(kInTheLibrary);

    QCOMPARE(WhatItSays(scene.dialog), (QStringList{"The startup file already lists this program, as Flow."}));
    QCOMPARE(UncutTexts(scene.dialog), (QStringList{AsText(kInTheLibrary)}));
}

void StartupDraftDialogTest::AnEditThatChangesThePathSaysWhatHappensToThePresetsAndToTheOldEntry()
{
    Scene scene(StartupDraft{.label = "FS2Crew Command Center", .file = kProgram, .commandLine = "-minimized"});
    scene.check = Outside("D:/FS2Crew/Command Center/FS2CrewCommandCenter.exe");
    scene.check.changesThePath = true;
    scene.check.presetsNamingTheEntry = 2;

    scene.dialog.TakeTheProgram(scene.check.pathToWrite);

    const QStringList said = WhatItSays(scene.dialog);

    QCOMPARE(said.size(), 2);
    QVERIFY(said.at(0).startsWith(QStringLiteral("2 ")));
    QVERIFY(said.at(0).contains(QStringLiteral("will follow the new path.")));
    QCOMPARE(said.at(1), QStringLiteral("The entry as it is now goes to Removed."));
    QVERIFY(Confirm(scene.dialog, "Save")->isEnabled());

    scene.check.presetsNamingTheEntry = 0;
    scene.dialog.TakeTheProgram(scene.check.pathToWrite);

    QCOMPARE(WhatItSays(scene.dialog), (QStringList{"The entry as it is now goes to Removed."}));
}

void StartupDraftDialogTest::AnEditThatKeepsThePathSaysNothingMore()
{
    Scene scene(StartupDraft{.label = "Simlink", .file = kProgram});
    scene.check = Outside(kProgram);
    scene.check.presetsNamingTheEntry = 3;

    scene.dialog.TakeTheProgram(kProgram);

    QVERIFY(WhatItSays(scene.dialog).isEmpty());
    QVERIFY(Confirm(scene.dialog, "Save")->isEnabled());
    QCOMPARE(scene.dialog.windowTitle(), QStringLiteral("Edit startup entry"));
}

void StartupDraftDialogTest::TheDialogOpensOnWhatIsBeingEdited()
{
    Scene scene(StartupDraft{.label = "Flow Manager", .file = kProgram, .commandLine = "--quiet"});
    scene.check = Outside(kProgram);

    const StartupDraft draft = scene.dialog.Draft();

    QCOMPARE(draft.label, std::string("Flow Manager"));
    QCOMPARE(draft.file, kProgram);
    QCOMPARE(draft.commandLine, std::string("--quiet"));
    QCOMPARE(UncutTexts(scene.dialog), (QStringList{AsText(kProgram)}));
}

void StartupDraftDialogTest::TheDraftCarriesTheChosenFileTheNameAndTheCommandLineTrimmed()
{
    Scene scene;
    scene.check = Outside(kProgram);
    scene.dialog.TakeTheProgram(kProgram);

    scene.dialog.findChild<QLineEdit*>(QStringLiteral("EntryName"))->setText(QStringLiteral("  Toolkit  "));
    scene.dialog.findChild<QLineEdit*>(QStringLiteral("EntryCommandLine"))->setText(QStringLiteral(" -a \"b c\" "));

    const StartupDraft draft = scene.dialog.Draft();

    QCOMPARE(draft.label, std::string("Toolkit"));
    QCOMPARE(draft.file, kProgram);
    QCOMPARE(draft.commandLine, std::string("-a \"b c\""));
}

void StartupDraftDialogTest::ThePathOnlyEntersThroughTheChooserAndTheDialogIsSixHundredTwentyWide()
{
    Scene scene;
    scene.check = Inside(true);
    scene.dialog.TakeTheProgram(kInTheLibrary);
    scene.dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&scene.dialog));
    QTest::qWait(50);

    const QList<QLineEdit*> fields = scene.dialog.findChildren<QLineEdit*>();

    QCOMPARE(fields.size(), 2);
    QVERIFY(ButtonSaying(scene.dialog, "Choose…") != nullptr);
    QVERIFY(!ButtonSaying(scene.dialog, "Choose…")->autoDefault());
    QCOMPARE(scene.dialog.width(), 620);
}

QTEST_MAIN(StartupDraftDialogTest)

#include "tst_startup_draft_dialog.moc"
