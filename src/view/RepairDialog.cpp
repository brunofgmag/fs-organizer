#include "view/RepairDialog.h"

#include <algorithm>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/WheelGuard.h"
#include "view/theme/ModernistMetrics.h"

RepairDialog::RepairDialog(const std::vector<RepairCandidate>& candidates, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Reparar links quebrados"));

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
        groups->addWidget(CreateGroup(tr("Links para as suas bibliotecas"), library, true, false));
    }

    if (!thirdParty.empty())
    {
        groups->addWidget(CreateGroup(tr("Links de outros programas"), thirdParty, false, true));
    }

    groups->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* repair = buttons->addButton(tr("Reparar selecionados"), QDialogButtonBox::AcceptRole);
    repair->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);

    resize(720, 520);
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
        action->addItem(tr("Remover o nó morto"));
        if (candidate.repointTo.has_value())
        {
            action->addItem(tr("Re-apontar para a biblioteca"));
        }
        action->setEnabled(action->count() > 1);
        LetTheWheelScrollPastUnlessTheWidgetHasFocus(action);
        grid->addWidget(action, row, 1);
        ++row;

        if (showOrigin)
        {
            auto* origin = new QLabel(tr("aponta para: %1").arg(AsText(candidate.entry.target)), group);
            origin->setTextInteractionFlags(Qt::TextSelectableByMouse);
            origin->setWordWrap(true);
            origin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            grid->addWidget(origin, row, 0, 1, 2);
            ++row;
        }

        rows_.push_back({candidate, selected, action});
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
            {row.candidate, row.action->currentIndex() == 1 ? RepairAction::Repoint : RepairAction::RemoveDeadNode});
    }

    return requests;
}
