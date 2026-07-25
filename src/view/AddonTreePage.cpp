#include "view/AddonTreePage.h"

#include <algorithm>
#include <ranges>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "domain/tree/AddonTree.h"

namespace
{
    QString Show(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }

    QString Explain(const LinkFailure failure)
    {
        switch (failure)
        {
        case LinkFailure::DestinationHoldsRealFolder:
            return QObject::tr("já existe uma pasta de verdade com esse nome no destino");
        case LinkFailure::DestinationHoldsLiveLink:
            return QObject::tr("o destino já tem um link vivo de outro programa");
        case LinkFailure::UnreadableLinkTarget:
            return QObject::tr("não foi possível ler o alvo do link que ocupa o destino");
        case LinkFailure::CouldNotReplaceStaleLink:
            return QObject::tr("não foi possível remover o link morto que ocupava o destino");
        case LinkFailure::CouldNotCreateLink:
            return QObject::tr("não foi possível criar o link");
        case LinkFailure::PathIsNotAReparsePoint:
            return QObject::tr("o caminho não é um link, então nada foi removido");
        case LinkFailure::CouldNotRemoveLink:
            return QObject::tr("não foi possível remover o link");
        case LinkFailure::None:
            break;
        }

        return {};
    }

    QString Describe(const LinkOperationResult& result)
    {
        QString line = QStringLiteral("%1 — %2")
            .arg(Show(result.addonFolder.filename()), Explain(result.outcome.Failure()));

        if (result.outcome.Conflict().has_value())
        {
            line += QObject::tr("\n    pasta no destino: %1\n    addon na biblioteca: %2")
                .arg(Show(result.outcome.Conflict()->destinationPath),
                     Show(result.outcome.Conflict()->libraryPath));
        }

        if (result.outcome.Occupation().has_value())
        {
            line += QObject::tr("\n    o link atual aponta para: %1")
                .arg(Show(result.outcome.Occupation()->existingTarget));
        }

        return line;
    }
}

