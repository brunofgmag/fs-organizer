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

    auto* placeholder = new QLabel(tr("A árvore de addons chega no próximo passo."), pages_);
    placeholder->setAlignment(Qt::AlignCenter);
    pages_->addWidget(placeholder);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(CreateHeader());
    layout->addWidget(pages_, 1);

    setCentralWidget(central);
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

void MainWindow::OnProfileActivated(const int index)
{
    if (profiles_->itemData(index).isValid())
    {
        return;
    }

    ShowProfiles(settings_);

    emit AddProfileRequested();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    ApplySystemTitleBarTheme(*this);
}
