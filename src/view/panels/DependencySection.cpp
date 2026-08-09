#include "view/panels/DependencySection.h"

#include <QtCore/QEvent>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayoutItem>
#include <QtWidgets/QVBoxLayout>

#include "viewmodel/DependencyText.h"

namespace
{
    QLabel* Quiet(QLabel* label)
    {
        label->setObjectName(QStringLiteral("DetailFieldName"));
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        return label;
    }
}

DependencySection::DependencySection(QWidget* parent) : QWidget(parent)
{
    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("PanelSubHeading"));

    lines_ = new QWidget(this);
    stack_ = new QVBoxLayout(lines_);
    stack_->setContentsMargins(0, 0, 0, 0);
    stack_->setSpacing(9);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("PanelPromise"));
    note_->setWordWrap(true);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 6, 0, 0);
    column->setSpacing(7);
    column->addWidget(title_);
    column->addWidget(lines_);
    column->addWidget(note_);

    Show({});
}

void DependencySection::Show(const DependencyReport& report)
{
    shown_ = report;

    setHidden(shown_.answers.empty());
    Rebuild();
}

void DependencySection::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Rebuild();
    }

    QWidget::changeEvent(event);
}

void DependencySection::Rebuild() const
{
    while (const QLayoutItem* item = stack_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    title_->setText(tr("Dependencies · %1").arg(shown_.answers.size()));

    for (const DependencyAnswer& answer : shown_.answers)
    {
        stack_->addWidget(LineFor(answer));
    }

    note_->setText(WhereTheListCameFrom(shown_));
}

QWidget* DependencySection::LineFor(const DependencyAnswer& answer) const
{
    auto* line = new QWidget(lines_);

    auto* grid = new QGridLayout(line);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(0, 1);

    auto* name = new QLabel(QString::fromStdString(answer.name), line);
    name->setObjectName(QStringLiteral("DetailFieldValue"));
    name->setWordWrap(true);
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* said = new QLabel(AnswerFor(answer), line);
    said->setObjectName(QStringLiteral("DetailFieldValue"));
    said->setWordWrap(true);

    grid->addWidget(name, 0, 0);
    grid->addWidget(said, 1, 0);

    if (!answer.declaredVersion.empty())
    {
        grid->addWidget(Quiet(new QLabel(tr("needs %1").arg(QString::fromStdString(answer.declaredVersion)), line)), 0,
                        1);
    }

    if (!answer.libraryVersion.empty())
    {
        grid->addWidget(Quiet(new QLabel(tr("has %1").arg(QString::fromStdString(answer.libraryVersion)), line)), 1, 1);
    }

    return line;
}
