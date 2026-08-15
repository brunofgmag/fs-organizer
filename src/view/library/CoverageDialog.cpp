#include "view/library/CoverageDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "view/ScrollThatReportsItsContent.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kDialogWidth = 620;
}

CoverageDialog::CoverageDialog(const std::vector<CoverageLine>& covered, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Two airports for the same place"));

    auto* explanation = new QLabel(
        tr("The simulator ships an airport of its own for %n of the places you are turning on. Its code comes from "
           "the package name, because that content is an archive the app cannot open, and it is the same code yours "
           "carries. Which one wins is the simulator's to decide.",
           nullptr, static_cast<int>(covered.size())),
        this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(2, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    for (const auto& [column, heading] :
         {std::pair{0, tr("Airport")}, std::pair{1, tr("Yours")}, std::pair{2, tr("The simulator's")}})
    {
        auto* label = new QLabel(heading, listed);
        label->setObjectName(QStringLiteral("PanelSubHeading"));
        grid->addWidget(label, 0, column);
    }

    int row = 1;
    for (const CoverageLine& line : covered)
    {
        grid->addWidget(new QLabel(line.code, listed), row, 0, Qt::AlignTop);

        auto* yours = new QLabel(line.covered, listed);
        yours->setWordWrap(true);
        grid->addWidget(yours, row, 1, Qt::AlignTop);

        auto* theirs = new QLabel(line.andBy, listed);
        theirs->setObjectName(QStringLiteral("PanelPromise"));
        theirs->setWordWrap(true);
        grid->addWidget(theirs, row, 2, Qt::AlignTop);

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->MeasureTheContentAt(kDialogWidth - 2 * kPageGutter);

    auto* promise = new QLabel(
        tr("Turning the simulator's one off writes one value in the package list. Nothing is added, removed or "
           "reordered, and the app keeps a copy of the file. Leaving both on turns your addon on all the same."),
        this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* turnOff = buttons->addButton(covered.size() == 1 ? tr("Turn the simulator's one off")
                                                                  : tr("Turn the simulator's ones off"),
                                              QDialogButtonBox::AcceptRole);
    turnOff->setProperty("role", "primary");
    turnOff->setDefault(true);
    buttons->addButton(tr("Leave both on"), QDialogButtonBox::RejectRole);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(promise);
    layout->addWidget(buttons);

    SizeToTheContent(*this, kDialogWidth);
}
