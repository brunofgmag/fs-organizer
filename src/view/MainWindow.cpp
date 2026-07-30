#include "view/MainWindow.h"

#include <algorithm>

#include <QtCore/QSignalBlocker>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>

#include "infrastructure/platform/WindowsTitleBar.h"
#include "view/panels/TriageStrip.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"

namespace
{
    constexpr int kStatusLingersFor = 6000;
    constexpr int kMeterWidth = 132;
    constexpr int kMeterHeight = 4;

    QString ProfileLabel(const SimulatorProfile& profile)
    {
        return profile.variant == SimulatorVariant::MSFS2020 ? QObject::tr("Flight Simulator 2020")
                                                             : QObject::tr("Flight Simulator 2024");
    }
}

MainWindow::MainWindow(const AppSettings& settings, QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QCoreApplication::applicationName());
    resize(1140, 760);

    pages_ = new QStackedWidget(this);

    triage_ = new TriageStrip(this);
    triage_->setVisible(false);

    connect(triage_, &TriageStrip::RepairRequested, this, &MainWindow::RepairRequested);
    connect(triage_, &TriageStrip::ResolveRequested, this, &MainWindow::ResolveRequested);
    connect(triage_, &TriageStrip::ImportRequested, this, &MainWindow::ImportRequested);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateHeader());
    layout->addWidget(CreateTabStrip());
    layout->addWidget(triage_);
    layout->addWidget(pages_, 1);

    setCentralWidget(central);

    CreateFooter();
    ShowProfiles(settings);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this]
            {
                ApplySystemTitleBarTheme(*this);
            });
}

QWidget* MainWindow::CreateHeader()
{
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("HeaderBar"));

    auto* logo = new QLabel(header);
    logo->setPixmap(BrandIcon().pixmap(24));

    auto* brand = new QLabel(QCoreApplication::applicationName(), header);
    QFont brandFont = brand->font();
    brandFont.setWeight(QFont::ExtraBold);
    brandFont.setPointSizeF(brandFont.pointSizeF() * 1.15);
    brand->setFont(brandFont);

    profiles_ = new QComboBox(header);
    profiles_->setObjectName(QStringLiteral("ProfilePicker"));

    connect(profiles_, &QComboBox::activated, this, &MainWindow::OnProfileActivated);

    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(10);
    layout->addWidget(logo);
    layout->addWidget(brand);
    layout->addStretch();
    layout->addWidget(profiles_);

    return header;
}

QWidget* MainWindow::CreateTabStrip()
{
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("TabStrip"));

    tabs_ = new QHBoxLayout(strip);
    tabs_->setContentsMargins(kPageGutter, 0, kPageGutter, 0);
    tabs_->setSpacing(2);
    tabs_->addStretch();

    return strip;
}

void MainWindow::CreateFooter()
{
    summary_ = new QLabel(statusBar());
    summary_->setObjectName(QStringLiteral("FooterSummary"));

    meter_ = new QProgressBar(statusBar());
    meter_->setObjectName(QStringLiteral("FooterMeter"));
    meter_->setTextVisible(false);
    meter_->setFixedSize(kMeterWidth, kMeterHeight);
    meter_->setVisible(false);

    aside_ = new QLabel(statusBar());
    aside_->setObjectName(QStringLiteral("FooterAside"));

    restart_ = new QLabel(statusBar());
    restart_->setObjectName(QStringLiteral("FooterRestart"));
    restart_->setVisible(false);

    statusBar()->addWidget(summary_);
    statusBar()->addWidget(meter_);
    statusBar()->addPermanentWidget(aside_);
    statusBar()->addPermanentWidget(restart_);
    statusBar()->setSizeGripEnabled(false);

    statusFades_ = new QTimer(this);
    statusFades_->setSingleShot(true);
    statusFades_->setInterval(kStatusLingersFor);

    connect(statusFades_, &QTimer::timeout, this,
            [this]
            {
                DressTheFooterFor(pages_->currentWidget());
            });
}

PageTab* MainWindow::AddPage(const QString& label, QWidget* page)
{
    pages_->addWidget(page);

    auto* tab = new PageTab(label, this);
    tabs_->insertWidget(tabs_->count() - 1, tab);

    connect(tab, &PageTab::clicked, this,
            [this, page]
            {
                pages_->setCurrentWidget(page);
                statusFades_->stop();
                DressTheFooterFor(page);
                emit PageSelected(page);
            });

    if (pages_->count() == 1)
    {
        tab->setChecked(true);
        pages_->setCurrentWidget(page);
        DressTheFooterFor(page);
    }

    return tab;
}

void MainWindow::CarryTriageOn(QWidget* page)
{
    triaged_.insert(page, true);
}

void MainWindow::ShowTriage(const std::size_t broken, const std::size_t conflicts, const std::size_t unmanaged)
{
    triage_->ShowBreakdown(broken, conflicts, unmanaged);
    DressTheFooterFor(pages_->currentWidget());
}

void MainWindow::ShowStatus(const QString& message)
{
    if (message.isEmpty())
    {
        return;
    }

    summary_->setText(message);
    meter_->setVisible(false);
    statusFades_->start();
}

void MainWindow::ShowRestartPending(const bool pending) const
{
    restart_->setText(pending ? tr("Reinicie o simulador para aplicar.") : QString());
    restart_->setVisible(pending);
}

void MainWindow::ShowSummary(QWidget* page, const QString& summary)
{
    summaries_.insert(page, summary);

    if (pages_->currentWidget() == page && !statusFades_->isActive())
    {
        DressTheFooterFor(page);
    }
}

void MainWindow::ShowAside(QWidget* page, const QString& aside)
{
    asides_.insert(page, aside);

    if (pages_->currentWidget() == page)
    {
        aside_->setText(aside);
    }
}

void MainWindow::ShowMeter(QWidget* page, const int filled, const int outOf)
{
    meters_.insert(page, Meter{filled, outOf});

    if (pages_->currentWidget() == page && !statusFades_->isActive())
    {
        DressTheFooterFor(page);
    }
}

void MainWindow::DressTheFooterFor(const QWidget* page)
{
    summary_->setText(summaries_.value(page));
    aside_->setText(asides_.value(page));

    const Meter meter = meters_.value(page);
    meter_->setVisible(meter.outOf > 0);
    meter_->setRange(0, std::max(1, meter.outOf));
    meter_->setValue(meter.filled);

    triage_->setVisible(triaged_.value(page, false) && triage_->HasAnythingToSay());
}

void MainWindow::ShowProfiles(const AppSettings& settings)
{
    settings_ = settings;

    const QSignalBlocker quiet(profiles_);
    profiles_->clear();

    for (const SimulatorProfile& profile : settings_.profiles)
    {
        profiles_->addItem(ProfileLabel(profile), QString::fromStdString(profile.id));
    }

    profiles_->insertSeparator(profiles_->count());
    profiles_->addItem(tr("Adicionar perfil..."), QVariant());

    const int active = profiles_->findData(QString::fromStdString(settings_.activeProfileId));
    profiles_->setCurrentIndex(active >= 0 ? active : 0);
}

void MainWindow::OnProfileActivated(const int index)
{
    const QVariant chosen = profiles_->itemData(index);

    if (!chosen.isValid())
    {
        ShowProfiles(settings_);
        emit AddProfileRequested();
        return;
    }

    settings_.activeProfileId = chosen.toString().toStdString();

    emit ProfileChosen(settings_.activeProfileId);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    ApplySystemTitleBarTheme(*this);

    if (QWidget* page = pages_->currentWidget(); page != nullptr)
    {
        page->setFocus(Qt::OtherFocusReason);
    }
}
