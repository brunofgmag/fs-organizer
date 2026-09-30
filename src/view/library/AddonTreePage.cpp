#include "view/library/AddonTreePage.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <set>
#include <string>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QItemSelection>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QButtonGroup>
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
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "domain/support/PathUtils.h"
#include "domain/tree/AddonTree.h"
#include "domain/tree/EffectiveDestination.h"
#include "support/PathText.h"
#include "view/delegates/RowDelegate.h"
#include "view/library/DeleteDialog.h"
#include "view/library/LibraryRootDialog.h"
#include "view/library/SuggestionDialog.h"
#include "view/library/CoverageDialog.h"
#include "view/library/SharedAirportsDialog.h"
#include "view/library/StartupEntryDialog.h"
#include "view/library/SwapDialog.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/DependencySection.h"
#include "view/panels/EmptyState.h"
#include "view/panels/ModelRowDetail.h"
#include "view/panels/ScrollBarCap.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "view/WrappingRow.h"
#include "viewmodel/FailureText.h"
#include "viewmodel/RowTagRoles.h"
#include "viewmodel/SizeSummary.h"

namespace
{
    constexpr std::size_t kAskAboveThisMany = 10;
    constexpr int kAddonColumnWidth = 420;
    constexpr int kVersionColumnWidth = 92;
    constexpr int kFilterGroupGap = 16;
    constexpr int kSearchMinimum = 120;
    constexpr int kSearchMaximum = 220;
    constexpr std::array kStates{AddonStateFilter::All, AddonStateFilter::Enabled, AddonStateFilter::Disabled};

    [[nodiscard]] QString LabelOf(const AddonStateFilter state)
    {
        switch (state)
        {
        case AddonStateFilter::All: return QCoreApplication::translate("AddonTreePage", "All", "several addons");
        case AddonStateFilter::Enabled:
            return QCoreApplication::translate("AddonTreePage", "Enabled", "several addons");
        case AddonStateFilter::Disabled:
            return QCoreApplication::translate("AddonTreePage", "Disabled", "several addons");
        }

        return {};
    }

    void ReserveTheWidestCount(QToolButton& chip, const QString& label, const int digits)
    {
        chip.setText(QStringLiteral("%1 %2").arg(label, QString(digits, QLatin1Char('0'))));
        chip.setMinimumWidth(chip.sizeHint().width());
    }

    [[nodiscard]] std::vector<const TreeNode*> AddonsAmong(const std::vector<const TreeNode*>& nodes)
    {
        std::vector<const TreeNode*> addons;

        for (const TreeNode* node : nodes)
        {
            for (const TreeNode* addon : AddonsUnder(*node))
            {
                addons.push_back(addon);
            }
        }

        return addons;
    }

    void StartASection(QMenu& menu)
    {
        if (!menu.isEmpty())
        {
            menu.addSeparator();
        }
    }

    QString AskForACategoryName(QWidget* parent, const QString& title, const QString& current)
    {
        return QInputDialog::getText(parent, title, QObject::tr("Category name:"), QLineEdit::Normal, current);
    }

    [[nodiscard]] std::string CarriedTo(const std::string& key, const std::string& moved, const std::string& landing)
    {
        if (key == moved)
        {
            return landing;
        }

        if (key.size() > moved.size() && key.compare(0, moved.size(), moved) == 0 && key[moved.size()] == '/')
        {
            return landing + key.substr(moved.size());
        }

        return key;
    }

    [[nodiscard]] std::set<std::string>
    CarriedTo(const std::set<std::string>& keys, const std::string& moved, const std::string& landing)
    {
        std::set<std::string> carried;

        for (const std::string& key : keys)
        {
            carried.insert(CarriedTo(key, moved, landing));
        }

        return carried;
    }
}

