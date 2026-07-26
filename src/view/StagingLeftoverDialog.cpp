#include "view/StagingLeftoverDialog.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"

StagingLeftoverDialog::StagingLeftoverDialog(const std::vector<StagingLeftover>& leftovers, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Importações que ficaram pela metade"));

    auto* explanation =
        new QLabel(tr("Uma importação foi interrompida antes de terminar. Os arquivos originais continuam onde "
                      "estavam: nada foi removido do destino."),
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
        action->addItem(tr("Deixar como está"), LeaveItThere);
        if (leftover.CanBeResumed())
        {
            action->addItem(tr("Retomar a importação"), Resume);
        }
        action->addItem(tr("Descartar a cópia pela metade"), Discard);
        action->setCurrentIndex(leftover.CanBeResumed() ? 1 : 0);
        grid->addWidget(action, row, 1);
        ++row;

        auto* origin = new QLabel(leftover.CanBeResumed()
                                      ? tr("veio de: %1").arg(AsText(leftover.source))
                                      : tr("o diário não sabe de onde isto veio, então só o descarte é oferecido"),
                                  this);
        origin->setWordWrap(true);
        origin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        grid->addWidget(origin, row, 0, 1, 2);
        ++row;

        rows_.push_back({leftover, action});
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* apply = buttons->addButton(tr("Aplicar"), QDialogButtonBox::AcceptRole);
    apply->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(explanation);
    layout->addLayout(grid, 1);
    layout->addWidget(buttons);

    resize(680, 320);
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
