#include "view/AddonTreePage.h"

#include <algorithm>
#include <ranges>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "domain/support/PathUtils.h"
#include "support/PathText.h"
#include "view/SuggestionDialog.h"
#include "viewmodel/FailureText.h"

namespace
{
    constexpr std::size_t kAskAboveThisMany = 10;

    void StartASection(QMenu& menu)
    {
        if (!menu.isEmpty())
        {
            menu.addSeparator();
        }
    }

    QString AskForACategoryName(QWidget* parent, const QString& title, const QString& current)
    {
        return QInputDialog::getText(parent, title, QObject::tr("Nome da categoria:"), QLineEdit::Normal, current);
    }
}

AddonTreePage::AddonTreePage(AddonTreeViewModel& viewModel,
                             AddonTreeModel& model,
                             const SessionNotifier& notifier,
                             QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), model_(model)
{
    tree_ = new QTreeView(this);
    filter_ = new AddonTreeFilterModel(this);
    filter_->setSourceModel(&model_);
    tree_->setModel(filter_);
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

    connect(tree_, &QTreeView::customContextMenuRequested, this, &AddonTreePage::ShowContextMenu);
    connect(&model_, &AddonTreeModel::ToggleRequested, this, &AddonTreePage::OnToggleRequested);

    connect(&model_, &QAbstractItemModel::modelAboutToBeReset, this,
            [this]
            {
                rebuilding_ = true;
            });

    connect(tree_, &QTreeView::expanded, this,
            [this](const QModelIndex& position)
            {
                NoteExpansion(position, true);
            });

    connect(tree_, &QTreeView::collapsed, this,
            [this](const QModelIndex& position)
            {
                NoteExpansion(position, false);
            });

    connect(&viewModel_, &AddonTreeViewModel::BatchFinished, this, &AddonTreePage::OnBatchFinished);
    connect(&viewModel_, &AddonTreeViewModel::Shown, this, &AddonTreePage::OnShown);

    connect(&notifier, &SessionNotifier::ScanStarted, this,
            [this]
            {
                emit StatusChanged(tr("Lendo a biblioteca..."));
            });

    connect(&viewModel_, &AddonTreeViewModel::Refused, this,
            [this](const QString& explanation)
            {
                QMessageBox::warning(this, tr("Nada foi alterado"), explanation);
            });

    connect(&viewModel_, &AddonTreeViewModel::SimulatorIsRunning, this,
            [this]
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

    auto* search = new QLineEdit(bar);
    search->setPlaceholderText(tr("Buscar addon..."));
    search->setClearButtonEnabled(true);
    search->setMaximumWidth(220);

    auto* hideEmpty = new QCheckBox(tr("Ocultar categorias vazias"), bar);

    undo_->setEnabled(false);

    connect(enable, &QPushButton::clicked, this,
            [this]
            {
                ToggleSelection(true);
            });
    connect(disable, &QPushButton::clicked, this,
            [this]
            {
                ToggleSelection(false);
            });
    connect(undo_, &QPushButton::clicked, &viewModel_, &AddonTreeViewModel::UndoLastBatch);
    connect(rescan, &QPushButton::clicked, &viewModel_, &AddonTreeViewModel::ShowActiveProfile);
    connect(search, &QLineEdit::textChanged, filter_, &AddonTreeFilterModel::Search);
    connect(hideEmpty, &QCheckBox::toggled, filter_, &AddonTreeFilterModel::HideEmptyCategories);

    auto* layout = new QHBoxLayout(bar);
    layout->addWidget(enable);
    layout->addWidget(disable);
    layout->addWidget(search);
    layout->addWidget(hideEmpty);
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

    auto* explanation =
        new QLabel(tr("A biblioteca é a pasta raiz onde os seus addons ficam guardados, fora do simulador.\n"
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
        if (const TreeNode* node = AddonTreeModel::NodeAt(filter_->mapToSource(position)))
        {
            nodes.push_back(node);
        }
    }

    if (clicked == nullptr)
    {
        return nodes;
    }

    return std::ranges::find(nodes, clicked) == nodes.end() ? std::vector<const TreeNode*>{clicked} : nodes;
}

void AddonTreePage::ToggleSelection(const bool enable)
{
    const std::vector<const TreeNode*> nodes = Chosen(nullptr);
    if (nodes.empty())
    {
        emit StatusChanged(tr("Selecione ao menos um addon ou categoria."));
        return;
    }

    if (TheUserMeantIt(nodes, enable))
    {
        viewModel_.Toggle(nodes, enable);
    }
}

void AddonTreePage::OnToggleRequested(const TreeNode* node)
{
    const std::vector<const TreeNode*> nodes = Chosen(node);
    const bool enable = viewModel_.WouldEnable(nodes);

    if (TheUserMeantIt(nodes, enable))
    {
        viewModel_.Toggle(nodes, enable);
    }
}

bool AddonTreePage::TheUserMeantIt(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    const std::size_t many = viewModel_.AddonsThatWouldChange(nodes, enable);

    if (many <= kAskAboveThisMany)
    {
        return true;
    }

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, enable ? tr("Ligar em massa") : tr("Desligar em massa"),
                              enable ? tr("Isto vai ligar %1 addons de uma vez.\n\nContinuar?").arg(many)
                                     : tr("Isto vai desligar %1 addons de uma vez.\n\nContinuar?").arg(many));

    return answer == QMessageBox::Yes;
}

void AddonTreePage::RefreshUndoState() const
{
    undo_->setEnabled(viewModel_.CanUndo());
}

void AddonTreePage::OnBatchFinished(const std::vector<LinkOperationResult>& results)
{
    RefreshUndoState();

    std::vector<LinkOperationResult> failed;
    std::ranges::copy_if(results, std::back_inserter(failed),
                         [](const LinkOperationResult& result)
                         {
                             return !result.outcome.Succeeded();
                         });

    const auto done = static_cast<int>(results.size() - failed.size());

    if (failed.empty())
    {
        emit StatusChanged(results.empty() ? tr("Nada a fazer: a seleção já estava como você pediu.")
                                           : tr("%n operação(ões) concluída(s).", nullptr, done));
        return;
    }

    QStringList lines;
    for (const LinkOperationResult& result : failed)
    {
        lines.append(Describe(result));
    }

    QMessageBox report(QMessageBox::Warning, tr("Nem tudo foi aplicado"),
                       tr("%n operação(ões) falhou(aram). Nada foi apagado.", nullptr, static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    report.setInformativeText(tr("%n operação(ões) concluída(s).", nullptr, done));
    report.setDetailedText(lines.join('\n'));
    report.exec();

    emit StatusChanged(tr("%1 · %2").arg(tr("%n operação(ões) concluída(s)", nullptr, done),
                                         tr("%n falhou(aram)", nullptr, static_cast<int>(failed.size()))));
}

void AddonTreePage::CountAddons(const QModelIndex& parent, std::size_t& addons, std::size_t& enabled) const
{
    for (int row = 0; row < model_.rowCount(parent); ++row)
    {
        const QModelIndex position = model_.index(row, 0, parent);
        const TreeNode* node = AddonTreeModel::NodeAt(position);

        if (node != nullptr && node->kind == TreeNodeKind::Addon)
        {
            ++addons;
            enabled += model_.data(position, AddonTreeModel::EnabledRole).toBool() ? 1 : 0;
        }

        CountAddons(position, addons, enabled);
    }
}

void AddonTreePage::OnShown()
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

    CountAddons({}, addons, enabled);

    rebuilding_ = false;

    if (shownOnce_)
    {
        RestoreExpansion({});
    }
    else
    {
        shownOnce_ = true;
        tree_->expandToDepth(0);
    }

    emit StatusChanged(tr("%1 · %2 habilitado(s)")
                           .arg(tr("%n addon(s) na biblioteca", nullptr, static_cast<int>(addons)))
                           .arg(enabled));
}

void AddonTreePage::NoteExpansion(const QModelIndex& position, const bool expanded)
{
    if (rebuilding_)
    {
        return;
    }

    const TreeNode* node = AddonTreeModel::NodeAt(filter_->mapToSource(position));
    if (node == nullptr)
    {
        return;
    }

    if (expanded)
    {
        expanded_.insert(ComparablePath(node->path));
    }
    else
    {
        expanded_.erase(ComparablePath(node->path));
    }
}

void AddonTreePage::CarryTheExpansion(const std::filesystem::path& from, const std::filesystem::path& to)
{
    const std::string moved = ComparablePath(from);
    const std::string landing = ComparablePath(to);

    if (moved == landing)
    {
        return;
    }

    std::set<std::string> carried;

    for (const std::string& open : expanded_)
    {
        if (open == moved)
        {
            carried.insert(landing);
        }
        else if (open.size() > moved.size() && open.compare(0, moved.size(), moved) == 0 && open[moved.size()] == '/')
        {
            carried.insert(landing + open.substr(moved.size()));
        }
        else
        {
            carried.insert(open);
        }
    }

    expanded_ = std::move(carried);
}

void AddonTreePage::RestoreExpansion(const QModelIndex& parent)
{
    for (int row = 0; row < filter_->rowCount(parent); ++row)
    {
        const QModelIndex position = filter_->index(row, 0, parent);
        const TreeNode* node = AddonTreeModel::NodeAt(filter_->mapToSource(position));

        if (node != nullptr && expanded_.contains(ComparablePath(node->path)))
        {
            tree_->setExpanded(position, true);
        }

        RestoreExpansion(position);
    }
}

void AddonTreePage::ShowContextMenu(const QPoint& where)
{
    const QModelIndex position = filter_->mapToSource(tree_->indexAt(where));
    const TreeNode* node = AddonTreeModel::NodeAt(position);

    if (node == nullptr)
    {
        return;
    }

    QMenu menu(this);

    AddConflictAction(menu, position);
    AddMoveAction(menu, node);
    AddCategoryActions(menu, node);
    AddDestinationActions(menu, node);

    if (menu.isEmpty())
    {
        return;
    }

    menu.exec(tree_->viewport()->mapToGlobal(where));
}

void AddonTreePage::AddConflictAction(QMenu& menu, const QModelIndex& position)
{
    const QVariant conflict = model_.data(position, AddonTreeModel::ConflictDetailsRole);
    if (!conflict.isValid())
    {
        return;
    }

    const auto chosen = conflict.value<CopyConflict>();
    menu.addAction(tr("Resolver o conflito de cópia..."), this,
                   [this, chosen]
                   {
                       emit ConflictChosen(chosen);
                   });
}

void AddonTreePage::AddMoveAction(QMenu& menu, const TreeNode* node)
{
    if (node->kind != TreeNodeKind::Addon)
    {
        return;
    }

    const std::vector<MoveTarget> offered = viewModel_.CategoriesFor(node);
    if (offered.empty())
    {
        return;
    }

    StartASection(menu);

    QMenu* where = menu.addMenu(tr("Mover para..."));
    for (const MoveTarget& target : offered)
    {
        where->addAction(AsText(target.relativePath), this,
                         [this, node, target]
                         {
                             viewModel_.MoveTo(Chosen(node), target.category);
                         });
    }
}

void AddonTreePage::AddCategoryActions(QMenu& menu, const TreeNode* node)
{
    StartASection(menu);

    menu.addAction(tr("Nova categoria aqui..."), this,
                   [this, node]
                   {
                       viewModel_.CreateCategory(node, AskForACategoryName(this, tr("Nova categoria"), {}));
                   });

    if (node->kind != TreeNodeKind::Addon)
    {
        menu.addAction(tr("Sugerir categorias..."), this,
                       [this, node]
                       {
                           ShowSuggestions(node);
                       });
    }

    if (node->kind != TreeNodeKind::Category)
    {
        return;
    }

    menu.addAction(tr("Renomear categoria..."), this,
                   [this, node]
                   {
                       const QString current = AsText(node->path.filename());
                       const std::filesystem::path from = node->path;

                       const std::filesystem::path landing = viewModel_.RenameCategory(
                           node, AskForACategoryName(this, tr("Renomear categoria"), current));

                       if (!landing.empty())
                       {
                           CarryTheExpansion(from, landing);
                       }
                   });

    if (viewModel_.CanRemoveCategory(node))
    {
        menu.addAction(tr("Apagar categoria"), this,
                       [this, node]
                       {
                           viewModel_.RemoveCategory(node);
                       });
    }
}

void AddonTreePage::ShowSuggestions(const TreeNode* node)
{
    SuggestionDialog dialog(viewModel_.SuggestionsFor(node), this);

    if (!dialog.HasAnythingToShow())
    {
        QMessageBox::information(this, tr("Sugestões de categoria"),
                                 tr("Nenhum addon daqui está numa categoria diferente da que as regras sugerem."));
        return;
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        viewModel_.ApplySuggestions(dialog.Chosen());
    }
}

void AddonTreePage::AddStrayActions(QMenu& menu, const TreeNode* node)
{
    if (viewModel_.StrayAddonsUnder({node}) == 0)
    {
        return;
    }

    menu.addAction(tr("Religar no destino do perfil"), this,
                   [this, node]
                   {
                       viewModel_.RelinkToTheProfileDestination(Chosen(node));
                   });

    if (node->kind == TreeNodeKind::Category)
    {
        menu.addAction(tr("Adotar o destino em que os addons já estão"), this,
                       [this, node]
                       {
                           viewModel_.AdoptDestination(node);
                       });
    }

    menu.addSeparator();
}

void AddonTreePage::AddDestinationActions(QMenu& menu, const TreeNode* node)
{
    const SimulatorProfile& profile = viewModel_.Profile();
    if (profile.destinations.size() < 2)
    {
        return;
    }

    StartASection(menu);

    AddStrayActions(menu, node);

    menu.addAction(tr("Herdar o destino de cima"), this,
                   [this, node]
                   {
                       ChooseDestination(Chosen(node), {});
                   });
    menu.addSeparator();

    for (const std::filesystem::path& destination : profile.destinations)
    {
        menu.addAction(tr("Fixar o destino em %1").arg(AsText(destination.filename())), this,
                       [this, node, destination]
                       {
                           ChooseDestination(Chosen(node), destination);
                       });
    }
}

bool AddonTreePage::AskWhetherToRelink(const std::size_t strayed)
{
    QMessageBox question(QMessageBox::Question, tr("Destino alterado"),
                         tr("%n addon(s) daqui continuam ligados fora do destino que o perfil agora manda usar.",
                            nullptr, static_cast<int>(strayed)),
                         QMessageBox::NoButton, this);

    const QPushButton* relink = question.addButton(tr("Religar agora"), QMessageBox::AcceptRole);
    question.addButton(tr("Deixar como está"), QMessageBox::RejectRole);
    question.exec();

    return question.clickedButton() == relink;
}

void AddonTreePage::ChooseDestination(const std::vector<const TreeNode*>& nodes,
                                      const std::filesystem::path& destination)
{
    viewModel_.OverrideDestination(nodes, destination);

    const std::size_t strayed = viewModel_.StrayAddonsUnder(nodes);

    if (strayed > 0 && AskWhetherToRelink(strayed))
    {
        viewModel_.RelinkToTheProfileDestination(nodes);
    }
}

void AddonTreePage::BrowseForLibrary()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Escolha a pasta da biblioteca"));
    if (chosen.isEmpty())
    {
        return;
    }

    const LibraryReport report = viewModel_.AddLibrary(AsPath(chosen));

    if (!report.Accepted())
    {
        QMessageBox::warning(this, tr("Biblioteca repetida"),
                             tr("Essa pasta já está dentro de uma biblioteca cadastrada. Escolha a pasta raiz onde os "
                                "addons ficam guardados; as subpastas dela viram categorias."));
        return;
    }

    QMessageBox::information(this, tr("Biblioteca cadastrada"),
                             tr("%1 — %2, %3")
                                 .arg(chosen, tr("%n categoria(s)", nullptr, static_cast<int>(report.categories)),
                                      tr("%n addon(s)", nullptr, static_cast<int>(report.addons))));
}
