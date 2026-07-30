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

    separator_ = new QFrame(this);
    separator_->setObjectName(QStringLiteral("TriageSeparator"));
    separator_->setFixedSize(1, 18);
    row->addWidget(separator_);

    conflicts_ = AddItem("outlined", tr("Resolver conflito(s)..."), row);

    row->addStretch();

    unmanaged_ = AddItem(nullptr, tr("Importar selecionados..."), row);
    unmanaged_.label->setObjectName(QStringLiteral("TriageQuiet"));

    connect(broken_.action, &QPushButton::clicked, this, &TriageStrip::RepairRequested);
    connect(conflicts_.action, &QPushButton::clicked, this, &TriageStrip::ResolveRequested);
    connect(unmanaged_.action, &QPushButton::clicked, this, &TriageStrip::ImportRequested);

    ShowBreakdown(0, 0, 0);
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

void TriageStrip::ShowBreakdown(const std::size_t broken, const std::size_t conflicts, const std::size_t unmanaged)
{
    broken_.label->setText(tr("%n sem alvo", nullptr, static_cast<int>(broken)));
    conflicts_.label->setText(tr("%n conflito(s)", nullptr, static_cast<int>(conflicts)));
    unmanaged_.label->setText(tr("%n pasta(s) fora da biblioteca", nullptr, static_cast<int>(unmanaged)));

    ShowItem(broken_, broken > 0);
    ShowItem(conflicts_, conflicts > 0);
    ShowItem(unmanaged_, unmanaged > 0);
    separator_->setVisible(broken > 0 && conflicts > 0);

    anythingToSay_ = broken + conflicts + unmanaged > 0;
}

bool TriageStrip::HasAnythingToSay() const
{
    return anythingToSay_;
}
