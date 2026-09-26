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
        QMessageBox::Question, QObject::tr("A culprit search was interrupted"),
        QObject::tr("Your addons are still as the last round left them, not as they were before the search."),
        QMessageBox::NoButton, parent);

    const QPushButton* carryOn = question.addButton(QObject::tr("Continue the search"), QMessageBox::AcceptRole);
    const QPushButton* putBack =
        question.addButton(QObject::tr("Restore the previous setup"), QMessageBox::DestructiveRole);
    const QPushButton* forget = question.addButton(QObject::tr("Discard the search and keep the addons as they are"),
                                                   QMessageBox::DestructiveRole);
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

    QMessageBox question(QMessageBox::Warning, QObject::tr("Pinned destinations not found"),
                         QObject::tr("%n pinned destination of this profile is no longer one of its destinations. "
                                     "Until that changes, the addons pinned to it use the default destination.",
                                     nullptr, static_cast<int>(orphans.size())),
                         QMessageBox::NoButton, parent);
    question.setDetailedText(detailed.join(QChar::LineFeed));

    const QPushButton* drop = question.addButton(QObject::tr("Remove the pins"), QMessageBox::AcceptRole);
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

    QMessageBox question(QMessageBox::Question, QObject::tr("MSFS Addons Linker found"),
                         QObject::tr("It has libraries FS Organizer does not know yet. You choose what to import; no "
                                     "files are moved or deleted."),
                         QMessageBox::NoButton, parent);

    const QPushButton* look = question.addButton(QObject::tr("See what can be imported"), QMessageBox::AcceptRole);
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
        QMessageBox::Warning, QObject::tr("Another program's folder was left renamed"),
        QObject::tr("%n folder imported by FS Organizer still has a temporary name from an interrupted swap, so the "
                    "other program cannot find it. Your addon is safe in the library.",
                    nullptr, static_cast<int>(swaps.size())),
        QMessageBox::NoButton, parent);
    question.setDetailedText(detailed.join(QChar::LineFeed));

    const QPushButton* putBack = question.addButton(QObject::tr("Restore the folder names"), QMessageBox::AcceptRole);
    question.addButton(QObject::tr("Decide later"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() == putBack)
    {
        importViewModel.UndoInterruptedSwaps(swaps);
    }
}

void OfferWhatALostImportLeftBehind(ImportViewModel& importViewModel, QWidget* parent)
{
    QObject::connect(
        &importViewModel, &ImportViewModel::LeftoversFound, parent,
        [&importViewModel, parent](const std::vector<StagingLeftover>& leftovers)
        {
            StagingLeftoverDialog dialog(leftovers, parent);
            if (dialog.exec() != QDialog::Accepted)
            {
                return;
            }

            importViewModel.SettleTheLeftovers(dialog.ToDiscard(), dialog.ToResume());
        },
        Qt::SingleShotConnection);

    importViewModel.LookForLeftovers();
}