AddonTreePage::AddonTreePage(AddonTreeViewModel& viewModel,
                             DeletionViewModel& deletion,
                             ImportViewModel& importViewModel,
                             CoverageViewModel& coverage,
                             AddonDocumentsViewModel& documents,
                             AddonTreeModel& model,
                             const SessionNotifier& notifier,
                             QWidget* parent)
    : QWidget(parent),
      viewModel_(viewModel),
      deletion_(deletion),
      importViewModel_(importViewModel),
      coverage_(coverage),
      documents_(documents),
      model_(model)
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
    header->setSectionResizeMode(AddonTreeModel::AddonColumn, QHeaderView::Interactive);
    header->setSectionResizeMode(AddonTreeModel::DestinationColumn, QHeaderView::Stretch);
    header->resizeSection(AddonTreeModel::AddonColumn, kAddonColumnWidth);
    header->resizeSection(AddonTreeModel::VersionColumn, kVersionColumnWidth);

    auto* browser = new QWidget(this);
    auto* browserLayout = new QVBoxLayout(browser);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(0);
    browserLayout->addWidget(CreateActions());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(tree_, 1);
    body->addWidget(CreatePanel());
    browserLayout->addLayout(body, 1);

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
                NoteSelection();
                ShowTheSelectedAddon();
            });

    connect(tree_->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &AddonTreePage::NoteScrolling);

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
                Recount();
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
    connect(&viewModel_, &AddonTreeViewModel::LibraryRegistered, this, &AddonTreePage::SayTheLibraryWasRegistered);
    connect(&viewModel_, &AddonTreeViewModel::Shown, this, &AddonTreePage::OnShown);

    connect(&viewModel_, &AddonTreeViewModel::SizeMeasuring, this,
            [this]
            {
                ShowTheFields(tr("measuring…"));
            });

    connect(&viewModel_, &AddonTreeViewModel::SizeMeasured, this,
            [this](const SelectionSize& size)
            {
                ShowTheFields(SizeOfTheSelection(size));
            });

    connect(&model_, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&)
            {
                PublishSummary();
                Recount();
            });

    connect(&notifier, &SessionNotifier::ScanStarted, this,
            [this]
            {
                coverage_.StopChecking();

                emit StatusChanged(tr("Reading the library…"));
            });

    connect(&viewModel_, &AddonTreeViewModel::Refused, this,
            [this](const QString& explanation)
            {
                QMessageBox::warning(this, tr("Nothing changed"), explanation);
            });

    connect(&deletion_, &DeletionViewModel::Weighing, this,
            [this]
            {
                emit StatusChanged(tr("Measuring the selection…"));
            });

    connect(&deletion_, &DeletionViewModel::Deleting, this,
            [this]
            {
                emit StatusChanged(tr("Deleting the selection…"));
            });

    connect(&deletion_, &DeletionViewModel::Planned, this, &AddonTreePage::OfferToDelete);
    connect(&deletion_, &DeletionViewModel::Deleted, this, &AddonTreePage::OnDeleted);
    connect(&importViewModel_, &ImportViewModel::GaveBack, this, &AddonTreePage::OnGaveBack);

    connect(&coverage_, &CoverageViewModel::CheckProgressed, this,
            [this](const int read, const int outOf)
            {
                emit StatusChanged(tr("Checking the airports of what you enabled: %1 of %2").arg(read).arg(outOf));
            });

    connect(&coverage_, &CoverageViewModel::TurningThemOnWasChecked, this, &AddonTreePage::OnTurningThemOnWasChecked);

    counted_ = PopulationNow();

    RetranslateUi();
}

void AddonTreePage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        model_.Retranslate();
        ShowTheSelectedAddon();
        PublishSummary();
    }

    QWidget::changeEvent(event);
}

void AddonTreePage::RetranslateUi() const
{
    enable_->setText(tr("Enable selected"));
    disable_->setText(tr("Disable selected"));
    undo_->setText(tr("Undo last change"));
    rescan_->setText(tr("Refresh"));
    search_->setPlaceholderText(tr("Search addons…"));
    hideEmpty_->setText(tr("Hide empty categories"));
    ShowTheChips(PopulationNow());
    relink_->setText(tr("Repoint to the library"));
    moveTo_->setText(tr("Move to…"));
    openFolder_->setText(tr("Open folder"));
    ShowWhatTheDocumentationHolds();
    delete_->setText(tr("Delete…"));
    panel_->RenameTheFallback(tr("Addon selected"));
    invite_->Retell(tr("This profile has no library yet."),
                    tr("A library is a folder outside the simulator where you keep your addons. Enabling an addon "
                       "links it into the simulator."));
    inviteAction_->setText(tr("Add library…"));
}

QWidget* AddonTreePage::CreateActions()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("PageToolbar"));

    QSizePolicy roomForASecondRow(QSizePolicy::Preferred, QSizePolicy::Minimum);
    roomForASecondRow.setHeightForWidth(true);
    bar->setSizePolicy(roomForASecondRow);

    rescan_ = new QPushButton(bar);
    rescan_->setObjectName(QStringLiteral("LibraryRefresh"));
    rescan_->setProperty("role", "primary");
    enable_ = new QPushButton(bar);
    disable_ = new QPushButton(bar);
    undo_ = new QPushButton(bar);
    undo_->setEnabled(false);

    QWidget* stateFilter = CreateStateFilter(bar);

    hideEmpty_ = new QCheckBox(bar);
    hideEmpty_->setObjectName(QStringLiteral("LibraryHideEmpty"));
    hideEmpty_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    search_ = new QLineEdit(bar);
    search_->setObjectName(QStringLiteral("LibrarySearch"));
    search_->setClearButtonEnabled(true);
    search_->setMinimumWidth(kSearchMinimum);
    search_->setMaximumWidth(kSearchMaximum);

    connect(enable_, &QPushButton::clicked, this,
            [this]
            {
                ToggleSelection(true);
            });
    connect(disable_, &QPushButton::clicked, this,
            [this]
            {
                ToggleSelection(false);
            });
    connect(undo_, &QPushButton::clicked, &viewModel_, &AddonTreeViewModel::UndoLastBatch);
    connect(rescan_, &QPushButton::clicked, &viewModel_, &AddonTreeViewModel::ShowActiveProfile);
    connect(search_, &QLineEdit::textChanged, filter_, &AddonTreeFilterModel::Search);
    connect(hideEmpty_, &QCheckBox::toggled, filter_, &AddonTreeFilterModel::HideEmptyCategories);

    auto* layout = new WrappingRow(bar);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(kToolbarGap);
    layout->addWidget(rescan_);
    layout->addWidget(enable_);
    layout->addWidget(disable_);
    layout->addWidget(undo_);
    layout->AddSpring();
    layout->AddWidgetThatStepsDown(stateFilter);
    layout->AddSpringOnTheLowerLine();
    layout->AddWidgetThatStepsDown(hideEmpty_);
    layout->AddWidgetThatStepsDown(search_);

    return bar;
}

