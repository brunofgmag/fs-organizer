#include "view/library/AddonTreePage.h"

#include <algorithm>
#include <ranges>

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
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
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "support/PathText.h"
#include "view/delegates/RowDelegate.h"
#include "view/library/SuggestionDialog.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/EmptyState.h"
#include "view/panels/ModelRowDetail.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr std::size_t kAskAboveThisMany = 10;
    constexpr int kVersionColumnWidth = 92;
    constexpr int kDestinationColumnWidth = 168;

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
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setUniformRowHeights(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setItemDelegate(new RowDelegate(tree_));
    tree_->setIndentation(20);
    tree_->setExpandsOnDoubleClick(true);

    QHeaderView* header = tree_->header();
    DressTheHeaderOf(header);
    header->setStretchLastSection(false);
    header->setSectionResizeMode(AddonTreeModel::AddonColumn, QHeaderView::Stretch);
    header->resizeSection(AddonTreeModel::VersionColumn, kVersionColumnWidth);
    header->resizeSection(AddonTreeModel::DestinationColumn, kDestinationColumnWidth);

    auto* browser = new QWidget(this);
    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(CreateActions());
    column->addWidget(tree_, 1);

    auto* browserLayout = new QHBoxLayout(browser);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(0);
    browserLayout->addLayout(column, 1);
    browserLayout->addWidget(CreatePanel());

    pages_ = new QStackedWidget(this);
    pages_->addWidget(browser);
    pages_->addWidget(CreateInvite());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(pages_);

    setFocusProxy(tree_);

    connect(tree_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]
            {
                ShowTheSelectedAddon();
            });

    connect(tree_, &QTreeView::customContextMenuRequested, this, &AddonTreePage::ShowContextMenu);
    connect(&model_, &AddonTreeModel::ToggleRequested, this, &AddonTreePage::OnToggleRequested);

    connect(&model_, &QAbstractItemModel::modelAboutToBeReset, this,
            [this]
            {
                rebuilding_ = true;
            });

    connect(&model_, &QAbstractItemModel::modelReset, this,
            [this]
            {
                ShowTheSelectedAddon();
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

    connect(&model_, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&)
            {
                PublishSummary();
            });

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

    connect(&notifier, &SessionNotifier::SimulatorIsRunning, this,
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
    bar->setObjectName(QStringLiteral("PageToolbar"));

    auto* enable = new QPushButton(tr("Marcar selecionados"), bar);
    auto* disable = new QPushButton(tr("Desmarcar selecionados"), bar);
    undo_ = new QPushButton(tr("Desfazer último lote"), bar);
    auto* rescan = new QPushButton(tr("Reler do disco"), bar);
    rescan->setProperty("role", "primary");

    auto* search = new QLineEdit(bar);
    search->setPlaceholderText(tr("Buscar addon..."));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(180);
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
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(8);
    layout->addWidget(rescan);
    layout->addWidget(enable);
    layout->addWidget(disable);
    layout->addWidget(undo_);
    layout->addStretch();
    layout->addWidget(hideEmpty);
    layout->addWidget(search);

    return bar;
}

QWidget* AddonTreePage::CreateInvite()
{
    auto* invite = new EmptyState(tr("Este perfil ainda não tem biblioteca."),
                                  tr("Uma biblioteca é a pasta onde os seus addons moram, fora do simulador. "
                                     "Habilitar um addon cria um link do simulador para lá — nada é copiado "
                                     "nem movido."),
                                  this);

    connect(invite->OfferTheOnlyAction(tr("Cadastrar biblioteca...")), &QPushButton::clicked, this,
            &AddonTreePage::BrowseForLibrary);

    return invite;
}

QWidget* AddonTreePage::CreatePanel()
{
    panel_ = new ContextPanel(tr("Addon selecionado"), 380, this);
    panel_->setObjectName(QStringLiteral("LibraryAddonPanel"));

    detail_ = new ModelRowDetail(panel_);

    relink_ = new QPushButton(tr("Re-apontar para a biblioteca"), panel_);
    relink_->setProperty("role", "primary");
    moveTo_ = new QPushButton(tr("Mover para..."), panel_);
    openFolder_ = new QPushButton(tr("Abrir a pasta"), panel_);

    auto* promise = new QLabel(tr("Reparar nunca toca os arquivos reais: só o nó de reparse é reescrito."), panel_);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    panel_->Add(detail_);
    panel_->Add(relink_);
    panel_->Add(moveTo_);
    panel_->Add(openFolder_);
    panel_->Add(promise);

    panel_->RestoreCollapsedState();
    panel_->Summon(false);

    connect(relink_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.RelinkToTheProfileDestination(Chosen(nullptr));
            });
    connect(moveTo_, &QPushButton::clicked, this, &AddonTreePage::MoveTheSelectedAddon);
    connect(openFolder_, &QPushButton::clicked, this, &AddonTreePage::OpenTheSelectedFolder);
    connect(panel_, &ContextPanel::CloseRequested, tree_->selectionModel(), &QItemSelectionModel::clearSelection);

    return panel_;
}

