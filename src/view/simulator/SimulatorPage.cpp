#include "view/simulator/SimulatorPage.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"

SimulatorPage::SimulatorPage(QWidget* startup, QWidget* packages, QWidget* parent) : QWidget(parent)
{
    panels_ = new QStackedWidget(this);
    panels_->addWidget(startup);
    panels_->addWidget(packages);

    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("PageToolbar"));

    startup_ = new QPushButton(bar);
    startup_->setCheckable(true);
    startup_->setChecked(true);
    packages_ = new QPushButton(bar);
    packages_->setCheckable(true);

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    row->setSpacing(8);
    row->addWidget(startup_);
    row->addWidget(packages_);
    row->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(bar);
    layout->addWidget(panels_, 1);

    connect(startup_, &QPushButton::clicked, this, &SimulatorPage::ShowTheStartupEntries);
    connect(packages_, &QPushButton::clicked, this, &SimulatorPage::ShowThePackageList);

    RetranslateUi();
    Open(StartupEntries);
}

void SimulatorPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

void SimulatorPage::RetranslateUi() const
{
    startup_->setText(tr("Startup entries"));
    packages_->setText(tr("Package list"));
}

void SimulatorPage::ShowTheStartupEntries() const
{
    Open(StartupEntries);
}

void SimulatorPage::ShowThePackageList() const
{
    Open(PackageList);
}

void SimulatorPage::Open(const int panel) const
{
    panels_->setCurrentIndex(panel);

    startup_->setChecked(panel == StartupEntries);
    packages_->setChecked(panel == PackageList);

    GiveItTheRole(startup_, panel == StartupEntries ? QStringLiteral("primary") : QString());
    GiveItTheRole(packages_, panel == PackageList ? QStringLiteral("primary") : QString());
}

void SimulatorPage::CarrySummaryFrom(const QWidget* panel, const QString& summary)
{
    (panel == panels_->widget(StartupEntries) ? startupSummary_ : packageSummary_) = summary;

    emit SummaryChanged(panels_->currentIndex() == StartupEntries ? startupSummary_ : packageSummary_);
}
