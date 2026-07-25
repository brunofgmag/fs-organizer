#include "view/SetupWizard.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

namespace
{
    QString Show(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }

    QString VariantLabel(const SimulatorVariant variant)
    {
        return variant == SimulatorVariant::MSFS2020
                   ? QObject::tr("Flight Simulator 2020")
                   : QObject::tr("Flight Simulator 2024");
    }

    QString CandidateLabel(const SimulatorCandidate& candidate)
    {
        QStringList names;
        for (const std::filesystem::path& destination : candidate.destinations)
        {
            names.append(Show(destination.filename()));
        }

        return QStringLiteral("%1 — %2 (%3)")
            .arg(VariantLabel(candidate.variant), Show(candidate.packagesPath), names.join(", "));
    }
}

SetupWizard::SetupWizard(SetupViewModel& viewModel, QWidget* parent)
    : QWizard(parent), viewModel_(viewModel)
{
    setWindowTitle(tr("FS Organizer — primeira configuração"));
    setWizardStyle(ModernStyle);
    setOption(NoBackButtonOnStartPage, true);
    resize(720, 460);

    addPage(CreateSimulatorPage());
    addPage(CreateLibraryPage());

    connect(this, &QWizard::accepted, this, [this]
    {
        viewModel_.ChooseCandidate(static_cast<std::size_t>(simulators_->currentRow()));
        viewModel_.Complete();
    });
}

QWizardPage* SetupWizard::CreateSimulatorPage()
{
    auto* page = new QWizardPage;
    page->setTitle(tr("Simulador"));
    page->setSubTitle(tr("Escolha a instalação que este perfil vai gerenciar."));

    simulators_ = new QListWidget(page);
    RefreshCandidates();

    variant_ = new QComboBox(page);
    variant_->addItem(VariantLabel(SimulatorVariant::MSFS2024),
                      static_cast<int>(SimulatorVariant::MSFS2024));
    variant_->addItem(VariantLabel(SimulatorVariant::MSFS2020),
                      static_cast<int>(SimulatorVariant::MSFS2020));

    auto* browse = new QPushButton(tr("Apontar uma pasta manualmente..."), page);
    connect(browse, &QPushButton::clicked, this, &SetupWizard::BrowseForDestination);

    auto* manual = new QHBoxLayout;
    manual->addWidget(variant_);
    manual->addWidget(browse);
    manual->addStretch();

    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Instalações encontradas:"), page));
    layout->addWidget(simulators_);
    layout->addLayout(manual);

    return page;
}

QWizardPage* SetupWizard::CreateLibraryPage()
{
    auto* page = new QWizardPage;
    page->setTitle(tr("Bibliotecas"));
    page->setSubTitle(tr("Escolha a pasta raiz onde os seus addons ficam guardados, fora do "
        "simulador. As subpastas dela viram categorias."));

    libraries_ = new QListWidget(page);

    auto* add = new QPushButton(tr("Adicionar biblioteca..."), page);
    connect(add, &QPushButton::clicked, this, &SetupWizard::BrowseForLibrary);

    auto* layout = new QVBoxLayout(page);
    layout->addWidget(libraries_);
    layout->addWidget(add);

    return page;
}

void SetupWizard::BrowseForDestination()
{
    const QString chosen =
        QFileDialog::getExistingDirectory(this, tr("Escolha a pasta de destino do simulador"));
    if (chosen.isEmpty())
    {
        return;
    }

    const std::filesystem::path path(chosen.toStdWString());
    if (!ConfirmDestination(path))
    {
        return;
    }

    viewModel_.AddManualCandidate(
        path, static_cast<SimulatorVariant>(variant_->currentData().toInt()));

    RefreshCandidates();
    simulators_->setCurrentRow(simulators_->count() - 1);
}

bool SetupWizard::ConfirmDestination(const std::filesystem::path& path)
{
    switch (viewModel_.CheckDestination(path))
    {
    case DestinationCheck::RejectedMissing:
        QMessageBox::warning(this, tr("Pasta inválida"), tr("Essa pasta não existe."));
        return false;

    case DestinationCheck::RejectedNotWritable:
        QMessageBox::warning(this, tr("Pasta inválida"), tr("Não é possível gravar nessa pasta."));
        return false;

    case DestinationCheck::AcceptedButUnfamiliar:
        QMessageBox::information(
            this, tr("Confirme a pasta"),
            tr("Essa pasta não se parece com um destino do simulador, que costuma se chamar "
                "Community. Ela será usada assim mesmo."));
        return true;

    case DestinationCheck::Accepted:
        return true;
    }

    return false;
}

void SetupWizard::RefreshCandidates() const
{
    simulators_->clear();
    for (const SimulatorCandidate& candidate : viewModel_.Candidates())
    {
        simulators_->addItem(CandidateLabel(candidate));
    }

    if (simulators_->count() > 0 && simulators_->currentRow() < 0)
    {
        simulators_->setCurrentRow(0);
    }
}

void SetupWizard::BrowseForLibrary()
{
    const QString chosen =
        QFileDialog::getExistingDirectory(this, tr("Escolha a pasta da biblioteca"));
    if (chosen.isEmpty())
    {
        return;
    }

    const std::filesystem::path path(chosen.toStdWString());
    if (viewModel_.RegisterLibrary(path, path.filename().string())
        == LibraryCheck::RejectedInsideAnotherLibrary)
    {
        QMessageBox::warning(
            this, tr("Biblioteca repetida"),
            tr("Essa pasta já está dentro de uma biblioteca cadastrada. Escolha a pasta raiz "
                "onde os addons ficam guardados; as subpastas dela viram categorias."));
        return;
    }

    RefreshLibraries();
}

void SetupWizard::RefreshLibraries() const
{
    libraries_->clear();
    for (const RegisteredLibrary& registered : viewModel_.Libraries())
    {
        const QString categories =
            tr("%n categoria(s)", nullptr, static_cast<int>(registered.categories));
        const QString addons = tr("%n addon(s)", nullptr, static_cast<int>(registered.addons));

        libraries_->addItem(QStringLiteral("%1 — %2 · %3, %4")
            .arg(QString::fromStdString(registered.library.label),
                 Show(registered.library.path), categories, addons));
    }
}
