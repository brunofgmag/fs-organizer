#include <QtTest/QtTest>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>

#include <filesystem>

#include "application/model/ConflictDetails.h"
#include "tests/support/ButtonLookup.h"
#include "tests/support/EnumPrinting.h"
#include "tests/support/PathPrinting.h"
#include "view/community/ConflictDialog.h"

namespace
{
    class ConflictDialogTest : public QObject
    {
        Q_OBJECT

    private slots:
        static void AConflictWithTheOtherProgramNeverCallsItTheDestination();
        static void AnOrdinaryConflictStillCallsItTheDestination();
        static void TheWarningAboutTheLinksFollowsTheSameWording();
        static void KeepingTheProvenanceCopyAnswersTheSameChoiceInBothWordings();
        static void ADeepPathOnOneSideLeavesTheTwoSidesTheSameWidth();
        static void AReplacedLinkIsToldApartFromTwoCopiesThatMerelyShareAName();
        static void TheTakeBackIsOfferedWhenTheVersionsDoNotTellTheCopiesApart();
        static void AnOlderCopyInTheDestinationIsNotOfferedForTakingBack();
    };

    const std::filesystem::path kOtherProgramsFolder = "C:/Addon Manager/Aircraft/aerosoft-crj";
    const std::filesystem::path kInTheDestination = "E:/Sim/Community/aerosoft-crj";
    const std::filesystem::path kInTheLibrary = "D:/Library/Aircrafts/aerosoft-crj";

    ConflictDetails AConflictFromAnotherProgram()
    {
        return ConflictDetails{.provenance = ConflictSide{.path = kOtherProgramsFolder},
                               .library = ConflictSide{.path = kInTheLibrary},
                               .linksToTheLibraryCopy = {},
                               .theProvenanceIsAnotherProgram = true};
    }

    ConflictDetails ALinkSomethingReplaced(const std::string& atTheDestination, const std::string& inTheLibrary)
    {
        return ConflictDetails{
            .provenance =
                ConflictSide{.path = kInTheDestination, .manifest = Manifest{.packageVersion = atTheDestination}},
            .library = ConflictSide{.path = kInTheLibrary, .manifest = Manifest{.packageVersion = inTheLibrary}},
            .linksToTheLibraryCopy = {},
            .theProvenanceIsAnotherProgram = false,
            .ourLinkWasReplaced = true};
    }

    ConflictDetails AnOrdinaryConflict()
    {
        return ConflictDetails{.provenance = ConflictSide{.path = kInTheDestination},
                               .library = ConflictSide{.path = kInTheLibrary},
                               .linksToTheLibraryCopy = {},
                               .theProvenanceIsAnotherProgram = false};
    }

    QStringList EverythingWritten(const ConflictDialog& dialog)
    {
        QStringList said;

        for (const QLabel* label : dialog.findChildren<QLabel*>())
        {
            said.append(label->text());
        }

        for (const QGroupBox* group : dialog.findChildren<QGroupBox*>())
        {
            said.append(group->title());
        }

        for (const QAbstractButton* button : dialog.findChildren<QAbstractButton*>())
        {
            said.append(button->text());
        }

        return said;
    }

    QString AllOfIt(const ConflictDialog& dialog)
    {
        return EverythingWritten(dialog).join(QStringLiteral("\n"));
    }
}

void ConflictDialogTest::AConflictWithTheOtherProgramNeverCallsItTheDestination()
{
    const ConflictDialog dialog(AConflictFromAnotherProgram());

    const QString said = AllOfIt(dialog);

    QVERIFY2(said.contains(QStringLiteral("other program")),
             qPrintable(QStringLiteral("nothing named the other program:\n%1").arg(said)));
    QVERIFY2(!said.contains(QStringLiteral("destination")),
             qPrintable(QStringLiteral("the dialog still says destination:\n%1").arg(said)));
}

void ConflictDialogTest::AnOrdinaryConflictStillCallsItTheDestination()
{
    const ConflictDialog dialog(AnOrdinaryConflict());

    const QString said = AllOfIt(dialog);

    QVERIFY2(said.contains(QStringLiteral("destination")),
             qPrintable(QStringLiteral("the dialog stopped naming the destination:\n%1").arg(said)));
    QVERIFY2(!said.contains(QStringLiteral("other program")),
             qPrintable(QStringLiteral("an ordinary conflict named the other program:\n%1").arg(said)));
}

