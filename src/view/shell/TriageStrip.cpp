#include "view/shell/TriageStrip.h"

#include <QtCore/QEvent>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include "view/theme/ModernistMetrics.h"

TriageStrip::TriageStrip(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("TriageStrip"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kPageGutter, 8, kPageGutter, 8);
    row->setSpacing(10);

    broken_ = AddItem("filled", row);
    broken_.action->setProperty("role", "primary");

    beforeConflicts_ = AddSeparator(row);

    conflicts_ = AddItem("outlined", row);

    beforeDuplicated_ = AddSeparator(row);

    duplicated_ = AddItem("outlined", row);

    row->addStretch();

    unmanaged_ = AddItem(nullptr, row);
    unmanaged_.label->setObjectName(QStringLiteral("TriageQuiet"));

    connect(broken_.action, &QPushButton::clicked, this, &TriageStrip::RepairRequested);
    connect(conflicts_.action, &QPushButton::clicked, this, &TriageStrip::ResolveRequested);
    connect(duplicated_.action, &QPushButton::clicked, this, &TriageStrip::DuplicatesRequested);
    connect(unmanaged_.action, &QPushButton::clicked, this, &TriageStrip::ImportRequested);

    ShowBreakdown({});
}

QFrame* TriageStrip::AddSeparator(QHBoxLayout* into)
{
    auto* separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("TriageSeparator"));
    separator->setFixedSize(1, 18);
    into->addWidget(separator);

    return separator;
}

TriageStrip::Item TriageStrip::AddItem(const char* tag, QHBoxLayout* into)
{
    auto* label = new QLabel(this);

    if (tag != nullptr)
    {
        label->setProperty("tag", tag);
    }

    auto* button = new QPushButton(this);
    button->setProperty("scale", "small");
    button->setCursor(Qt::PointingHandCursor);

    into->addWidget(label);
    into->addWidget(button);

    return {.label = label, .action = button};
}

void TriageStrip::ShowItem(const Item& item, const bool shown)
{
    item.label->setVisible(shown);
    item.action->setVisible(shown);
}

void TriageStrip::ShowBreakdown(const AttentionBreakdown& breakdown)
{
    shown_ = breakdown;
    RetranslateUi();
}

void TriageStrip::RetranslateUi()
{
    broken_.action->setText(tr("Repair the broken ones…"));
    conflicts_.action->setText(tr("Resolve conflicts…"));
    duplicated_.action->setText(tr("See the duplicated ones…"));
    unmanaged_.action->setText(tr("Import the ones outside the library…"));

    broken_.label->setText(tr("%n with no target", nullptr, static_cast<int>(shown_.broken)));
    conflicts_.label->setText(tr("%n conflict", nullptr, static_cast<int>(shown_.conflicts)));
    duplicated_.label->setText(tr("%n duplicated", nullptr, static_cast<int>(shown_.duplicated)));
    unmanaged_.label->setText(tr("%n folder outside the library", nullptr, static_cast<int>(shown_.unmanaged)));

    ShowItem(broken_, shown_.broken > 0);
    ShowItem(conflicts_, shown_.conflicts > 0);
    ShowItem(duplicated_, shown_.duplicated > 0);
    ShowItem(unmanaged_, shown_.unmanaged > 0);
    beforeConflicts_->setVisible(shown_.broken > 0 && shown_.conflicts > 0);
    beforeDuplicated_->setVisible(shown_.duplicated > 0 && (shown_.broken > 0 || shown_.conflicts > 0));

    anythingToSay_ = shown_.broken + shown_.conflicts + shown_.duplicated + shown_.unmanaged > 0;
}

void TriageStrip::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

bool TriageStrip::HasAnythingToSay() const
{
    return anythingToSay_;
}
