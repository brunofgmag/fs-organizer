#include "view/library/DeleteDialog.h"

#include <QtCore/QStringList>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/SizeSummary.h"

namespace
{
    constexpr int kDetailIndent = 22;
    constexpr int kDialogWidth = 520;

    SelectionSize WhatItWeighs(const DeletionPlan& plan)
    {
        SelectionSize size{.selected = plan.addons.size()};

        for (const AddonToDelete& addon : plan.addons)
        {
            if (addon.bytes.has_value())
            {
                size.bytes += *addon.bytes;
                ++size.measured;
            }
        }

        return size;
    }
}

DeleteDialog::DeleteDialog(DeletionPlan plan, DeletionViewModel& viewModel, QWidget* parent)
    : QDialog(parent), plan_(std::move(plan)), viewModel_(viewModel)
{
    setWindowTitle(tr("Delete from the library"));

    auto* heading = new QLabel(WhatWasSelected(), this);
    heading->setObjectName(QStringLiteral("PanelSubHeading"));
    heading->setWordWrap(true);

    auto* column = new QVBoxLayout;
    column->setSpacing(4);

    const std::size_t refused = AddonsTheRecycleBinRefuses(plan_);
    const bool anyGoes = refused < plan_.addons.size();

    recycle_ =
        AddRoute(*column, tr("Move to the Recycle Bin"),
                 refused == 0 ? tr("You can put it back from Windows. Windows may quietly evict older items from the "
                                   "Bin to make room for these %1, and the app cannot prevent that.")
                                    .arg(SizeOfTheSelection(WhatItWeighs(plan_)))
                              : WhatTheRecycleBinWillNotTake());
    recycle_->setEnabled(anyGoes);

    forGood_ =
        AddRoute(*column, tr("Delete permanently"), tr("It does not come back. Not through the Recycle Bin either."));

    recycle_->setChecked(anyGoes);
    forGood_->setChecked(!anyGoes);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(heading);
    layout->addLayout(column);

    if (const QString links = WhereTheLinksAre(); !links.isEmpty())
    {
        auto* enabled = new QLabel(links, this);
        enabled->setWordWrap(true);
        layout->addWidget(enabled);
    }

    if (plan_.nodesThatAreNotAddons > 0)
    {
        auto* aside = new QLabel(tr("%n selected item is not an addon and stays where it is.", nullptr,
                                    static_cast<int>(plan_.nodesThatAreNotAddons)),
                                 this);
        aside->setObjectName(QStringLiteral("PanelPromise"));
        aside->setWordWrap(true);
        layout->addWidget(aside);
    }

    auto* promise = new QLabel(tr("Nothing happens until you say so."), this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    confirm_ = buttons->addButton(tr("Delete permanently"), QDialogButtonBox::AcceptRole);
    confirm_->setProperty("role", "primary");
    confirm_->setDefault(true);
    confirm_->setEnabled(!plan_.addons.empty());

    connect(buttons, &QDialogButtonBox::accepted, this, &DeleteDialog::DeleteThem);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(recycle_, &QRadioButton::toggled, this, &DeleteDialog::ShowTheChosenRoute);

    layout->addWidget(promise);
    layout->addWidget(buttons);

    ShowTheChosenRoute();

    layout->activate();
    resize(kDialogWidth, layout->totalHeightForWidth(kDialogWidth));
}

QRadioButton* DeleteDialog::AddRoute(QVBoxLayout& column, const QString& title, const QString& detail)
{
    auto* route = new QRadioButton(title, this);

    auto* said = new QLabel(detail, this);
    said->setObjectName(QStringLiteral("PanelPromise"));
    said->setWordWrap(true);
    said->setContentsMargins(kDetailIndent, 0, 0, 8);

    column.addWidget(route);
    column.addWidget(said);

    return route;
}

DeletionRoute DeleteDialog::ChosenRoute() const
{
    return recycle_->isChecked() ? DeletionRoute::RecycleBin : DeletionRoute::Permanently;
}

QString DeleteDialog::WhatWasSelected() const
{
    const QString weight = SizeOfTheSelection(WhatItWeighs(plan_));

    if (plan_.addons.size() == 1)
    {
        return tr("%1 · %2").arg(AsText(plan_.addons.front().folder.filename()), weight);
    }

    return tr("%1 · %2").arg(tr("%n addon", nullptr, static_cast<int>(plan_.addons.size())), weight);
}

QString DeleteDialog::WhatTheRecycleBinWillNotTake() const
{
    QStringList refused;

    for (const AddonToDelete& addon : plan_.addons)
    {
        const FileResult verdict = WhatTheRecycleBinRefuses(plan_, addon);

        if (!Succeeded(verdict))
        {
            refused.append(tr("%1: %2").arg(AsText(addon.folder.filename()), Explain(verdict)));
        }
    }

    return tr("%n of the selected addons cannot go there, and this route leaves them in the library:", nullptr,
              refused.size())
        + QStringLiteral("\n") + refused.join(QStringLiteral("\n"));
}

QString DeleteDialog::WhereTheLinksAre() const
{
    QStringList said;

    for (const AddonToDelete& addon : plan_.addons)
    {
        for (const EnabledSomewhere& link : addon.enabled)
        {
            said.append(tr("%1: the link in %2 goes away with it · %3")
                            .arg(AsText(addon.folder.filename()), AsText(link.linkPath.parent_path().filename()),
                                 viewModel_.LabelOfProfile(link.profileId)));
        }
    }

    if (said.isEmpty())
    {
        return {};
    }

    return tr("Turning it off is part of this, and it happens because you asked for the deletion:")
        + QStringLiteral("\n") + said.join(QStringLiteral("\n"));
}

void DeleteDialog::ShowTheChosenRoute() const
{
    confirm_->setText(ChosenRoute() == DeletionRoute::RecycleBin ? tr("Move to the Recycle Bin")
                                                                 : tr("Delete permanently"));
}

void DeleteDialog::DeleteThem()
{
    viewModel_.Delete(plan_, ChosenRoute());

    accept();
}
