#include <QtTest/QtTest>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>

#include <filesystem>

#include "application/model/ConflictDetails.h"
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

    QAbstractButton* ButtonSaying(const ConflictDialog& dialog, const QString& wanted)
    {
        for (QAbstractButton* button : dialog.findChildren<QAbstractButton*>())
        {
            if (button->text().contains(wanted))
            {
                return button;
            }
        }

        return nullptr;
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
    QAbstractButton* keepTheirs = ButtonSaying(fromAnotherProgram, QStringLiteral("other program's one"));
    QVERIFY(keepTheirs != nullptr);
    keepTheirs->click();
    QCOMPARE(fromAnotherProgram.Choice(), ConflictChoice::KeepTheProvenanceCopy);

    ConflictDialog ordinary(AnOrdinaryConflict());
    QAbstractButton* keepDestination = ButtonSaying(ordinary, QStringLiteral("destination one"));
    QVERIFY(keepDestination != nullptr);
    keepDestination->click();
    QCOMPARE(ordinary.Choice(), ConflictChoice::KeepTheProvenanceCopy);
}

QTEST_MAIN(ConflictDialogTest)

#include "tst_conflict_dialog.moc"