QWidget* AddonTreePage::CreateStateFilter(QWidget* bar)
{
    auto* holder = new QWidget(bar);
    holder->setObjectName(QStringLiteral("LibraryStateFilter"));
    holder->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* line = new QHBoxLayout(holder);
    line->setContentsMargins(0, 0, kFilterGroupGap - kToolbarGap, 0);
    line->setSpacing(kChipGap);

    auto* group = new QButtonGroup(holder);

    for (const AddonStateFilter state : kStates)
    {
        auto* chip = new QToolButton(holder);
        chip->setObjectName(QStringLiteral("FilterChip"));
        chip->setCheckable(true);
        chip->setChecked(state == AddonStateFilter::All);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        group->addButton(chip, static_cast<int>(state));
        line->addWidget(chip);
        chips_.append(chip);
    }

    connect(group, &QButtonGroup::idClicked, this,
            [this](const int id)
            {
                filter_->ShowOnly(static_cast<AddonStateFilter>(id));
            });

    return holder;
}

AddonTreePage::Population AddonTreePage::PopulationNow() const
{
    const auto all = static_cast<int>(model_.AddonCount());
    const auto enabled = static_cast<int>(model_.EnabledCount());

    return {.all = all, .enabled = enabled, .disabled = all - enabled};
}

int AddonTreePage::Population::Of(const AddonStateFilter state) const
{
    switch (state)
    {
    case AddonStateFilter::All: return all;
    case AddonStateFilter::Enabled: return enabled;
    case AddonStateFilter::Disabled: return disabled;
    }

    return 0;
}

void AddonTreePage::ShowTheChips(const Population& population) const
{
    const auto digits = static_cast<int>(QString::number(population.all).size());

    for (const AddonStateFilter state : kStates)
    {
        QToolButton* chip = chips_[static_cast<qsizetype>(state)];
        const QString label = LabelOf(state);
        const int count = population.Of(state);

        chip->ensurePolished();
        chip->setProperty("population", count == 0 ? "none" : "some");
        chip->style()->unpolish(chip);
        chip->style()->polish(chip);

        ReserveTheWidestCount(*chip, label, digits);
        chip->setText(QStringLiteral("%1 %2").arg(label).arg(count));
    }
}

void AddonTreePage::Recount()
{
    const Population before = counted_;
    const Population now = PopulationNow();

    if (now == before)
    {
        return;
    }

    counted_ = now;
    ShowTheChips(now);
    LeaveAStateThatRanOut(before, now);
}

void AddonTreePage::LeaveAStateThatRanOut(const Population& before, const Population& now)
{
    const AddonStateFilter showing = filter_->ShowingOnly();

    if (showing == AddonStateFilter::All || before.Of(showing) == 0 || now.Of(showing) > 0)
    {
        return;
    }

    chips_[static_cast<qsizetype>(AddonStateFilter::All)]->setChecked(true);
    filter_->ShowOnly(AddonStateFilter::All);

    const QString message = showing == AddonStateFilter::Enabled
        ? tr("No addon is enabled now, so the filter was cleared.")
        : tr("No addon is disabled now, so the filter was cleared.");

    QTimer::singleShot(0, this,
                       [this, message]
                       {
                           emit StatusChanged(message);
                       });
}

QWidget* AddonTreePage::CreateInvite()
{
    invite_ = new EmptyState(this);

    inviteAction_ = invite_->OfferTheOnlyAction();
    connect(inviteAction_, &QPushButton::clicked, this, &AddonTreePage::BrowseForLibrary);

    return invite_;
}

QWidget* AddonTreePage::CreatePanel()
{
    panel_ = new ContextPanel(tr("Addon selected"), 380, this);
    panel_->setObjectName(QStringLiteral("LibraryAddonPanel"));

    detail_ = new ModelRowDetail(panel_);
    dependencies_ = new DependencySection(panel_);

    relink_ = new QPushButton(panel_);
    relink_->setProperty("role", "primary");
    moveTo_ = new QPushButton(panel_);
    openFolder_ = new QPushButton(panel_);
    delete_ = new QPushButton(panel_);
    delete_->setObjectName(QStringLiteral("PanelDeleteAction"));
    documentation_ = new QPushButton(panel_);
    documentation_->setObjectName(QStringLiteral("PanelDocumentationAction"));
    documentation_->hide();

    panel_->Add(detail_);
    panel_->Add(dependencies_);
    panel_->Add(relink_);
    panel_->Add(moveTo_);
    panel_->Add(documentation_);
    panel_->Add(openFolder_);
    panel_->Add(delete_);

    panel_->RestoreCollapsedState();
    panel_->LevelWith(tree_->header());
    CapTheScrollBarOf(tree_, tree_->header());
    panel_->Summon(false);

    connect(relink_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.RelinkToTheProfileDestination(Chosen(nullptr));
            });
    connect(moveTo_, &QPushButton::clicked, this, &AddonTreePage::MoveTheSelectedAddon);
    connect(openFolder_, &QPushButton::clicked, this, &AddonTreePage::OpenTheSelectedFolder);
    connect(documentation_, &QPushButton::clicked, this,
            [this]
            {
                if (const TreeNode* node = Current(); node != nullptr)
                {
                    emit DocumentationRequested(AsUtf8(node->path.filename()));
                }
            });
    connect(&documents_, &AddonDocumentsViewModel::Indexed, this, &AddonTreePage::ShowWhatTheDocumentationHolds);
    connect(delete_, &QPushButton::clicked, this, &AddonTreePage::DeleteTheSelectedAddons);
    connect(panel_, &ContextPanel::CloseRequested, tree_->selectionModel(), &QItemSelectionModel::clearSelection);

    return panel_;
}

