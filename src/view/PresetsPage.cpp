#include "view/PresetsPage.h"

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include <QtWidgets/QStackedWidget>

#include "view/delegates/RowDelegate.h"
#include "view/TableColumns.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/EmptyState.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"

namespace
{
    constexpr int kAddonColumn = 0;
    constexpr int kActionColumn = 2;
    constexpr int kNameColumn = 0;
    constexpr int kContentColumn = 1;
    constexpr int kUpdatedColumn = 2;
    constexpr int kNameTableWidth = 380;

    class CenteredCheckDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem cell = option;
            initStyleOption(&cell, index);

            QStyle* style = cell.widget != nullptr ? cell.widget->style() : QApplication::style();

            QStyleOptionViewItem background = cell;
            background.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            style->drawControl(QStyle::CE_ItemViewItem, &background, painter, cell.widget);

            QStyleOptionViewItem check = cell;
            check.rect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &cell, cell.widget);
            check.rect.moveCenter(cell.rect.center());
            check.state &= ~QStyle::State_HasFocus;
            check.state |= cell.checkState == Qt::Checked ? QStyle::State_On : QStyle::State_Off;
            style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, painter, cell.widget);
        }

    protected:
        bool editorEvent(QEvent* event,
                         QAbstractItemModel* model,
                         const QStyleOptionViewItem& option,
                         const QModelIndex& index) override
        {
            if (!index.flags().testFlag(Qt::ItemIsUserCheckable) || !index.flags().testFlag(Qt::ItemIsEnabled))
            {
                return false;
            }

            if (event->type() == QEvent::MouseButtonRelease)
            {
                if (!option.rect.contains(static_cast<QMouseEvent*>(event)->position().toPoint()))
                {
                    return false;
                }
            }
            else if (event->type() == QEvent::KeyPress)
            {
                const int key = static_cast<QKeyEvent*>(event)->key();

                if (key != Qt::Key_Space && key != Qt::Key_Select)
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            const Qt::CheckState flipped =
                index.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked ? Qt::Unchecked : Qt::Checked;

            return model->setData(index, flipped, Qt::CheckStateRole);
        }
    };

    QString CountsOf(const PresetPreview& preview)
    {
        return QObject::tr("Liga %1, desliga %2. %3 já estão como o preset pede, %4 não foram encontrados, "
                           "e %5 entradas do destino este preset não toca.")
            .arg(preview.toEnable)
            .arg(preview.toDisable)
            .arg(preview.alreadyInPlace)
            .arg(preview.unresolved)
            .arg(preview.leftAlone);
    }
}

