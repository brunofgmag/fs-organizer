#include "view/PresetsPage.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include "view/TableColumns.h"
#include "view/WheelGuard.h"

namespace
{
    constexpr int kAddonColumn = 0;
    constexpr int kActionColumn = 2;

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
    names_ = new QListWidget(this);

    auto* create = new QPushButton(tr("Novo a partir dos habilitados"), this);
    update_ = new QPushButton(tr("Atualizar com os habilitados"), this);
    rename_ = new QPushButton(tr("Renomear"), this);
    remove_ = new QPushButton(tr("Excluir"), this);

    auto* side = new QVBoxLayout;
    side->addWidget(names_, 1);
    side->addWidget(create);
    side->addWidget(update_);
    side->addWidget(rename_);
    side->addWidget(remove_);

    mode_ = new QComboBox(this);
    mode_->addItem(tr("Deixar só o que o preset liga"), static_cast<int>(ApplyMode::Replace));
    mode_->addItem(tr("Ligar o do preset, sem mexer no resto"), static_cast<int>(ApplyMode::Cumulative));
    mode_->addItem(tr("Desligar o que o preset liga"), static_cast<int>(ApplyMode::Disable));
    LetTheWheelScrollPastUnlessTheWidgetHasFocus(mode_);

    entries_ = new QTableWidget(this);
    entries_->setColumnCount(3);
    entries_->setHorizontalHeaderLabels({tr("Addon"), tr("Biblioteca"), tr("Liga")});
    entries_->setSelectionBehavior(QAbstractItemView::SelectRows);
    entries_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    entries_->setItemDelegateForColumn(kActionColumn, new CenteredCheckDelegate(entries_));
    LetTheColumnsBeDraggedAndStillFillTheTable(entries_, kAddonColumn);
    entries_->verticalHeader()->setVisible(false);

    preview_ = new QLabel(this);
    preview_->setWordWrap(true);

    apply_ = new QPushButton(tr("Aplicar"), this);

    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("Modo:"), this));
    bar->addWidget(mode_);
    bar->addStretch(1);
    bar->addWidget(apply_);

    auto* detail = new QVBoxLayout;
    detail->addLayout(bar);
    detail->addWidget(entries_, 1);
    detail->addWidget(preview_);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(side);
    layout->addLayout(detail, 1);

    connect(create, &QPushButton::clicked, this, &PresetsPage::CreateFromWhatIsEnabled);
    connect(update_, &QPushButton::clicked, this, &PresetsPage::UpdateFromWhatIsEnabled);
    connect(rename_, &QPushButton::clicked, this, &PresetsPage::RenameSelected);
    connect(remove_, &QPushButton::clicked, this, &PresetsPage::RemoveSelected);
    connect(apply_, &QPushButton::clicked, this, &PresetsPage::ApplySelected);
    connect(names_, &QListWidget::currentRowChanged, this,
            [this]
            {
                ShowSelected();
            });
    connect(mode_, &QComboBox::currentIndexChanged, this,
            [this]
            {
                RefreshPreview();
            });

    connect(entries_, &QTableWidget::itemChanged, this, &PresetsPage::ActionToggled);

    connect(&viewModel_, &PresetViewModel::Changed, this, &PresetsPage::ReloadNames);
    connect(&notifier, &SessionNotifier::Refreshed, this, &PresetsPage::RefreshPreview);
    connect(&notifier, &SessionNotifier::ScanFinished, this, &PresetsPage::ReloadNames);
    connect(&viewModel_, &PresetViewModel::Refused, this,
            [this](const QString& explanation)
            {
                QMessageBox::information(this, tr("Nada foi alterado"), explanation);
            });
    connect(&viewModel_, &PresetViewModel::SimulatorIsRunning, this,
            [this]
            {
                QMessageBox::information(this, tr("O simulador está aberto"),
                                         tr("As mudanças só valem depois de reiniciar o simulador."));
            });
    connect(&viewModel_, &PresetViewModel::RestartPendingChanged, this,
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

QString PresetsPage::SelectedName() const
{
    const QListWidgetItem* item = names_->currentItem();

    return item == nullptr ? QString{} : item->text();
}

ApplyMode PresetsPage::Mode() const
{
    return static_cast<ApplyMode>(mode_->currentData().toInt());
}

void PresetsPage::ReloadNames()
{
    const QString wanted = SelectedName();

    populating_ = true;
    names_->clear();
    names_->addItems(viewModel_.Names());

    const QList<QListWidgetItem*> found = names_->findItems(wanted, Qt::MatchExactly);
    names_->setCurrentRow(found.isEmpty() ? (names_->count() > 0 ? 0 : -1) : names_->row(found.front()));
    populating_ = false;

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

void PresetsPage::ActionToggled(QTableWidgetItem* item)
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

void PresetsPage::RefreshPreview()
{
    if (!selected_.has_value())
    {
        preview_->clear();
        apply_->setText(tr("Aplicar"));
        return;
    }

    const PresetPreview preview = viewModel_.Preview(*selected_, Mode());

    preview_->setText(CountsOf(preview));
    apply_->setText(tr("Aplicar — liga %1, desliga %2").arg(preview.toEnable).arg(preview.toDisable));
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

void PresetsPage::UpdateFromWhatIsEnabled()
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