const TreeNode* AddonTreePage::Current() const
{
    return AddonTreeModel::NodeAt(filter_->mapToSource(tree_->selectionModel()->currentIndex()));
}

void AddonTreePage::ShowTheSelectedAddon() const
{
    const QModelIndexList chosen = tree_->selectionModel()->selectedRows();
    const TreeNode* node = chosen.isEmpty() ? nullptr : AddonTreeModel::NodeAt(filter_->mapToSource(chosen.front()));

    panel_->Summon(node != nullptr);
    ShowWhatTheActionsWillTouch(chosen);

    if (node == nullptr)
    {
        return;
    }

    if (chosen.size() > 1)
    {
        ShowTheSelectedBatch(chosen);
        return;
    }

    const std::filesystem::path destination = EffectiveDestination(viewModel_.Profile(), node->path);
    const QModelIndex source = filter_->mapToSource(chosen.front());
    const bool addon = node->kind == TreeNodeKind::Addon;
    const bool broken = model_.data(source, AddonTreeModel::BrokenRole).toBool();

    QList<ModelRowDetail::Field> fields;
    fields.append({tr("Categoria"), AsText(node->path.parent_path().filename())});
    fields.append({tr("Na biblioteca"), AsText(node->path)});

    if (addon)
    {
        fields.append({tr("Link em"), AsText(destination / node->path.filename())});
        fields.append({tr("Alvo existe"), broken ? tr("não — o link não acha a pasta") : tr("sim")});
        fields.append(
            {tr("Habilitado"), model_.data(source, AddonTreeModel::EnabledRole).toBool() ? tr("sim") : tr("não")});

        if (const QString version =
                model_.data(source.siblingAtColumn(AddonTreeModel::VersionColumn), Qt::DisplayRole).toString();
            !version.isEmpty())
        {
            fields.append({tr("Versão"), version});
        }
    }
    else
    {
        fields.append({tr("Conteúdo"), tr("%n addon(s)", nullptr, static_cast<int>(CountAddons(*node)))});
        fields.append({tr("Destino"), AsText(destination.filename())});
    }

    panel_->ShowTitle(AsText(node->path.filename()), model_.data(source, AlarmingRole).toBool());
    detail_->ShowFields(fields);
}

