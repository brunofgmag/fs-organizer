#include "view/shell/StartupOffers.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "application/Session.h"
#include "support/PathText.h"
#include "view/legacy/LegacyImportDialog.h"
#include "view/setup/StagingLeftoverDialog.h"
#include "viewmodel/BisectionViewModel.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/LegacyImportViewModel.h"

void OfferToCarryOnTheSearchThatWasLeftHalfway(BisectionViewModel& bisectionViewModel, QWidget* parent)
{
    if (!bisectionViewModel.AProcedureWasInterrupted())
    {
        return;
    }

    QMessageBox question(
        QMessageBox::Question, QObject::tr("A search for the culprit was left halfway"),
        QObject::tr("The addons of this profile are as the last round of it left them, and not as they were before it "
                    "started. Nothing was decided yet."),
        QMessageBox::NoButton, parent);

    const QPushButton* carryOn = question.addButton(QObject::tr("Carry on from that round"), QMessageBox::AcceptRole);
    const QPushButton* putBack =
        question.addButton(QObject::tr("Put back what was on before it started"), QMessageBox::DestructiveRole);
    const QPushButton* forget =
        question.addButton(QObject::tr("Forget it and leave the addons as they are"), QMessageBox::DestructiveRole);
    question.exec();

    if (question.clickedButton() == carryOn)
    {
        bisectionViewModel.Resume(ResumeChoice::CarryOnFromWhereItStopped);
    }

    if (question.clickedButton() == putBack)
    {
        bisectionViewModel.Resume(ResumeChoice::PutBackTheStartingConfiguration);
    }

    if (question.clickedButton() == forget)
    {
        bisectionViewModel.Resume(ResumeChoice::ForgetItAndLeaveTheDiskAsItIs);
    }
}

void OfferToDropTheOverridesThatPointNowhere(Session& session, QWidget* parent)
{
    const std::vector<DestinationOverride> orphans = session.OverridesPointingNowhere();
    if (orphans.empty())
    {
        return;
    }

    QStringList detailed;
    for (const DestinationOverride& orphan : orphans)
    {
        detailed.append(QStringLiteral("%1 -> %2").arg(AsText(orphan.relativePath), AsText(orphan.destination)));
    }

    detailed.append(QString{});
    detailed.append(QObject::tr("Destinations of this profile:"));
    for (const std::filesystem::path& destination : session.Profile().destinations)
    {
        detailed.append(AsText(destination));
    }

    QMessageBox question(
        QMessageBox::Warning, QObject::tr("Destination pinnings pointing outside"),
        QObject::tr("%n destination pinning of this profile names a folder that is not a destination of it. While that "
                    "is so, the pinned addons use the default destination. Nothing was deleted from the configuration.",
                    nullptr, static_cast<int>(orphans.size())),
        QMessageBox::NoButton, parent);
    question.setDetailedText(detailed.join(QChar::LineFeed));

    const QPushButton* drop = question.addButton(QObject::tr("Discard the pinnings"), QMessageBox::AcceptRole);
    question.addButton(QObject::tr("Keep them and decide later"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() == drop)
    {
        session.DropOverridesPointingNowhere();
    }
}

void OfferWhatTheOldProgramKept(LegacyImportViewModel& legacyViewModel, QWidget* parent)
{
    if (!legacyViewModel.SomethingIsWaiting())
    {
        return;
    }

    QMessageBox question(QMessageBox::Question, QObject::tr("MSFS Addons Linker is on this machine"),
                         QObject::tr("It has libraries FS Organizer does not know yet. Nothing is moved or "
                                     "deleted: you choose what to bring over before anything happens."),
                         QMessageBox::NoButton, parent);

    const QPushButton* look = question.addButton(QObject::tr("See what can be brought over"), QMessageBox::AcceptRole);
    question.addButton(QObject::tr("Not now"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != look)
    {
        return;
    }

    LegacyImportDialog dialog(legacyViewModel, parent);
    dialog.exec();
}

void OfferToPutBackWhatALostSwapRenamed(ImportViewModel& importViewModel, QWidget* parent)
{
    const std::vector<InterruptedSwap> swaps = importViewModel.InterruptedSwaps();
    if (swaps.empty())
    {
        return;
    }

    QStringList detailed;
    for (const InterruptedSwap& swap : swaps)
    {
        detailed.append(QStringLiteral("%1 -> %2").arg(AsText(swap.room), AsText(swap.folder)));
    }

    QMessageBox question(
        QMessageBox::Warning, QObject::tr("A folder of another program was left renamed"),
        QObject::tr("%n folder that FS Organizer took over is still under the name it was given while the swap ran, so "
                    "the other program no longer finds it. Your addon is safe in the library: what is missing is the "
                    "folder under its own name.",
                    nullptr, static_cast<int>(swaps.size())),
        QMessageBox::NoButton, parent);
    question.setDetailedText(detailed.join(QChar::LineFeed));

    const QPushButton* putBack = question.addButton(QObject::tr("Put the folders back"), QMessageBox::AcceptRole);
    question.addButton(QObject::tr("Leave them and decide later"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() == putBack)
    {
        importViewModel.UndoInterruptedSwaps(swaps);
    }
}

void OfferWhatALostImportLeftBehind(ImportViewModel& importViewModel, QWidget* parent)
{
    const std::vector<StagingLeftover> leftovers = importViewModel.Leftovers();
    if (leftovers.empty())
    {
        return;
    }

    StagingLeftoverDialog dialog(leftovers, parent);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    importViewModel.SettleTheLeftovers(dialog.ToDiscard(), dialog.ToResume());
}
