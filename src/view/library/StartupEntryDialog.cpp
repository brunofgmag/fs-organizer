#include "view/library/StartupEntryDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/HeadingsOverTheColumns.h"
#include "view/ListUnderItsHeadings.h"
#include "view/theme/ModernistMetrics.h"

StartupEntryDialog::StartupEntryDialog(const std::vector<StartupLine>& carried, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Startup program inside the addon"));

    auto* explanation =
        new QLabel(tr("%n simulator startup entry points inside what you are disabling. If it stays enabled, the "
                      "simulator will keep trying to launch a program that is no longer there.",
                      nullptr, static_cast<int>(carried.size())),
                   this);
    explanation->setWordWrap(true);

    auto* headings = new HeadingsOverTheColumns({tr("Program"), tr("Launches")}, this);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    headings->ShapeTheGridUnder(*grid);
    grid->setColumnStretch(1, 1);

    int row = 0;
    for (const StartupLine& line : carried)
    {
        auto* label = new QLabel(QString::fromStdString(line.label), listed);
        label->setWordWrap(true);
        grid->addWidget(label, row, 0, Qt::AlignTop);

        auto* path = new QLabel(AsText(line.path), listed);
        path->setObjectName(QStringLiteral("PanelPromise"));
        path->setWordWrap(true);
        grid->addWidget(path, row, 1, Qt::AlignTop);

        if (row == 0)
        {
            headings->StandOver({label, path});
        }

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    const QString turnOff =
        carried.size() == 1 ? tr("Disable the addon and the entry") : tr("Disable the addon and the entries");

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* both = buttons->addButton(turnOff, QDialogButtonBox::AcceptRole);
    both->setProperty("role", "primary");
    both->setDefault(true);
    buttons->addButton(tr("Disable only the addon"), QDialogButtonBox::RejectRole);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addLayout(ListUnderItsHeadings(*headings, *scroll, *this), 1);
    layout->addWidget(buttons);

    SizeToTheContent(*this, 620);
}