const TreeNode* AddonTreePage::Current() const
{
    return AddonTreeModel::NodeAt(filter_->mapToSource(tree_->selectionModel()->currentIndex()));
}

void AddonTreePage::ShowTheSelectedAddon()
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

    fields_.clear();
    fields_.append({tr("Category"), AsText(node->path.parent_path().filename())});
    fields_.append({tr("In the library"), AsText(node->path)});

    if (addon)
    {
        fields_.append({tr("Linked in"), AsText(destination / node->path.filename())});
        fields_.append({tr("Link"), broken ? tr("broken, the target folder is missing") : tr("working")});
        fields_.append(
            {tr("Enabled"), model_.data(source, AddonTreeModel::EnabledRole).toBool() ? tr("yes") : tr("no")});

        if (const QString version =
                model_.data(source.siblingAtColumn(AddonTreeModel::VersionColumn), Qt::DisplayRole).toString();
            !version.isEmpty())
        {
            fields_.append({tr("Version"), version});
        }
    }
    else
    {
        fields_.append({tr("Content"), tr("%n addon", nullptr, static_cast<int>(CountAddons(*node)))});
        fields_.append({tr("Destination"), AsText(destination.filename())});
    }

    panel_->ShowTitle(AsText(node->path.filename()), model_.data(source, AlarmingRole).toBool());
    ShowTheFields({});
    dependencies_->Show(viewModel_.DependenciesOf(node));

    documentation_->hide();

    if (addon)
    {
        documents_.Read(node->path);
    }

    viewModel_.MeasureTheSelection(model_.TallyOf({node}).addons);
}

void AddonTreePage::ShowTheSelectedBatch(const QModelIndexList& rows)
{
    const SelectionTally tally = model_.TallyOf(Chosen(nullptr));
    const auto addons = static_cast<int>(tally.addons.size());

    fields_.clear();

    if (addons > 0)
    {
        fields_.append({tr("Addons"), QString::number(addons)});
        fields_.append({tr("Enabled", "several addons"), tr("%1 of %2").arg(tally.enabled).arg(addons)});
    }

    if (tally.categories > 0)
    {
        fields_.append({tr("Categories"), QString::number(tally.categories)});
    }

    if (tally.broken > 0)
    {
        fields_.append({tr("Broken", "several addons"), QString::number(tally.broken)});
    }

    if (tally.strayed > 0)
    {
        fields_.append({tr("In another destination"), QString::number(tally.strayed)});
    }

    fields_.append({tr("Spread across"), tr("%n category", nullptr, static_cast<int>(tally.categoriesCrossed))});

    panel_->ShowTitle(tr("%n item selected", nullptr, static_cast<int>(rows.size())), tally.alarming);
    ShowTheFields({});
    dependencies_->Show({});

    viewModel_.MeasureTheSelection(tally.addons);
}

void AddonTreePage::ShowTheFields(const QString& size) const
{
    QList<ModelRowDetail::Field> fields = fields_;

    if (!size.isEmpty())
    {
        fields.append({tr("Size on disk"), size});
    }

    detail_->ShowFields(fields);
}

void AddonTreePage::ShowWhatTheActionsWillTouch(const QModelIndexList& rows) const
{
    int relinkable = 0;
    int deletable = 0;
    std::vector<const TreeNode*> addons;

    for (const QModelIndex& position : rows)
    {
        const QModelIndex source = filter_->mapToSource(position);
        const TreeNode* node = AddonTreeModel::NodeAt(source);

        if (node == nullptr || node->kind != TreeNodeKind::Addon)
        {
            continue;
        }

        ++deletable;

        relinkable += model_.data(source, AddonTreeModel::BrokenRole).toBool()
                || model_.data(source, AddonTreeModel::DivergentRole).toBool()
            ? 1
            : 0;
        addons.push_back(node);
    }

    const int movable = static_cast<int>(viewModel_.MovableAmong(addons));

    relink_->setEnabled(relinkable > 0);
    relink_->setText(relinkable > 1 ? tr("Repoint %n addon", nullptr, relinkable) : tr("Repoint to the library"));

    moveTo_->setEnabled(movable > 0);
    moveTo_->setText(movable > 1 ? tr("Move %n addon to…", nullptr, movable) : tr("Move to…"));

    openFolder_->setEnabled(rows.size() == 1);

    delete_->setEnabled(deletable > 0);
    delete_->setText(deletable > 1 ? tr("Delete %n addon…", nullptr, deletable) : tr("Delete…"));
}

