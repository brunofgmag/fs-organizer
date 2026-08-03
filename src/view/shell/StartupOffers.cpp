#include "view/shell/StartupOffers.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "application/Session.h"
#include "support/PathText.h"
#include "view/legacy/LegacyImportDialog.h"
#include "view/setup/StagingLeftoverDialog.h"
#include "viewmodel/ImportViewModel.h"
#include "viewmodel/LegacyImportViewModel.h"

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

    static_cast<void>(importViewModel.DiscardLeftovers(dialog.ToDiscard()));

    if (const std::vector<StagingLeftover> resumed = dialog.ToResume(); !resumed.empty())
    {
        importViewModel.Resume(resumed);
    }
}
