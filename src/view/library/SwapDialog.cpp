#include "view/library/SwapDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/AddonTreeViewModel.h"

SwapDialog::SwapDialog(const std::vector<TakenPlace>& swaps, const AddonTreeViewModel& viewModel, QWidget* parent)
    : QDialog(parent), viewModel_(viewModel)
{
    setWindowTitle(tr("That spot is taken"));

    auto* explanation = new QLabel(
        tr("%n addon of yours is using the place where the one you asked for goes. Both sides are addons of yours, so "
           "the app can swap them.",
           nullptr, static_cast<int>(swaps.size())),
        this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(2, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    auto* goesOff = new QLabel(tr("Goes off"), listed);
    goesOff->setObjectName(QStringLiteral("PanelSubHeading"));
    auto* goesOn = new QLabel(tr("Goes on"), listed);
    goesOn->setObjectName(QStringLiteral("PanelSubHeading"));
    auto* where = new QLabel(tr("In"), listed);
    where->setObjectName(QStringLiteral("PanelSubHeading"));

    grid->addWidget(goesOff, 0, 0);
    grid->addWidget(goesOn, 0, 1);
    grid->addWidget(where, 0, 2);

    int row = 1;
    for (const TakenPlace& swap : swaps)
    {
        grid->addWidget(new QLabel(NameAndVersionOf(swap.occupant), listed), row, 0, Qt::AlignTop);
        grid->addWidget(new QLabel(NameAndVersionOf(swap.addonFolder), listed), row, 1, Qt::AlignTop);

        auto* place = new QLabel(AsText(swap.linkPath.parent_path()), listed);
        place->setObjectName(QStringLiteral("PanelPromise"));
        place->setWordWrap(true);
        grid->addWidget(place, row, 2, Qt::AlignTop);

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);

    auto* promise = new QLabel(
        tr("One operation, one line in the Journal, undone as one. Nothing happens until you say so."), this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* swap = buttons->addButton(tr("Swap them"), QDialogButtonBox::AcceptRole);
    swap->setProperty("role", "primary");
    swap->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(promise);
    layout->addWidget(buttons);

    resize(620, 280);
}

QString SwapDialog::NameAndVersionOf(const std::filesystem::path& addonFolder) const
{
    const QString version = viewModel_.VersionOf(addonFolder);
    const QString name = AsText(addonFolder.filename());

    return version.isEmpty() ? name : tr("%1 · %2").arg(name, version);
}