void AddonTreePage::DeleteTheSelectedAddons()
{
    deletion_.PlanToDelete(Chosen(nullptr));
}

void AddonTreePage::OfferToDelete(const DeletionPlan& plan)
{
    if (plan.addons.empty())
    {
        emit StatusChanged(tr("Nothing to delete: the selection has no addons."));
        return;
    }

    DeleteDialog dialog(plan, deletion_, this);
    connect(&dialog, &DeleteDialog::GiveBackRequested, &importViewModel_, &ImportViewModel::GiveBack);
    static_cast<void>(dialog.exec());
}

void AddonTreePage::OnGaveBack(const std::vector<FileOperationResult>& results)
{
    QStringList lines;
    int failed = 0;

    for (const FileOperationResult& result : results)
    {
        if (!Succeeded(result.result))
        {
            lines.append(tr("%1: %2").arg(AsText(result.path.filename()), Explain(result.result)));
            ++failed;
        }
    }

    const int done = static_cast<int>(results.size()) - failed;

    if (failed == 0)
    {
        emit StatusChanged(tr("%n addon went back to the program that installed it.", nullptr, done));
        return;
    }

    QMessageBox dialog(QMessageBox::Warning, tr("Some addons were not given back"),
                       tr("%n addon is still in the library. Nothing was deleted.", nullptr, failed), QMessageBox::Ok,
                       this);
    dialog.setInformativeText(tr("%n addon went back to the program that installed it.", nullptr, done));
    dialog.setDetailedText(lines.join('\n'));
    dialog.exec();

    emit StatusChanged(
        tr("%1 · %2").arg(tr("%n given back", nullptr, done), tr("%n still in the library", nullptr, failed)));
}

void AddonTreePage::OnDeleted(const std::vector<DeletionResult>& results, const DeletionRoute route)
{
    QStringList lines;
    int failed = 0;

    for (const DeletionResult& result : results)
    {
        lines.append(Describe(result, route));
        failed += Succeeded(result.result) ? 0 : 1;
    }

    const int done = static_cast<int>(results.size()) - failed;

    if (failed == 0)
    {
        emit StatusChanged(tr("%n addon deleted.", nullptr, done));
        return;
    }

    QMessageBox dialog(QMessageBox::Warning, tr("Some addons were not deleted"),
                       tr("%n addon could not be deleted and is still in the library.", nullptr, failed),
                       QMessageBox::Ok, this);
    dialog.setInformativeText(tr("%n addon deleted.", nullptr, done));
    dialog.setDetailedText(lines.join('\n'));
    dialog.exec();

    emit StatusChanged(
        tr("%1 · %2").arg(tr("%n addon deleted", nullptr, done), tr("%n still in the library", nullptr, failed)));
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
                            viewModel_.MoveTo(SelectedNodes(node), target.category);
                        });
    }

    if (!where.isEmpty())
    {
        where.exec(moveTo_->mapToGlobal(QPoint(0, moveTo_->height())));
    }
}

void AddonTreePage::ShowWhatTheDocumentationHolds() const
{
    const std::size_t documents = documents_.Documents();
    const std::size_t charts = documents_.Charts();

    documentation_->setVisible(documents + charts > 0);
    documentation_->setText(tr("Documentation · %1 · %2")
                                .arg(tr("%n document(s)", nullptr, static_cast<int>(documents)),
                                     tr("%n chart(s)", nullptr, static_cast<int>(charts))));
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
    return ReachableAmong(SelectedNodes(clicked));
}

std::vector<const TreeNode*> AddonTreePage::SelectedNodes(const TreeNode* clicked) const
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

std::vector<const TreeNode*> AddonTreePage::ReachableAmong(const std::vector<const TreeNode*>& nodes) const
{
    if (!filter_->HidesAddons())
    {
        return nodes;
    }

    std::set<const TreeNode*> shown;
    CollectTheShownAddons({}, shown);

    std::vector<const TreeNode*> reachable;
    std::set<const TreeNode*> taken;

    for (const TreeNode* node : nodes)
    {
        for (const TreeNode* addon : AddonsUnder(*node))
        {
            if (shown.contains(addon) && taken.insert(addon).second)
            {
                reachable.push_back(addon);
            }
        }
    }

    return reachable;
}

void AddonTreePage::CollectTheShownAddons(const QModelIndex& parent, std::set<const TreeNode*>& found) const
{
    for (int row = 0; row < filter_->rowCount(parent); ++row)
    {
        const QModelIndex position = filter_->index(row, 0, parent);

        if (const TreeNode* node = AddonTreeModel::NodeAt(filter_->mapToSource(position));
            node != nullptr && node->kind == TreeNodeKind::Addon)
        {
            found.insert(node);
        }

        CollectTheShownAddons(position, found);
    }
}