PresetsPage::PresetsPage(PresetViewModel& viewModel, const SessionNotifier& notifier, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel)
{
    names_ = CreateNameTable();

    auto* create = new QPushButton(tr("Novo a partir dos habilitados…"), this);
    create->setProperty("role", "primary");
    update_ = new QPushButton(tr("Atualizar com os habilitados"), this);
    rename_ = new QPushButton(tr("Renomear…"), this);
    remove_ = new QPushButton(tr("Excluir"), this);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    filter_ = new QLineEdit(this);
    filter_->setPlaceholderText(tr("Filtrar presets"));
    filter_->setClearButtonEnabled(true);
    filter_->setMinimumWidth(220);
    filter_->setMaximumWidth(280);

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(create);
    bar->addWidget(update_);
    bar->addWidget(rename_);
    bar->addWidget(remove_);
    bar->addStretch();
    bar->addWidget(filter_);

    entries_ = new QTableWidget(this);
    entries_->setObjectName(QStringLiteral("PresetEntries"));
    entries_->setColumnCount(3);
    entries_->setHorizontalHeaderLabels({tr("Addon"), tr("Biblioteca"), tr("Liga")});
    entries_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entries_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entries_->setItemDelegate(new RowDelegate(entries_));
    entries_->setItemDelegateForColumn(kActionColumn, new CenteredCheckDelegate(entries_));
    entries_->setShowGrid(false);
    LetTheColumnsBeDraggedAndStillFillTheTable(entries_, kAddonColumn);
    entries_->verticalHeader()->setVisible(false);
    DressTheHeaderOf(entries_->horizontalHeader());

    auto* panel = new ContextPanel(tr("Aplicar como"), 440, this);
    panel->setObjectName(QStringLiteral("PresetApplyPanel"));
    panel_ = panel;

    auto* heading = new QLabel(tr("Aplicar como"), panel);
    heading->setObjectName(QStringLiteral("PanelSubHeading"));

    modes_ = new QButtonGroup(panel);
    auto* replace = new QRadioButton(tr("Substituir"), panel);
    replace->setObjectName(QStringLiteral("ModeReplace"));
    auto* cumulative = new QRadioButton(tr("Acumular"), panel);
    cumulative->setObjectName(QStringLiteral("ModeCumulative"));
    auto* disable = new QRadioButton(tr("Desabilitar"), panel);
    disable->setObjectName(QStringLiteral("ModeDisable"));
    modes_->addButton(replace, static_cast<int>(ApplyMode::Replace));
    modes_->addButton(cumulative, static_cast<int>(ApplyMode::Cumulative));
    modes_->addButton(disable, static_cast<int>(ApplyMode::Disable));
    replace->setChecked(true);

    modeExplained_ = new QLabel(panel);
    modeExplained_->setObjectName(QStringLiteral("ModeExplained"));
    modeExplained_->setWordWrap(true);

    preview_ = new QLabel(panel);
    preview_->setWordWrap(true);

    auto* promise = new QLabel(tr("Aplicar é um lote só: \"Desfazer último lote\" volta tudo de uma vez."), panel);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    apply_ = new QPushButton(tr("Aplicar"), panel);
    apply_->setObjectName(QStringLiteral("PresetApply"));
    apply_->setProperty("role", "primary");
    apply_->setDefault(true);

    panel->Add(heading);
    panel->Add(replace);
    panel->Add(cumulative);
    panel->Add(disable);
    panel->Add(modeExplained_);
    panel->Add(preview_);
    panel->Add(apply_);
    panel->Add(promise);

    panel->RestoreCollapsedState();

    auto* tables = new QHBoxLayout;
    tables->setContentsMargins(0, 0, 0, 0);
    tables->setSpacing(0);
    tables->addWidget(names_);
    tables->addWidget(entries_, 1);
    tables->addWidget(panel);

    auto* kept = new QWidget(this);
    auto* keptLayout = new QVBoxLayout(kept);
    keptLayout->setContentsMargins(0, 0, 0, 0);
    keptLayout->setSpacing(0);
    keptLayout->addWidget(toolbar);
    keptLayout->addLayout(tables, 1);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(kept);

    auto* nothing = new EmptyState(tr("Nenhum preset neste perfil ainda."),
                                   tr("Um preset guarda quais addons ficam ligados. Habilite o que você quer "
                                      "voar e guarde essa combinação com um nome. Aplicar depois é um lote só, "
                                      "com desfazer inteiro."),
                                   this);
    connect(nothing->OfferTheOnlyAction(tr("Novo a partir dos habilitados…")), &QPushButton::clicked, this,
            &PresetsPage::CreateFromWhatIsEnabled);
    pages_->addWidget(nothing);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(pages_);

    connect(create, &QPushButton::clicked, this, &PresetsPage::CreateFromWhatIsEnabled);
    connect(update_, &QPushButton::clicked, this, &PresetsPage::UpdateFromWhatIsEnabled);
    connect(rename_, &QPushButton::clicked, this, &PresetsPage::RenameSelected);
    connect(remove_, &QPushButton::clicked, this, &PresetsPage::RemoveSelected);
    connect(apply_, &QPushButton::clicked, this, &PresetsPage::ApplySelected);
    connect(names_, &QTableWidget::currentCellChanged, this,
            [this](const int row, int, const int previous, int)
            {
                if (row != previous)
                {
                    ShowSelected();
                }
            });
    connect(modes_, &QButtonGroup::idClicked, this,
            [this]
            {
                RefreshPreview();
            });

    connect(filter_, &QLineEdit::textChanged, this, &PresetsPage::ShowOnlyTheNamesThatMatch);

    connect(entries_, &QTableWidget::itemChanged, this, &PresetsPage::ActionToggled);

    connect(&viewModel_, &PresetViewModel::Changed, this, &PresetsPage::ReloadNames);
    connect(&notifier, &SessionNotifier::Refreshed, this, &PresetsPage::RefreshPreview);
    connect(&notifier, &SessionNotifier::ScanFinished, this, &PresetsPage::ReloadNames);
    connect(&viewModel_, &PresetViewModel::Refused, this,
            [this](const QString& explanation)
            {
                QMessageBox::information(this, tr("Nada foi alterado"), explanation);
            });
    connect(&notifier, &SessionNotifier::SimulatorIsRunning, this,
            [this]
            {
                QMessageBox::information(this, tr("O simulador está aberto"),
                                         tr("As mudanças só valem depois de reiniciar o simulador."));
            });
    connect(&notifier, &SessionNotifier::RestartPendingChanged, this,
            [this](const bool pending)
            {
                if (pending)
                {
                    emit StatusChanged(tr("Reinicie o simulador para aplicar as mudanças."));
                }
            });
    connect(&viewModel_, &PresetViewModel::Applied, this,
            [this](const QStringList& unresolved)
            {
                if (!unresolved.isEmpty())
                {
                    QMessageBox::warning(this, tr("Entradas não encontradas"),
                                         tr("Estes addons do preset não existem mais na biblioteca:\n\n%1")
                                             .arg(unresolved.join(QStringLiteral("\n"))));
                }
            });

    ReloadNames();
}

