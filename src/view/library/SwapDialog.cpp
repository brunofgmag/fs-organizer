#include "view/library/SwapDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/ScrollThatReportsItsContent.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/AddonTreeViewModel.h"

namespace
{
    constexpr int kDialogWidth = 620;
}

SwapDialog::SwapDialog(const std::vector<TakenPlace>& swaps, const AddonTreeViewModel& viewModel, QWidget* parent)
    : QDialog(parent), viewModel_(viewModel)
{
    setWindowTitle(tr("Folder name already in use"));

    auto* explanation = new QLabel(tr("%n addon you are enabling has the same folder name as another enabled addon of "
                                      "yours. Swapping disables that one and enables the one you chose.",
                                      nullptr, static_cast<int>(swaps.size())),
                                   this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(2, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    auto* goesOff = new QLabel(tr("Will be disabled"), listed);
    goesOff->setObjectName(QStringLiteral("PanelSubHeading"));
    auto* goesOn = new QLabel(tr("Will be enabled"), listed);
    goesOn->setObjectName(QStringLiteral("PanelSubHeading"));
    auto* where = new QLabel(tr("In"), listed);
    where->setObjectName(QStringLiteral("PanelSubHeading"));

    grid->addWidget(goesOff, 0, 0);
    grid->addWidget(goesOn, 0, 1);
    grid->addWidget(where, 0, 2);

    int row = 1;
    for (const TakenPlace& swap : swaps)
    {
        goesOff_.push_back(AddTheSide(*grid, row, 0, NameAndVersionOf(swap.occupant)));
        goesOn_.push_back(AddTheSide(*grid, row, 1, NameAndVersionOf(swap.addonFolder)));

        auto* place = new QLabel(AsText(swap.linkPath.parent_path()), listed);
        place->setObjectName(QStringLiteral("PanelPromise"));
        place->setWordWrap(true);
        grid->addWidget(place, row, 2, Qt::AlignTop);

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->MeasureTheContentAt(kDialogWidth - 2 * kPageGutter);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* swap = buttons->addButton(tr("Swap"), QDialogButtonBox::AcceptRole);
    swap->setProperty("role", "primary");
    swap->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);

    ShowTheSizes(std::vector<WeighedSwap>(swaps.size()));

    SizeToTheContent(*this, kDialogWidth);
}

QLabel* SwapDialog::AddTheSide(QGridLayout& grid, const int row, const int column, const QString& nameAndVersion)
{
    auto* said = new QLabel(grid.parentWidget());
    said->setWordWrap(true);
    said->setProperty("nameAndVersion", nameAndVersion);

    grid.addWidget(said, row, column, Qt::AlignTop);

    return said;
}

void SwapDialog::ShowTheSizes(const std::vector<WeighedSwap>& weighed)
{
    for (std::size_t at = 0; at < weighed.size() && at < goesOff_.size(); ++at)
    {
        Retell(*goesOff_[at], weighed[at].goesOff);
        Retell(*goesOn_[at], weighed[at].goesOn);
    }
}

void SwapDialog::Retell(QLabel& side, const MeasuredFolder& measured)
{
    const QString nameAndVersion = side.property("nameAndVersion").toString();

    side.setText(measured.measured ? tr("%1 · %2").arg(nameAndVersion, AsSize(measured.bytes)) : nameAndVersion);
}

QString SwapDialog::NameAndVersionOf(const std::filesystem::path& addonFolder) const
{
    const QString version = viewModel_.VersionOf(addonFolder);
    const QString name = AsText(addonFolder.filename());

    return version.isEmpty() ? name : tr("%1 · %2").arg(name, version);
}
