#include "view/quarantine/RestoreDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"

RestoreDialog::RestoreDialog(const std::vector<QuarantinedItem>& items, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Restaurar da quarentena"));

    for (const QuarantinedItem& item : items)
    {
        if (item.KnowsWhereItCameFrom())
        {
            restorable_.push_back(item);
        }
    }

    const auto held = static_cast<int>(items.size());
    const auto stranded = held - static_cast<int>(restorable_.size());

    auto* explanation = new QLabel(tr("Cada pasta volta para o lugar de onde veio. Nada é sobrescrito: se já "
                                      "existe algo com o mesmo nome no destino, aquela restauração falha e diz "
                                      "por quê."),
                                   this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    int row = 0;
    for (const QuarantinedItem& item : restorable_)
    {
        auto* name = new QLabel(AsText(item.path.filename()), listed);
        name->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(name, row, 0);

        auto* target = new QLabel(tr("volta para %1").arg(AsText(item.origin.parent_path())), listed);
        target->setObjectName(QStringLiteral("PanelPromise"));
        target->setWordWrap(true);
        target->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(target, row, 1);

        ++row;
    }

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);

    auto* counted = new QLabel(
        stranded > 0 ? tr("%n pasta(s) será(ão) restaurada(s).", nullptr, static_cast<int>(restorable_.size()))
                + QStringLiteral(" ")
                + tr("%n ficou(aram) de fora porque o diário não sabe de onde veio(vieram).", nullptr, stranded)
                     : tr("%n pasta(s) será(ão) restaurada(s).", nullptr, static_cast<int>(restorable_.size())),
        this);
    counted->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* restore = buttons->addButton(tr("Restaurar"), QDialogButtonBox::AcceptRole);
    restore->setDefault(true);
    restore->setEnabled(!restorable_.empty());

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(counted);
    layout->addWidget(buttons);

    resize(660, 380);
}

std::vector<QuarantinedItem> RestoreDialog::Restorable() const
{
    return restorable_;
}