QTableWidget* PresetsPage::CreateNameTable()
{
    auto* table = new QTableWidget(this);
    table->setObjectName(QStringLiteral("PresetNames"));
    table->setFixedWidth(kNameTableWidth);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({tr("Preset"), tr("Conteúdo"), tr("Atualizado")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setItemDelegate(new RowDelegate(table));
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    DressTheHeaderOf(table->horizontalHeader());
    LetTheColumnsBeDraggedAndStillFillTheTable(table, kNameColumn);

    return table;
}

void PresetsPage::ShowOnlyTheNamesThatMatch()
{
    const QString wanted = filter_->text().trimmed();
    int firstStanding = -1;

    for (int row = 0; row < names_->rowCount(); ++row)
    {
        const QTableWidgetItem* item = names_->item(row, kNameColumn);
        const bool matches = item != nullptr && item->text().contains(wanted, Qt::CaseInsensitive);

        names_->setRowHidden(row, !matches);

        if (matches && firstStanding < 0)
        {
            firstStanding = row;
        }
    }

    if (names_->currentRow() >= 0 && !names_->isRowHidden(names_->currentRow()))
    {
        return;
    }

    if (firstStanding < 0)
    {
        names_->setCurrentItem(nullptr);
        return;
    }

    names_->setCurrentCell(firstStanding, kNameColumn);
}

QString PresetsPage::SelectedName() const
{
    const QTableWidgetItem* item = names_->item(names_->currentRow(), kNameColumn);

    return item == nullptr ? QString{} : item->text();
}

ApplyMode PresetsPage::Mode() const
{
    return static_cast<ApplyMode>(modes_->checkedId());
}

void PresetsPage::ReloadNames()
{
    const QString wanted = SelectedName();
    const QList<PresetRow> rows = viewModel_.Rows();

    populating_ = true;
    names_->clearContents();
    names_->setRowCount(static_cast<int>(rows.size()));

    int landOn = rows.isEmpty() ? -1 : 0;

    for (int row = 0; row < rows.size(); ++row)
    {
        names_->setItem(row, kNameColumn, new QTableWidgetItem(rows[row].name));
        names_->setItem(row, kContentColumn, new QTableWidgetItem(rows[row].content));
        names_->setItem(row, kUpdatedColumn, new QTableWidgetItem(rows[row].updated));

        if (rows[row].name == wanted)
        {
            landOn = row;
        }
    }

    names_->setCurrentCell(landOn, kNameColumn);
    populating_ = false;

    ShowOnlyTheNamesThatMatch();

    pages_->setCurrentIndex(rows.isEmpty() ? 1 : 0);

    emit SummaryChanged(rows.isEmpty() ? tr("Nenhum preset neste perfil ainda.")
                                       : tr("%n preset(s) neste perfil.", nullptr, static_cast<int>(rows.size())));

    ShowSelected();
}

void PresetsPage::ShowSelected()
{
    if (populating_)
    {
        return;
    }

    const QString name = SelectedName();
    selected_ = name.isEmpty() ? std::nullopt : viewModel_.Load(name);

    const bool holdsOne = selected_.has_value();
    update_->setEnabled(holdsOne);
    rename_->setEnabled(holdsOne);
    remove_->setEnabled(holdsOne);
    apply_->setEnabled(holdsOne);
    panel_->Summon(holdsOne);
    panel_->ShowTitle(name);

    populating_ = true;
    entries_->setRowCount(holdsOne ? static_cast<int>(selected_->entries.size()) : 0);

    if (holdsOne)
    {
        for (int row = 0; row < static_cast<int>(selected_->entries.size()); ++row)
        {
            const PresetEntry& entry = selected_->entries[static_cast<std::size_t>(row)];
            entries_->setItem(row, kAddonColumn,
                              new QTableWidgetItem(QString::fromStdString(entry.addonId.folderName)));
            entries_->setItem(row, 1, new QTableWidgetItem(viewModel_.LibraryLabel(entry.addonId.libraryId)));

            auto* action = new QTableWidgetItem;
            action->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            action->setCheckState(entry.action == PresetAction::Disable ? Qt::Unchecked : Qt::Checked);

            entries_->setItem(row, kActionColumn, action);
        }
    }

    populating_ = false;

    RefreshPreview();
}

void PresetsPage::ActionToggled(const QTableWidgetItem* item)
{
    if (populating_ || item->column() != kActionColumn || !selected_.has_value())
    {
        return;
    }

    const auto row = static_cast<std::size_t>(item->row());

    if (row >= selected_->entries.size())
    {
        return;
    }

    const PresetAction wanted = item->checkState() == Qt::Checked ? PresetAction::Enable : PresetAction::Disable;

    if (!viewModel_.SetAction(SelectedName(), row, selected_->entries[row].addonId, wanted))
    {
        QMetaObject::invokeMethod(this, &PresetsPage::ReloadNames, Qt::QueuedConnection);
        return;
    }

    selected_->entries[row].action = wanted;

    RefreshPreview();
}

void PresetsPage::RefreshPreview() const
{
    switch (Mode())
    {
    case ApplyMode::Replace: modeExplained_->setText(tr("Deixa só o que o preset liga.")); break;
    case ApplyMode::Cumulative: modeExplained_->setText(tr("Liga o do preset, sem mexer no resto.")); break;
    case ApplyMode::Disable: modeExplained_->setText(tr("Desliga o que o preset liga.")); break;
    }

    if (!selected_.has_value())
    {
        preview_->clear();
        apply_->setText(tr("Aplicar"));
        return;
    }

    const PresetPreview preview = viewModel_.Preview(*selected_, Mode());

    preview_->setText(CountsOf(preview));
    apply_->setText(tr("Aplicar: liga %1, desliga %2").arg(preview.toEnable).arg(preview.toDisable));
}

void PresetsPage::CreateFromWhatIsEnabled()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("Novo preset"), tr("Nome:"), QLineEdit::Normal, {}, &accepted);

    if (accepted)
    {
        viewModel_.Create(name);
    }
}

