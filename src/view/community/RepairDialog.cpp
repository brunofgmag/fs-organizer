#include "view/community/RepairDialog.h"

#include <algorithm>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/ScrollThatReportsItsContent.h"
#include "view/WheelGuard.h"
#include "view/theme/ModernistMetrics.h"

RepairDialog::RepairDialog(const std::vector<RepairCandidate>& candidates, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Repair broken links"));

    std::vector<RepairCandidate> library;
    std::vector<RepairCandidate> thirdParty;
    for (const RepairCandidate& candidate : candidates)
    {
        (candidate.targetsLibrary ? library : thirdParty).push_back(candidate);
    }

    auto* content = new QWidget(this);
    auto* groups = new QVBoxLayout(content);

    if (!library.empty())
    {
        groups->addWidget(CreateGroup(tr("Links to your libraries"), library, true, false));
    }

    if (!thirdParty.empty())
    {
        groups->addWidget(CreateGroup(tr("Links from other programs"), thirdParty, false, true));
    }

    groups->addStretch();

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* repair = buttons->addButton(tr("Repair the selected ones"), QDialogButtonBox::AcceptRole);
    repair->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);

    SizeToTheContent(*this, 720);
}

QWidget* RepairDialog::CreateGroup(const QString& title,
                                   const std::vector<RepairCandidate>& candidates,
                                   const bool checkedByDefault,
                                   const bool showOrigin)
{
    auto* group = new QGroupBox(title, this);
    auto* grid = new QGridLayout(group);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(1, 200);
    grid->setHorizontalSpacing(12);

    int row = 0;
    for (const RepairCandidate& candidate : candidates)
    {
        auto* selected = new QCheckBox(AsText(candidate.entry.path.filename()), group);
        selected->setChecked(checkedByDefault);
        grid->addWidget(selected, row, 0);

        auto* action = new QComboBox(group);
        action->addItem(tr("Remove the dead node"));
        if (candidate.repointTo.has_value())
        {
            action->addItem(tr("Repoint to the library"));
        }
        action->setEnabled(action->count() > 1);
        LetTheWheelScrollPastUnlessTheWidgetHasFocus(action);
        grid->addWidget(action, row, 1);
        ++row;

        if (showOrigin)
        {
            auto* origin = new QLabel(tr("points at: %1").arg(AsText(candidate.entry.target)), group);
            origin->setTextInteractionFlags(Qt::TextSelectableByMouse);
            origin->setWordWrap(true);
            origin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            grid->addWidget(origin, row, 0, 1, 2);
            ++row;
        }

        rows_.push_back({.candidate = candidate, .selected = selected, .action = action});
    }

    return group;
}

std::vector<RepairRequest> RepairDialog::ChosenRequests() const
{
    std::vector<RepairRequest> requests;

    for (const Row& row : rows_)
    {
        if (!row.selected->isChecked())
        {
            continue;
        }

        requests.push_back(
            {.candidate = row.candidate,
             .action = row.action->currentIndex() == 1 ? RepairAction::Repoint : RepairAction::RemoveDeadNode});
    }

    return requests;
}