void AddonTreePage::ToggleSelection(const bool enable)
{
    const std::vector<const TreeNode*> nodes = Chosen(nullptr);
    if (nodes.empty())
    {
        emit StatusChanged(tr("Select at least one addon or category."));
        return;
    }

    Apply(nodes, enable);
}

void AddonTreePage::OnToggleRequested(const TreeNode* node)
{
    const std::vector<const TreeNode*> nodes = Chosen(node);
    if (nodes.empty())
    {
        return;
    }

    Apply(nodes, viewModel_.WouldEnable(nodes));
}

void AddonTreePage::Apply(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    if (!TheUserMeantIt(nodes, enable))
    {
        return;
    }

    TogglePlan plan = viewModel_.PlanToggle(nodes, enable);
    const std::vector<TakenPlace> agreedSwaps = SwapsTheUserAgreedTo(plan.swapsNeeded);
    const std::vector<StartupLine> agreedEntries = StartupEntriesTheUserAgreedTo(nodes, enable);

    viewModel_.Toggle(nodes, enable, std::move(plan), agreedSwaps, agreedEntries);

    if (enable)
    {
        coverage_.CheckWhatWasTurnedOn(AddonsAmong(nodes));
    }
}

void AddonTreePage::OnTurningThemOnWasChecked(const WhatTurningThemOnFound& found)
{
    TurnOffWhatTheSimulatorAlsoCovers(found.alsoCovered);
    SayWhatTheLibraryAlreadyCovers(found.shared);
}

void AddonTreePage::SayWhatTheLibraryAlreadyCovers(const std::vector<SharedAirportsLine>& shared)
{
    if (shared.empty())
    {
        return;
    }

    SharedAirportsDialog dialog(shared, this);
    const std::vector<CoexistingPair> marked =
        dialog.exec() == QDialog::Accepted ? dialog.Chosen() : std::vector<CoexistingPair>{};

    if (marked.empty())
    {
        emit StatusChanged(tr("%n addon covers the same airport as another of your addons.", nullptr,
                              static_cast<int>(shared.size())));

        return;
    }

    coverage_.TheyCanAllCoexist(marked);

    emit StatusChanged(tr("No more warnings about %n pair.", nullptr, static_cast<int>(marked.size())));
}

void AddonTreePage::TurnOffWhatTheSimulatorAlsoCovers(const std::vector<CoverageLine>& covered)
{
    if (covered.empty())
    {
        return;
    }

    CoverageDialog dialog(covered, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        emit StatusChanged(
            tr("%n simulator airport stays enabled alongside yours.", nullptr, static_cast<int>(covered.size())));

        return;
    }

    std::vector<std::string> packageNames;
    packageNames.reserve(covered.size());

    for (const CoverageLine& line : covered)
    {
        packageNames.push_back(line.packageName);
    }

    if (const FileResult result = coverage_.SwitchAll(packageNames, false); !Succeeded(result))
    {
        emit StatusChanged(tr("The package list was not changed: %1.").arg(Explain(result)));

        return;
    }

    emit StatusChanged(tr("%n simulator airport disabled.", nullptr, static_cast<int>(covered.size())));
}

std::vector<TakenPlace> AddonTreePage::SwapsTheUserAgreedTo(const std::vector<TakenPlace>& swaps)
{
    if (swaps.empty())
    {
        return {};
    }

    SwapDialog dialog(swaps, viewModel_, this);

    viewModel_.WeighTheSwaps(swaps,
                             [&dialog](const std::vector<WeighedSwap>& weighed)
                             {
                                 dialog.ShowTheSizes(weighed);
                             });

    return dialog.exec() == QDialog::Accepted ? swaps : std::vector<TakenPlace>{};
}

std::vector<StartupLine> AddonTreePage::StartupEntriesTheUserAgreedTo(const std::vector<const TreeNode*>& nodes,
                                                                      const bool enable)
{
    if (enable)
    {
        return {};
    }

    const std::vector<StartupLine> carried = viewModel_.StartupEntriesAtRisk(nodes);
    if (carried.empty())
    {
        return {};
    }

    StartupEntryDialog dialog(carried, this);

    return dialog.exec() == QDialog::Accepted ? carried : std::vector<StartupLine>{};
}

bool AddonTreePage::TheUserMeantIt(const std::vector<const TreeNode*>& nodes, const bool enable)
{
    const std::size_t many = viewModel_.AddonsThatWouldChange(nodes, enable);

    if (many <= kAskAboveThisMany)
    {
        return true;
    }

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, enable ? tr("Enable addons") : tr("Disable addons"),
                              enable ? tr("Enable %1 addons?").arg(many) : tr("Disable %1 addons?").arg(many));

    return answer == QMessageBox::Yes;
}

void AddonTreePage::RefreshUndoState() const
{
    undo_->setEnabled(viewModel_.CanUndo());
}

QString AddonTreePage::NothingChangedBecause(const LinkBatchReport& report) const
{
    if (report.leftAlone > 0)
    {
        return tr("Nothing changed: %n addon was skipped because its place in the destination is taken.", nullptr,
                  static_cast<int>(report.leftAlone));
    }

    if (report.drifted == 0)
    {
        return tr("Nothing to do: the selection is already that way.");
    }

    return tr("Nothing changed: %n addon had changed on the disk. The list has been refreshed.", nullptr,
              static_cast<int>(report.drifted));
}