void AddonTreePage::ShowTheSelectedBatch(const QModelIndexList& rows) const
{
    int addons = 0;
    int categories = 0;
    int enabled = 0;
    int broken = 0;
    int strayed = 0;
    bool alarming = false;
    QSet<QString> categoriesCrossed;

    for (const QModelIndex& position : rows)
    {
        const QModelIndex source = filter_->mapToSource(position);
        const TreeNode* node = AddonTreeModel::NodeAt(source);

        if (node == nullptr)
        {
            continue;
        }

        alarming = alarming || model_.data(source, AlarmingRole).toBool();

        if (node->kind != TreeNodeKind::Addon)
        {
            ++categories;
            continue;
        }

        ++addons;
        enabled += model_.data(source, AddonTreeModel::EnabledRole).toBool() ? 1 : 0;
        broken += model_.data(source, AddonTreeModel::BrokenRole).toBool() ? 1 : 0;
        strayed += model_.data(source, AddonTreeModel::DivergentRole).toBool() ? 1 : 0;
        categoriesCrossed.insert(AsText(node->path.parent_path().filename()));
    }

    QList<ModelRowDetail::Field> fields;

    if (addons > 0)
    {
        fields.append({tr("Addons"), QString::number(addons)});
        fields.append({tr("Habilitados"), tr("%1 de %2").arg(enabled).arg(addons)});
    }

    if (categories > 0)
    {
        fields.append({tr("Categorias"), QString::number(categories)});
    }

    if (broken > 0)
    {
        fields.append({tr("Quebrados"), QString::number(broken)});
    }

    if (strayed > 0)
    {
        fields.append({tr("Fora do destino"), QString::number(strayed)});
    }

    fields.append({tr("Espalhados por"), tr("%n categoria(s)", nullptr, categoriesCrossed.size())});

    panel_->ShowTitle(tr("%n item(ns) selecionado(s)", nullptr, static_cast<int>(rows.size())), alarming);
    detail_->ShowFields(fields);
}

void AddonTreePage::ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const
{
    int relinkable = 0;
    int movable = 0;

    for (const QModelIndex& position : rows)
    {
        const QModelIndex source = filter_->mapToSource(position);
        const TreeNode* node = AddonTreeModel::NodeAt(source);

        if (node == nullptr || node->kind != TreeNodeKind::Addon)
        {
            continue;
        }

        relinkable += model_.data(source, AddonTreeModel::BrokenRole).toBool()
                || model_.data(source, AddonTreeModel::DivergentRole).toBool()
            ? 1
            : 0;
        movable += viewModel_.CategoriesFor(node).empty() ? 0 : 1;
    }

    relink_->setEnabled(relinkable > 0);
    relink_->setText(relinkable > 1 ? tr("Re-apontar %n addons", nullptr, relinkable)
                                    : tr("Re-apontar para a biblioteca"));

    moveTo_->setEnabled(movable > 0);
    moveTo_->setText(movable > 1 ? tr("Mover %n addons para...", nullptr, movable) : tr("Mover para..."));

    openFolder_->setEnabled(rows.size() == 1);
}

void AddonTreePage::MoveTheSelectedAddon()
{
    const TreeNode* node = Current();
    if (node == nullptr)
    {
        return;
    }

    QMenu where(this);

    for (const MoveTarget& target : viewModel_.CategoriesFor(node))
    {
        where.addAction(AsText(target.relativePath), this,
                        [this, node, target]
                        {
                            viewModel_.MoveTo(Chosen(node), target.category);
                        });
    }

    if (!where.isEmpty())
    {
        where.exec(moveTo_->mapToGlobal(QPoint(0, moveTo_->height())));
    }
}

void AddonTreePage::OpenTheSelectedFolder() const
{
    if (const TreeNode* node = Current(); node != nullptr)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(node->path)));
    }
}

std::vector<const TreeNode*> AddonTreePage::Chosen(const TreeNode* clicked) const
{
    std::vector<const TreeNode*> nodes;

    for (const QModelIndex& position : tree_->selectionModel()->selectedRows())
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

void AddonTreePage::PublishSummary()
{
    if (viewModel_.Profile().libraries.empty())
    {
        emit SummaryChanged(tr("Cadastre uma biblioteca para começar."));
        emit MeterChanged(0, 0);
        return;
    }

    const auto addons = static_cast<int>(model_.AddonCount());
    const auto enabled = static_cast<int>(model_.EnabledCount());

    emit SummaryChanged(tr("%1 addons · %2 habilitados").arg(addons).arg(enabled));
    emit MeterChanged(enabled, addons);
}

void AddonTreePage::OnShown()
{
    const bool empty = viewModel_.Profile().libraries.empty();
    pages_->setCurrentIndex(empty ? 1 : 0);

    if (empty)
    {
        PublishSummary();
        return;
    }

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

    PublishSummary();
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
