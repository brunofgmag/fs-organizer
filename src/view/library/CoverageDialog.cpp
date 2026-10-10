#include "view/library/CoverageDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "view/HeadingsOverTheColumns.h"
#include "view/ListUnderItsHeadings.h"
#include "view/ScrollThatReportsItsContent.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kDialogWidth = 620;
}

CoverageDialog::CoverageDialog(const std::vector<CoverageLine>& covered, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("The simulator has this airport too"));

    auto* explanation =
        new QLabel(tr("The simulator ships its own version of %n airport you are enabling. If both stay enabled, the "
                      "simulator decides which one loads. The match is made by the package name.",
                      nullptr, static_cast<int>(covered.size())),
                   this);
    explanation->setWordWrap(true);

    auto* headings = new HeadingsOverTheColumns({tr("Airport"), tr("Yours"), tr("The simulator's")}, this);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    headings->ShapeTheGridUnder(*grid);
    grid->setColumnStretch(2, 1);

    int row = 0;
    for (const CoverageLine& line : covered)
    {
        auto* code = new QLabel(line.code, listed);
        grid->addWidget(code, row, 0, Qt::AlignTop);

        auto* yours = new QLabel(line.covered, listed);
        yours->setWordWrap(true);
        grid->addWidget(yours, row, 1, Qt::AlignTop);

        auto* theirs = new QLabel(line.andBy, listed);
        theirs->setObjectName(QStringLiteral("PanelPromise"));
        theirs->setWordWrap(true);
        grid->addWidget(theirs, row, 2, Qt::AlignTop);

        if (row == 0)
        {
            headings->StandOver({code, yours, theirs});
        }

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->MeasureTheContentAt(kDialogWidth - 2 * kPageGutter);

    auto* promise = new QLabel(tr("Disabling the simulator's airport changes only its entry in the package list, and a "
                                  "backup of the file is kept. Your addon is enabled either way."),
                               this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* turnOff = buttons->addButton(covered.size() == 1 ? tr("Disable the simulator's airport")
                                                                  : tr("Disable the simulator's airports"),
                                              QDialogButtonBox::AcceptRole);
    turnOff->setProperty("role", "primary");
    turnOff->setDefault(true);
    buttons->addButton(tr("Keep both enabled"), QDialogButtonBox::RejectRole);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addLayout(ListUnderItsHeadings(*headings, *scroll, *this), 1);
    layout->addWidget(promise);
    layout->addWidget(buttons);

    SizeToTheContent(*this, kDialogWidth);
}
