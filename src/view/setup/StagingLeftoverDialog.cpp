#include "view/setup/StagingLeftoverDialog.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/WheelGuard.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    QString WhyThisOneIsHere(const StagingLeftover& leftover)
    {
        if (leftover.CanBeResumed())
        {
            return StagingLeftoverDialog::tr("came from: %1").arg(AsText(leftover.source));
        }

        if (leftover.theCopyResolvedAConflict)
        {
            return StagingLeftoverDialog::tr(
                "half of a conflict resolution: the two copies are still where they were, so only discarding is "
                "offered");
        }

        return StagingLeftoverDialog::tr(
            "the journal does not know where this came from, so only discarding is offered");
    }
}

StagingLeftoverDialog::StagingLeftoverDialog(const std::vector<StagingLeftover>& leftovers, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Imports that were left half finished"));

    auto* explanation = new QLabel(tr("An import was interrupted before it finished. The original files are still "
                                      "where they were: nothing was removed from the destination."),
                                   this);
    explanation->setWordWrap(true);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(0, 1);
    grid->setHorizontalSpacing(12);

    int row = 0;
    for (const StagingLeftover& leftover : leftovers)
    {
        auto* name = new QLabel(AsText(leftover.staging), this);
        name->setWordWrap(true);
        name->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(name, row, 0);

        auto* action = new QComboBox(this);
        action->addItem(tr("Leave it as it is"), LeaveItThere);
        if (leftover.CanBeResumed())
        {
            action->addItem(tr("Resume the import"), Resume);
        }
        action->addItem(tr("Discard the half finished copy"), Discard);
        action->setCurrentIndex(action->findData(leftover.CanBeResumed() ? Resume : LeaveItThere));
        LetTheWheelScrollPastUnlessTheWidgetHasFocus(action);
        grid->addWidget(action, row, 1);
        ++row;

        auto* origin = new QLabel(WhyThisOneIsHere(leftover), this);
        origin->setWordWrap(true);
        origin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        grid->addWidget(origin, row, 0, 1, 2);
        ++row;

        rows_.push_back({.leftover = leftover, .action = action});
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* apply = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    apply->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addLayout(grid, 1);
    layout->addWidget(buttons);

    SizeToTheContent(*this, 680);
}

std::vector<StagingLeftover> StagingLeftoverDialog::Chosen(const Action action) const
{
    std::vector<StagingLeftover> chosen;

    for (const Row& row : rows_)
    {
        if (row.action->currentData().toInt() == action)
        {
            chosen.push_back(row.leftover);
        }
    }

    return chosen;
}

std::vector<StagingLeftover> StagingLeftoverDialog::ToResume() const
{
    return Chosen(Resume);
}

std::vector<StagingLeftover> StagingLeftoverDialog::ToDiscard() const
{
    return Chosen(Discard);
}
