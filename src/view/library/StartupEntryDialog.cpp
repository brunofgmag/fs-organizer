#include "view/library/StartupEntryDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"

StartupEntryDialog::StartupEntryDialog(const std::vector<StartupLine>& carried, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("The simulator launches this from inside the addon"));

    auto* explanation = new QLabel(
        tr("%n startup entry of the simulator points inside what you are turning off. Leave it on and the simulator "
           "keeps trying to launch a program that will not be there.",
           nullptr, static_cast<int>(carried.size())),
        this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    auto* program = new QLabel(tr("Program"), listed);
    program->setObjectName(QStringLiteral("PanelSubHeading"));
    auto* where = new QLabel(tr("Launches"), listed);
    where->setObjectName(QStringLiteral("PanelSubHeading"));

    grid->addWidget(program, 0, 0);
    grid->addWidget(where, 0, 1);

    int row = 1;
    for (const StartupLine& line : carried)
    {
        auto* label = new QLabel(QString::fromStdString(line.label), listed);
        label->setWordWrap(true);
        grid->addWidget(label, row, 0, Qt::AlignTop);

        auto* path = new QLabel(AsText(line.path), listed);
        path->setObjectName(QStringLiteral("PanelPromise"));
        path->setWordWrap(true);
        grid->addWidget(path, row, 1, Qt::AlignTop);

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    const QString turnOff =
        carried.size() == 1 ? tr("Turn the addon and the entry off") : tr("Turn the addon and the entries off");

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* both = buttons->addButton(turnOff, QDialogButtonBox::AcceptRole);
    both->setProperty("role", "primary");
    both->setDefault(true);
    buttons->addButton(tr("Only turn the addon off"), QDialogButtonBox::RejectRole);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);

    SizeToTheContent(*this, 620);
}
