#include "view/shell/MainWindow.h"

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
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "view/platform/WindowsTitleBar.h"
#include "view/WheelGuard.h"
#include "view/shell/TriageStrip.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "view/theme/ModernistTheme.h"
#include "view/theme/PageTab.h"
#include "viewmodel/SimulatorText.h"

namespace
{
    constexpr int kStatusLingersFor = 6000;
    constexpr int kMeterWidth = 132;
    constexpr int kMeterHeight = 4;
    constexpr int kStatusBarAlreadyInsetsTheFirstWidgetBy = 2;
    constexpr int kGearGlyph = 14;
    constexpr QSize kWindowStartsAt(1140, 760);

}

MainWindow::MainWindow(const AppSettings& settings, QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QCoreApplication::applicationName());
    resize(kWindowStartsAt);

    pages_ = new QStackedWidget(this);

    triage_ = new TriageStrip(this);
    triage_->setVisible(false);

    connect(triage_, &TriageStrip::RepairRequested, this, &MainWindow::RepairRequested);
    connect(triage_, &TriageStrip::ResolveRequested, this, &MainWindow::ResolveRequested);
    connect(triage_, &TriageStrip::DuplicatesRequested, this, &MainWindow::DuplicatesRequested);
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
                gear_->setIcon(GearIcon(kGearGlyph));
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
    LetTheWheelScrollPastUnlessTheWidgetHasFocus(profiles_);

    connect(profiles_, &QComboBox::activated, this, &MainWindow::OnProfileActivated);

    gear_ = new QToolButton(header);
    gear_->setObjectName(QStringLiteral("Gear"));
    gear_->setIcon(GearIcon(kGearGlyph));
    gear_->setIconSize(QSize(kGearGlyph, kGearGlyph));
    gear_->setToolTip(tr("Opções"));
    gear_->setCursor(Qt::PointingHandCursor);
    gear_->setFixedHeight(profiles_->sizeHint().height());

    connect(gear_, &QToolButton::clicked, this, &MainWindow::ShowOptions);

    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(10);
    layout->addWidget(logo);
    layout->addWidget(brand);
    layout->addStretch();
    layout->addWidget(profiles_);
    layout->addWidget(gear_);

    return header;
}

QWidget* MainWindow::CreateTabStrip()
{
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("TabStrip"));

    tabs_ = new QHBoxLayout(strip);
    tabs_->setContentsMargins(kPageGutter, 0, kPageGutter, 0);
    tabs_->setSpacing(2);

    back_ = new PageTab(tr("Voltar"), strip);
    back_->setVisible(false);
    connect(back_, &PageTab::clicked, this, &MainWindow::LeaveOptions);

    tabs_->addWidget(back_);
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
    LineTheFooterUpWithThePage();

    statusFades_ = new QTimer(this);
    statusFades_->setSingleShot(true);
    statusFades_->setInterval(kStatusLingersFor);

    connect(statusFades_, &QTimer::timeout, this,
            [this]
            {
                DressTheFooterFor(pages_->currentWidget());
            });
}

void MainWindow::LineTheFooterUpWithThePage() const
{
    statusBar()->setContentsMargins(kPageGutter - kStatusBarAlreadyInsetsTheFirstWidgetBy, 0, kPageGutter, 0);
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

    tabsByPage_.insert(page, tab);

    if (pages_->count() == 1)
    {
        tab->setChecked(true);
        pages_->setCurrentWidget(page);
        DressTheFooterFor(page);
    }

    return tab;
}

void MainWindow::CarryOptionsOn(QWidget* page)
{
    options_ = page;
    pages_->addWidget(page);
}

bool MainWindow::ShowingOptions() const
{
    return options_ != nullptr && pages_->currentWidget() == options_;
}

void MainWindow::ShowOptions()
{
    if (options_ == nullptr || ShowingOptions())
    {
        return;
    }

    behindTheOptions_ = pages_->currentWidget();

    const PageTab* origin = tabsByPage_.value(behindTheOptions_);
    back_->Relabel(origin != nullptr ? tr("← Voltar para %1").arg(origin->Label()) : tr("← Voltar"));

    for (PageTab* tab : tabsByPage_)
    {
        tab->setVisible(false);
    }

    back_->setVisible(true);
    pages_->setCurrentWidget(options_);
    statusFades_->stop();
    DressTheFooterFor(options_);

    emit OptionsRequested();
}

void MainWindow::LeaveOptions()
{
    if (!ShowingOptions())
    {
        return;
    }

    back_->setVisible(false);

    for (PageTab* tab : tabsByPage_)
    {
        tab->setVisible(true);
    }

    QWidget* landing = behindTheOptions_ != nullptr ? behindTheOptions_ : pages_->widget(0);
    pages_->setCurrentWidget(landing);
    statusFades_->stop();
    DressTheFooterFor(landing);

    emit OptionsLeft();
    emit PageSelected(landing);
}

void MainWindow::CarryTriageOn(const QWidget* page)
{
    triaged_.insert(page, true);
}

void MainWindow::ShowTriage(const std::size_t broken,
                            const std::size_t conflicts,
                            const std::size_t duplicated,
                            const std::size_t unmanaged) const
{
    triage_->ShowBreakdown(broken, conflicts, duplicated, unmanaged);
    DressTheFooterFor(pages_->currentWidget());
}

void MainWindow::ShowStatus(const QString& message) const
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

void MainWindow::ShowSummary(const QWidget* page, const QString& summary)
{
    summaries_.insert(page, summary);

    if (pages_->currentWidget() == page && !statusFades_->isActive())
    {
        DressTheFooterFor(page);
    }
}

void MainWindow::ShowAside(const QWidget* page, const QString& aside)
{
    asides_.insert(page, aside);

    if (pages_->currentWidget() == page)
    {
        aside_->setText(aside);
    }
}

void MainWindow::ShowMeter(const QWidget* page, const int filled, const int outOf)
{
    meters_.insert(page, Meter{filled, outOf});

    if (pages_->currentWidget() == page && !statusFades_->isActive())
    {
        DressTheFooterFor(page);
    }
}

void MainWindow::DressTheFooterFor(const QWidget* page) const
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
        profiles_->addItem(NameOf(profile.variant), QString::fromStdString(profile.id));
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