void ConflictDialogTest::TheWarningAboutTheLinksFollowsTheSameWording()
{
    ConflictDetails details = AConflictFromAnotherProgram();
    details.linksToTheLibraryCopy = {kInTheDestination};

    const ConflictDialog dialog(details);

    const QString said = AllOfIt(dialog);

    QVERIFY2(said.contains(QStringLiteral("Keeping the other program's one")),
             qPrintable(QStringLiteral("the warning kept the destination wording:\n%1").arg(said)));
}

void ConflictDialogTest::KeepingTheProvenanceCopyAnswersTheSameChoiceInBothWordings()
{
    ConflictDialog fromAnotherProgram(AConflictFromAnotherProgram());
    QPushButton* keepTheirs = ButtonContaining(fromAnotherProgram, QStringLiteral("other program's one"));
    QVERIFY(keepTheirs != nullptr);
    keepTheirs->click();
    QCOMPARE(fromAnotherProgram.Choice(), ConflictChoice::KeepTheProvenanceCopy);

    ConflictDialog ordinary(AnOrdinaryConflict());
    QPushButton* keepDestination = ButtonContaining(ordinary, QStringLiteral("destination one"));
    QVERIFY(keepDestination != nullptr);
    keepDestination->click();
    QCOMPARE(ordinary.Choice(), ConflictChoice::KeepTheProvenanceCopy);
}

void ConflictDialogTest::ADeepPathOnOneSideLeavesTheTwoSidesTheSameWidth()
{
    ConflictDetails details = AConflictFromAnotherProgram();
    details.provenance.path = "C:/Users/bruno/AppData/Roaming/SayIntentionsAI/SayIntentionsAI/si-flowpro/"
                              "p42-util-flow-SayIntentionsAI-widget";

    ConflictDialog dialog(details);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    const QList<QGroupBox*> sides = dialog.findChildren<QGroupBox*>();
    QCOMPARE(sides.size(), 2);
    QVERIFY2(sides.first()->width() == sides.last()->width(),
             "the deeper path pushed its own column, so a dialog whose job is to compare shows two unequal halves");
}

void ConflictDialogTest::AReplacedLinkIsToldApartFromTwoCopiesThatMerelyShareAName()
{
    const ConflictDialog dialog(ALinkSomethingReplaced("0.1.0", "0.1.0"));

    const QString said = AllOfIt(dialog);

    QVERIFY2(said.contains(QStringLiteral("link")),
             qPrintable(QStringLiteral("nothing said the link was what got replaced: %1").arg(said)));
    QVERIFY2(!said.contains(QStringLiteral("other program")),
             qPrintable(QStringLiteral("no other program handed this folder over: %1").arg(said)));
}

void ConflictDialogTest::TheTakeBackIsOfferedWhenTheVersionsDoNotTellTheCopiesApart()
{
    const ConflictDialog dialog(ALinkSomethingReplaced("0.1.0", "0.1.0"));

    QPushButton* takeItBack = ButtonContaining(dialog, QStringLiteral("into the library"));

    QVERIFY(takeItBack != nullptr);
    QVERIFY2(!takeItBack->isHidden(),
             "a package rewritten every cycle under a fixed version would never be offered the way back");
    QVERIFY2(AllOfIt(dialog).contains(QStringLiteral("same version")),
             "offering the gesture without saying the versions settle nothing lets the user read the offer as a "
             "verdict on which copy is newer");
}

void ConflictDialogTest::AnOlderCopyInTheDestinationIsNotOfferedForTakingBack()
{
    const ConflictDialog dialog(ALinkSomethingReplaced("2.9.1", "2.26.16"));

    QPushButton* takeItBack = ButtonContaining(dialog, QStringLiteral("into the library"));

    QVERIFY(takeItBack != nullptr);
    QVERIFY2(takeItBack->isHidden(), "carrying an older copy over a newer one is not a gesture worth offering");
    QVERIFY2(ButtonContaining(dialog, QStringLiteral("Put the link back")) != nullptr,
             "with the take back gone there has to be a way out that is not the close button");
}

QTEST_MAIN(ConflictDialogTest)

#include "tst_conflict_dialog.moc"