AddonTreePage::AddonTreePage(AddonTreeViewModel& viewModel, AddonTreeModel& model, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), model_(model)
{
    tree_ = new QTreeView(this);
    tree_->setModel(&model_);
    tree_->setHeaderHidden(true);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setUniformRowHeights(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* browser = new QWidget(this);
    auto* browserLayout = new QVBoxLayout(browser);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->addWidget(CreateActions());
    browserLayout->addWidget(tree_, 1);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(browser);
    pages_->addWidget(CreateInvite());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(pages_);

    connect(tree_, &QTreeView::customContextMenuRequested, this, &AddonTreePage::ShowDestinationMenu);
    connect(&model_, &AddonTreeModel::ToggleRequested, this, &AddonTreePage::OnToggleRequested);
    connect(&viewModel_, &AddonTreeViewModel::BatchFinished, this, &AddonTreePage::OnBatchFinished);
    connect(&viewModel_, &AddonTreeViewModel::ScanFinished, this, &AddonTreePage::OnScanFinished);

    connect(&viewModel_, &AddonTreeViewModel::ScanStarted, this,
            [this] { emit StatusChanged(tr("Lendo a biblioteca...")); });

    connect(&viewModel_, &AddonTreeViewModel::SimulatorIsRunning, this, [this]
    {
        QMessageBox::information(
            this, tr("Simulador aberto"),
            tr("O simulador está em execução. Os links foram criados, mas ele só vai enxergar a "
                "mudança depois de ser reiniciado."));
    });
}

QWidget* AddonTreePage::CreateActions()
{
    auto* bar = new QWidget(this);

    auto* enable = new QPushButton(tr("Marcar selecionados"), bar);
    auto* disable = new QPushButton(tr("Desmarcar selecionados"), bar);
    undo_ = new QPushButton(tr("Desfazer último lote"), bar);
    auto* rescan = new QPushButton(tr("Reler do disco"), bar);

    undo_->setEnabled(false);

    connect(enable, &QPushButton::clicked, this, [this] { ToggleSelection(true); });
    connect(disable, &QPushButton::clicked, this, [this] { ToggleSelection(false); });
    connect(undo_, &QPushButton::clicked, &viewModel_, &AddonTreeViewModel::UndoLastBatch);
    connect(rescan, &QPushButton::clicked, &viewModel_, &AddonTreeViewModel::ShowActiveProfile);

    auto* layout = new QHBoxLayout(bar);
    layout->addWidget(enable);
    layout->addWidget(disable);
    layout->addStretch();
    layout->addWidget(undo_);
    layout->addWidget(rescan);

    return bar;
}

QWidget* AddonTreePage::CreateInvite()
{
    auto* invite = new QWidget(this);

    auto* headline = new QLabel(tr("Este perfil ainda não tem biblioteca."), invite);
    QFont headlineFont = headline->font();
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    headline->setAlignment(Qt::AlignHCenter);

    auto* explanation = new QLabel(
        tr("A biblioteca é a pasta raiz onde os seus addons ficam guardados, fora do simulador.\n"
            "As subpastas dela viram categorias."),
        invite);
    explanation->setAlignment(Qt::AlignHCenter);

    auto* add = new QPushButton(tr("Cadastrar biblioteca..."), invite);
    connect(add, &QPushButton::clicked, this, &AddonTreePage::BrowseForLibrary);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(add);
    buttons->addStretch();

    auto* layout = new QVBoxLayout(invite);
    layout->addStretch();
    layout->addWidget(headline);
    layout->addWidget(explanation);
    layout->addSpacing(12);
    layout->addLayout(buttons);
    layout->addStretch();

    return invite;
}

std::vector<const TreeNode*> AddonTreePage::Chosen(const TreeNode* clicked) const
{
    std::vector<const TreeNode*> nodes;

    for (const QModelIndex& position : tree_->selectionModel()->selectedIndexes())
    {
        if (const TreeNode* node = model_.NodeAt(position))
        {
            nodes.push_back(node);
        }
    }

    if (clicked == nullptr)
    {
        return nodes;
    }

    return std::ranges::find(nodes, clicked) == nodes.end()
               ? std::vector<const TreeNode*>{clicked}
               : nodes;
}

void AddonTreePage::ToggleSelection(const bool enable)
{
    const std::vector<const TreeNode*> nodes = Chosen(nullptr);
    if (nodes.empty())
    {
        emit StatusChanged(tr("Selecione ao menos um addon ou categoria."));
        return;
    }

    viewModel_.Toggle(nodes, enable);
}

void AddonTreePage::OnToggleRequested(const TreeNode* node) const
{
    viewModel_.Toggle(Chosen(node));
}

void AddonTreePage::OnBatchFinished(const std::vector<LinkOperationResult>& results)
{
    undo_->setEnabled(viewModel_.CanUndo());

    std::vector<LinkOperationResult> failed;
    std::ranges::copy_if(results, std::back_inserter(failed), [](const LinkOperationResult& result)
    {
        return !result.outcome.Succeeded();
    });

    const auto done = static_cast<int>(results.size() - failed.size());

    if (failed.empty())
    {
        emit StatusChanged(results.empty()
                               ? tr("Nada a fazer: a seleção já estava como você pediu.")
                               : tr("%n operação(ões) concluída(s).", nullptr, done));
        return;
    }

    QStringList lines;
    for (const LinkOperationResult& result : failed)
    {
        lines.append(Describe(result));
    }

    QMessageBox report(QMessageBox::Warning, tr("Nem tudo foi aplicado"),
                       tr("%n operação(ões) falhou(aram). Nada foi apagado.", nullptr,
                          static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    report.setInformativeText(tr("%n operação(ões) concluída(s).", nullptr, done));
    report.setDetailedText(lines.join('\n'));
    report.exec();

    emit StatusChanged(tr("%1 · %2")
                       .arg(tr("%n operação(ões) concluída(s)", nullptr, done),
                            tr("%n falhou(aram)", nullptr, static_cast<int>(failed.size()))));
}

void AddonTreePage::OnScanFinished()
{
    const bool empty = viewModel_.Profile().libraries.empty();
    pages_->setCurrentIndex(empty ? 1 : 0);

    if (empty)
    {
        emit StatusChanged(tr("Cadastre uma biblioteca para começar."));
        return;
    }

    std::size_t addons = 0;
    std::size_t enabled = 0;

    for (const TreeNode& library : model_.Snapshot().libraries)
    {
        for (const TreeNode* addon : AddonsUnder(library))
        {
            ++addons;
            enabled += model_.Snapshot().enabled.Contains(addon->path) ? 1 : 0;
        }
    }

    tree_->expandToDepth(0);

    emit StatusChanged(tr("%1 · %2 habilitado(s)")
                       .arg(tr("%n addon(s) na biblioteca", nullptr, static_cast<int>(addons)))
                       .arg(enabled));
}

void AddonTreePage::ShowDestinationMenu(const QPoint& where)
{
    const TreeNode* node = model_.NodeAt(tree_->indexAt(where));
    const SimulatorProfile& profile = viewModel_.Profile();

    if (node == nullptr || profile.destinations.size() < 2)
    {
        return;
    }

    QMenu menu(this);
    menu.addAction(tr("Herdar o destino de cima"), this,
                   [this, node] { viewModel_.OverrideDestination(node, {}); });
    menu.addSeparator();

    for (const std::filesystem::path& destination : profile.destinations)
    {
        menu.addAction(tr("Ligar em %1").arg(Show(destination.filename())), this,
                       [this, node, destination] { viewModel_.OverrideDestination(node, destination); });
    }

    menu.exec(tree_->viewport()->mapToGlobal(where));
}

void AddonTreePage::BrowseForLibrary()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Escolha a pasta da biblioteca"));
    if (chosen.isEmpty())
    {
        return;
    }

    const LibraryReport report = viewModel_.AddLibrary(std::filesystem::path(chosen.toStdWString()));

    if (!report.Accepted())
    {
        QMessageBox::warning(
            this, tr("Biblioteca repetida"),
            tr("Essa pasta já está dentro de uma biblioteca cadastrada. Escolha a pasta raiz onde os "
                "addons ficam guardados; as subpastas dela viram categorias."));
        return;
    }

    QMessageBox::information(
        this, tr("Biblioteca cadastrada"),
        tr("%1 — %2, %3")
        .arg(chosen, tr("%n categoria(s)", nullptr, static_cast<int>(report.categories)),
             tr("%n addon(s)", nullptr, static_cast<int>(report.addons))));
}
