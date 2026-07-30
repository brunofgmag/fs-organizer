#include "view/shell/TriageStrip.h"

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

    broken_ = AddItem("filled", tr("Reparar quebrados..."), row);
    broken_.action->setProperty("role", "primary");

    beforeConflicts_ = AddSeparator(row);

    conflicts_ = AddItem("outlined", tr("Resolver conflito(s)..."), row);

    beforeDuplicated_ = AddSeparator(row);

    duplicated_ = AddItem("outlined", tr("Ver duplicadas..."), row);

    row->addStretch();

    unmanaged_ = AddItem(nullptr, tr("Importar selecionados..."), row);
    unmanaged_.label->setObjectName(QStringLiteral("TriageQuiet"));

    connect(broken_.action, &QPushButton::clicked, this, &TriageStrip::RepairRequested);
    connect(conflicts_.action, &QPushButton::clicked, this, &TriageStrip::ResolveRequested);
    connect(duplicated_.action, &QPushButton::clicked, this, &TriageStrip::DuplicatesRequested);
    connect(unmanaged_.action, &QPushButton::clicked, this, &TriageStrip::ImportRequested);

    ShowBreakdown(0, 0, 0, 0);
}

QFrame* TriageStrip::AddSeparator(QHBoxLayout* into)
{
    auto* separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("TriageSeparator"));
    separator->setFixedSize(1, 18);
    into->addWidget(separator);

    return separator;
}

TriageStrip::Item TriageStrip::AddItem(const char* tag, const QString& action, QHBoxLayout* into)
{
    auto* label = new QLabel(this);

    if (tag != nullptr)
    {
        label->setProperty("tag", tag);
    }

    auto* button = new QPushButton(action, this);
    button->setProperty("scale", "small");
    button->setCursor(Qt::PointingHandCursor);

    into->addWidget(label);
    into->addWidget(button);

    return {label, button};
}

void TriageStrip::ShowItem(const Item& item, const bool shown)
{
    item.label->setVisible(shown);
    item.action->setVisible(shown);
}

void TriageStrip::ShowBreakdown(const std::size_t broken,
                                const std::size_t conflicts,
                                const std::size_t duplicated,
                                const std::size_t unmanaged)
{
    broken_.label->setText(tr("%n sem alvo", nullptr, static_cast<int>(broken)));
    conflicts_.label->setText(tr("%n conflito(s)", nullptr, static_cast<int>(conflicts)));
    duplicated_.label->setText(tr("%n duplicada(s)", nullptr, static_cast<int>(duplicated)));
    unmanaged_.label->setText(tr("%n pasta(s) fora da biblioteca", nullptr, static_cast<int>(unmanaged)));

    ShowItem(broken_, broken > 0);
    ShowItem(conflicts_, conflicts > 0);
    ShowItem(duplicated_, duplicated > 0);
    ShowItem(unmanaged_, unmanaged > 0);
    beforeConflicts_->setVisible(broken > 0 && conflicts > 0);
    beforeDuplicated_->setVisible(duplicated > 0 && (broken > 0 || conflicts > 0));

    anythingToSay_ = broken + conflicts + duplicated + unmanaged > 0;
}

bool TriageStrip::HasAnythingToSay() const
{
    return anythingToSay_;
}
