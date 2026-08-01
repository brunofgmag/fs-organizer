#include "view/setup/SetupWizard.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWizardPage>

#include "support/PathText.h"
#include "view/WheelGuard.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/SimulatorText.h"

namespace
{
    QString CandidateLabel(const SimulatorCandidate& candidate)
    {
        QStringList names;
        for (const std::filesystem::path& destination : candidate.destinations)
        {
            names.append(AsText(destination.filename()));
        }

        return QStringLiteral("%1 · %2 (%3)")
            .arg(NameOf(candidate.variant), AsText(candidate.packagesPath), names.join(", "));
    }
}

SetupWizard::SetupWizard(SetupViewModel& viewModel, QWidget* parent) : QWizard(parent), viewModel_(viewModel)
{
    setWindowTitle(tr("FS Organizer · primeira configuração"));
    setWizardStyle(ModernStyle);
    setOption(NoBackButtonOnStartPage, true);
    resize(720, 460);

    addPage(CreateSimulatorPage());
    addPage(CreateLibraryPage());
}

void SetupWizard::accept()
{
    viewModel_.ChooseCandidate(static_cast<std::size_t>(simulators_->currentRow()));

    if (!viewModel_.Complete())
    {
        QMessageBox::critical(
            this, tr("Não foi possível salvar a configuração"),
            tr("O perfil não pôde ser gravado no disco, então a configuração não foi concluída. "
               "Confira se você tem permissão de escrita na pasta de configurações e tente de novo."));
        return;
    }

    QWizard::accept();
}

QWizardPage* SetupWizard::CreateSimulatorPage()
{
    auto* page = new QWizardPage;
    page->setTitle(tr("Simulador"));
    page->setSubTitle(tr("Escolha a instalação que este perfil vai gerenciar."));

    simulators_ = new QListWidget(page);
    RefreshCandidates();

    variant_ = new QComboBox(page);
    variant_->addItem(NameOf(SimulatorVariant::MSFS2024), static_cast<int>(SimulatorVariant::MSFS2024));
    variant_->addItem(NameOf(SimulatorVariant::MSFS2020), static_cast<int>(SimulatorVariant::MSFS2020));
    LetTheWheelScrollPastUnlessTheWidgetHasFocus(variant_);

    auto* browse = new QPushButton(tr("Apontar uma pasta manualmente…"), page);
    connect(browse, &QPushButton::clicked, this, &SetupWizard::BrowseForDestination);

    auto* manual = new QHBoxLayout;
    manual->addWidget(variant_);
    manual->addWidget(browse);
    manual->addStretch();

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
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

    auto* add = new QPushButton(tr("Adicionar biblioteca…"), page);
    connect(add, &QPushButton::clicked, this, &SetupWizard::BrowseForLibrary);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(libraries_);
    layout->addWidget(add);

    return page;
}

void SetupWizard::BrowseForDestination()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Escolha a pasta de destino do simulador"));
    if (chosen.isEmpty())
    {
        return;
    }

    const std::filesystem::path path = AsPath(chosen);
    if (!ConfirmDestination(path))
    {
        return;
    }

    viewModel_.AddManualCandidate(path, static_cast<SimulatorVariant>(variant_->currentData().toInt()));

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
        QMessageBox::information(this, tr("Confirme a pasta"),
                                 tr("Essa pasta não se parece com um destino do simulador, que costuma se chamar "
                                    "Community. Ela será usada assim mesmo."));
        return true;

    case DestinationCheck::Accepted: return true;
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
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Escolha a pasta da biblioteca"));
    if (chosen.isEmpty())
    {
        return;
    }

    const std::filesystem::path path = AsPath(chosen);
    if (!viewModel_.RegisterLibrary(path, path.filename().string()).Accepted())
    {
        QMessageBox::warning(this, tr("Biblioteca repetida"),
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
        const QString categories = tr("%n categoria(s)", nullptr, static_cast<int>(registered.categories));
        const QString addons = tr("%n addon(s)", nullptr, static_cast<int>(registered.addons));

        libraries_->addItem(QStringLiteral("%1 · %2 · %3, %4")
                                .arg(QString::fromStdString(registered.library.label), AsText(registered.library.path),
                                     categories, addons));
    }
}
