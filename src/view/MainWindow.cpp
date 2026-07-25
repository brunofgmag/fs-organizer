#include "view/MainWindow.h"

#include <QtCore/QSignalBlocker>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>

#include "infrastructure/platform/WindowsTitleBar.h"

namespace
{
    QString ProfileLabel(const SimulatorProfile& profile)
    {
        return profile.variant == SimulatorVariant::MSFS2020
                   ? QObject::tr("Flight Simulator 2020")
                   : QObject::tr("Flight Simulator 2024");
    }
}

MainWindow::MainWindow(const AppSettings& settings, QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QCoreApplication::applicationName());
    resize(1024, 720);

    pages_ = new QStackedWidget(this);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(CreateHeader());
    layout->addWidget(pages_, 1);

    setCentralWidget(central);

    restart_ = new QLabel(statusBar());
    restart_->setVisible(false);
    statusBar()->addPermanentWidget(restart_);
    statusBar()->showMessage(tr("Pronto."));

    ShowProfiles(settings);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { ApplySystemTitleBarTheme(*this); });
}

QWidget* MainWindow::CreateHeader()
{
    auto* header = new QWidget(this);

    auto* brand = new QLabel(QCoreApplication::applicationName(), header);
    QFont brandFont = brand->font();
    brandFont.setBold(true);
    brand->setFont(brandFont);

    profiles_ = new QComboBox(header);

    connect(profiles_, &QComboBox::activated, this, &MainWindow::OnProfileActivated);

    auto* layout = new QHBoxLayout(header);
    layout->addWidget(brand);
    layout->addStretch();
    layout->addWidget(new QLabel(tr("Perfil:"), header));
    layout->addWidget(profiles_);

    return header;
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

void MainWindow::ShowPage(QWidget* page) const
{
    pages_->addWidget(page);
    pages_->setCurrentWidget(page);
}

void MainWindow::ShowStatus(const QString& message) const
{
    statusBar()->showMessage(message);
}

void MainWindow::ShowRestartPending(const bool pending) const
{
    restart_->setText(pending ? tr("Reinicie o simulador para aplicar.") : QString());
    restart_->setVisible(pending);
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
}
