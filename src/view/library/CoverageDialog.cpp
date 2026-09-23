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
    setWindowTitle(tr("The simulator has this airport too"));

    auto* explanation =
        new QLabel(tr("The simulator ships its own version of %n airport you are enabling. If both stay enabled, the "
                      "simulator decides which one loads. The match is made by the package name.",
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
    layout->addWidget(scroll, 1);
    layout->addWidget(promise);
    layout->addWidget(buttons);

    SizeToTheContent(*this, kDialogWidth);
}
