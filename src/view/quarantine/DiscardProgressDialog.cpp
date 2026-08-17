#include "view/quarantine/DiscardProgressDialog.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QVBoxLayout>

#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kDialogWidth = 420;
}

DiscardProgressDialog::DiscardProgressDialog(const int items, QWidget* over) : QDialog(over)
{
    setWindowTitle(tr("Emptying the quarantine"));
    setWindowModality(Qt::ApplicationModal);
    setSizeGripEnabled(false);

    line_ = new QLabel(this);
    line_->setObjectName(QStringLiteral("ImportQuiet"));

    bar_ = new QProgressBar(this);
    bar_->setObjectName(QStringLiteral("ImportMeter"));
    bar_->setRange(0, items);
    bar_->setValue(0);
    bar_->setTextVisible(false);
    bar_->setFixedHeight(kMeterHeight);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    column->addWidget(line_);
    column->addWidget(bar_);

    ShowTheItem(0, items);

    SizeToTheContent(*this, kDialogWidth);
}

void DiscardProgressDialog::ShowTheItem(const int discarded, const int outOf)
{
    line_->setText(tr("Deleting %1 of %n item", nullptr, outOf).arg(discarded + 1));
    bar_->setRange(0, outOf);
    bar_->setValue(discarded);
}