void PresetsPage::UpdateFromWhatIsEnabled() const
{
    const QString name = SelectedName();

    if (!name.isEmpty())
    {
        viewModel_.Update(name);
    }
}

void PresetsPage::RenameSelected()
{
    const QString name = SelectedName();

    if (name.isEmpty())
    {
        return;
    }

    bool accepted = false;
    const QString wanted =
        QInputDialog::getText(this, tr("Renomear preset"), tr("Nome:"), QLineEdit::Normal, name, &accepted);

    if (accepted)
    {
        viewModel_.Rename(name, wanted);
    }
}

void PresetsPage::RemoveSelected()
{
    const QString name = SelectedName();

    if (name.isEmpty())
    {
        return;
    }

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Excluir preset"), tr("Excluir o preset \"%1\"?").arg(name));

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Remove(name);
    }
}

void PresetsPage::ApplySelected()
{
    if (!selected_.has_value())
    {
        return;
    }

    const ApplyMode mode = Mode();

    if (mode == ApplyMode::Replace)
    {
        const PresetPreview preview = viewModel_.Preview(*selected_, mode);
        const QMessageBox::StandardButton answer =
            QMessageBox::question(this, tr("Substituir o que está habilitado"),
                                  tr("%1\n\nAplicar o preset \"%2\"?").arg(CountsOf(preview), selected_->name.c_str()));

        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    viewModel_.Apply(*selected_, mode);
}