void AddonTreePage::OnBatchFinished(const LinkBatchReport& report)
{
    RefreshUndoState();

    std::vector<LinkOperationResult> failed;
    std::ranges::copy_if(report.results, std::back_inserter(failed),
                         [](const LinkOperationResult& result)
                         {
                             return !result.outcome.Succeeded();
                         });

    const auto done = static_cast<int>(report.results.size() - failed.size());

    if (report.results.empty())
    {
        emit StatusChanged(NothingChangedBecause(report));
        return;
    }

    if (failed.empty() && report.leftAlone > 0)
    {
        emit StatusChanged(tr("%1 · %2").arg(
            tr("%n applied", nullptr, done),
            tr("%n skipped because the place is taken", nullptr, static_cast<int>(report.leftAlone))));
        return;
    }

    if (failed.empty())
    {
        emit StatusChanged(tr("%n change applied.", nullptr, done));
        return;
    }

    QStringList lines;
    for (const LinkOperationResult& result : failed)
    {
        lines.append(Describe(result));
    }

    QMessageBox dialog(QMessageBox::Warning, tr("Some changes failed"),
                       tr("%n change failed. No file was deleted.", nullptr, static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    dialog.setInformativeText(tr("%n change applied.", nullptr, done));
    dialog.setDetailedText(lines.join('\n'));
    dialog.exec();

    emit StatusChanged(
        tr("%1 · %2").arg(tr("%n applied", nullptr, done), tr("%n failed", nullptr, static_cast<int>(failed.size()))));
}

void AddonTreePage::PublishSummary()
{
    if (viewModel_.Profile().libraries.empty())
    {
        emit SummaryChanged(tr("Add a library to get started."));
        emit MeterChanged(0, 0);
        return;
    }

    const auto addons = static_cast<int>(model_.AddonCount());
    const auto enabled = static_cast<int>(model_.EnabledCount());

    emit SummaryChanged(tr("%1 · %2").arg(tr("%n addon", nullptr, addons), tr("%n enabled", nullptr, enabled)));
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

    const int scrolled = scrolled_;

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

    if (RestoreSelection())
    {
        RestoreScrolling(scrolled);
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

void AddonTreePage::NoteSelection()
{
    if (rebuilding_)
    {
        return;
    }

    selected_.clear();
    current_.clear();

    for (const QModelIndex& position : tree_->selectionModel()->selectedRows())
    {
        if (const TreeNode* node = AddonTreeModel::NodeAt(filter_->mapToSource(position)))
        {
            selected_.insert(ComparablePath(node->path));
        }
    }

    if (const TreeNode* node = Current(); node != nullptr)
    {
        current_ = ComparablePath(node->path);
    }
}

void AddonTreePage::NoteScrolling(const int value)
{
    if (rebuilding_)
    {
        return;
    }

    scrolled_ = value;
}

void AddonTreePage::CarryTheRememberedPaths(const std::filesystem::path& from, const std::filesystem::path& to)
{
    const std::string moved = ComparablePath(from);
    const std::string landing = ComparablePath(to);

    if (moved == landing)
    {
        return;
    }

    expanded_ = CarriedTo(expanded_, moved, landing);
    selected_ = CarriedTo(selected_, moved, landing);
    current_ = CarriedTo(current_, moved, landing);
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

void AddonTreePage::GatherSelection(const QModelIndex& parent, QModelIndexList& found, QModelIndex& current) const
{
    for (int row = 0; row < filter_->rowCount(parent); ++row)
    {
        const QModelIndex position = filter_->index(row, 0, parent);

        if (const TreeNode* node = AddonTreeModel::NodeAt(filter_->mapToSource(position)))
        {
            const std::string key = ComparablePath(node->path);

            if (selected_.contains(key))
            {
                found.append(position);
            }

            if (key == current_)
            {
                current = position;
            }
        }

        GatherSelection(position, found, current);
    }
}

bool AddonTreePage::RestoreSelection()
{
    if (selected_.empty())
    {
        return true;
    }

    QModelIndexList found;
    QModelIndex current;
    GatherSelection({}, found, current);

    if (found.isEmpty())
    {
        selected_.clear();
        current_.clear();

        return false;
    }

    QItemSelection chosen;
    for (const QModelIndex& position : found)
    {
        chosen.select(position, position);
    }

    tree_->selectionModel()->setCurrentIndex(current.isValid() ? current : found.front(),
                                             QItemSelectionModel::NoUpdate);
    tree_->selectionModel()->select(chosen, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    return true;
}

void AddonTreePage::RestoreScrolling(const int value) const
{
    tree_->verticalScrollBar()->setValue(value);
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
    menu.addAction(
        chosen.theProvenanceIsAnotherProgram ? tr("Choose which copy to keep…") : tr("Resolve the conflict…"), this,
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

    QMenu* where = menu.addMenu(tr("Move to…"));
    for (const MoveTarget& target : offered)
    {
        where->addAction(AsText(target.relativePath), this,
                         [this, node, target]
                         {
                             viewModel_.MoveTo(SelectedNodes(node), target.category);
                         });
    }
}

void AddonTreePage::AddCategoryActions(QMenu& menu, const TreeNode* node)
{
    StartASection(menu);

    menu.addAction(tr("New category here…"), this,
                   [this, node]
                   {
                       viewModel_.CreateCategory(node, AskForACategoryName(this, tr("New category"), {}));
                   });

    if (node->kind != TreeNodeKind::Addon)
    {
        menu.addAction(tr("Suggest categories…"), this,
                       [this, node]
                       {
                           ShowSuggestions(node);
                       });
    }

    if (node->kind != TreeNodeKind::Category)
    {
        return;
    }

    menu.addAction(tr("Rename category…"), this,
                   [this, node]
                   {
                       const QString current = AsText(node->path.filename());
                       const std::filesystem::path from = node->path;

                       const std::filesystem::path landing =
                           viewModel_.RenameCategory(node, AskForACategoryName(this, tr("Rename category"), current));

                       if (!landing.empty())
                       {
                           CarryTheRememberedPaths(from, landing);
                       }
                   });

    if (AddonTreeViewModel::CanRemoveCategory(node))
    {
        menu.addAction(tr("Delete category"), this,
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
        QMessageBox::information(this, tr("Category suggestions"),
                                 tr("Every addon here is already in the suggested category."));
        return;
    }

    if (dialog.exec() == QDialog::Accepted)
    {
        viewModel_.ApplySuggestions(dialog.Chosen());
    }
}

void AddonTreePage::AddStrayActions(QMenu& menu, const TreeNode* node)
{
    const std::vector<const TreeNode*> strayed = viewModel_.StrayedUnder(Chosen(node));
    const bool theCategoryStrayed = node->kind == TreeNodeKind::Category && !viewModel_.StrayedUnder({node}).empty();

    if (strayed.empty() && !theCategoryStrayed)
    {
        return;
    }

    if (!strayed.empty())
    {
        menu.addAction(tr("Relink in the profile destination"), this,
                       [this, strayed]
                       {
                           viewModel_.RelinkStrayed(strayed);
                       });
    }

    if (theCategoryStrayed)
    {
        menu.addAction(tr("Keep the destination they are linked in"), this,
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

    menu.addAction(tr("Use the parent category's destination"), this,
                   [this, node]
                   {
                       ChooseDestination(node, {});
                   });
    menu.addSeparator();

    for (const std::filesystem::path& destination : profile.destinations)
    {
        menu.addAction(tr("Always use %1").arg(AsText(destination.filename())), this,
                       [this, node, destination]
                       {
                           ChooseDestination(node, destination);
                       });
    }
}

bool AddonTreePage::AskWhetherToRelink(const std::size_t strayed)
{
    QMessageBox question(
        QMessageBox::Question, tr("Destination changed"),
        tr("%n addon here is still linked in the previous destination.", nullptr, static_cast<int>(strayed)),
        QMessageBox::NoButton, this);

    const QPushButton* relink = question.addButton(tr("Relink now"), QMessageBox::AcceptRole);
    question.addButton(tr("Leave as is"), QMessageBox::RejectRole);
    question.exec();

    return question.clickedButton() == relink;
}

void AddonTreePage::ChooseDestination(const TreeNode* clicked, const std::filesystem::path& destination)
{
    viewModel_.OverrideDestination(SelectedNodes(clicked), destination);

    const std::vector<const TreeNode*> strayed = viewModel_.StrayedUnder(Chosen(clicked));

    if (!strayed.empty() && AskWhetherToRelink(strayed.size()))
    {
        viewModel_.RelinkStrayed(strayed);
    }
}

bool AddonTreePage::TheRootIsWorthKeeping(const std::filesystem::path& root)
{
    const RootDepth depth = MeasureTheRoot(root);
    if (depth.ItLeavesRoom())
    {
        return true;
    }

    LibraryRootDialog dialog(root, depth, this);

    return dialog.exec() == QDialog::Accepted;
}

void AddonTreePage::BrowseForLibrary()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Choose the library folder"));
    if (chosen.isEmpty())
    {
        return;
    }

    if (!TheRootIsWorthKeeping(AsPath(chosen)))
    {
        return;
    }

    if (!viewModel_.WouldAcceptLibrary(AsPath(chosen)))
    {
        QMessageBox::warning(this, tr("Already in a library"),
                             tr("That folder is inside a library you already added. Choose the top folder where you "
                                "keep your addons; its subfolders become categories."));
        return;
    }

    emit StatusChanged(tr("Reading the library folder…"));

    viewModel_.AddLibrary(AsPath(chosen));
}

void AddonTreePage::SayTheLibraryWasRegistered(const std::filesystem::path& path, const LibraryReport& report)
{
    if (!report.Accepted())
    {
        QMessageBox::warning(this, tr("Already in a library"),
                             tr("That folder is inside a library you already added. Choose the top folder where you "
                                "keep your addons; its subfolders become categories."));
        return;
    }

    QMessageBox::information(this, tr("Library added"),
                             tr("%1 · %2, %3")
                                 .arg(AsText(path), tr("%n category", nullptr, static_cast<int>(report.categories)),
                                      tr("%n addon", nullptr, static_cast<int>(report.addons))));
}
