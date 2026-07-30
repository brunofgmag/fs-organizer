#include "view/JournalPage.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "view/PlainTextDelegate.h"
#include "view/panels/ContextPanel.h"
#include "view/panels/ModelRowDetail.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"

JournalPage::JournalPage(JournalViewModel& viewModel, JournalModel& model, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), model_(model)
{
    filter_ = new JournalFilterModel(this);
    filter_->setSourceModel(&model_);

    operations_ = new QTreeView(this);
    operations_->setModel(filter_);
    operations_->setRootIsDecorated(true);
    operations_->setUniformRowHeights(true);
    operations_->setSelectionBehavior(QAbstractItemView::SelectRows);
    operations_->header()->setStretchLastSection(true);
    operations_->setItemDelegate(new PlainTextDelegate(operations_));
    DressTheHeaderOf(operations_->header());

    auto* search = new QLineEdit(this);
    search->setPlaceholderText(tr("Buscar addon, caminho ou operação..."));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(220);
    search->setMaximumWidth(280);

    auto* failuresOnly = new QCheckBox(tr("Só o que falhou"), this);
    auto* reload = new QPushButton(tr("Reler o diário"), this);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(search);
    bar->addWidget(failuresOnly);
    bar->addStretch();
    bar->addWidget(reload);

    panel_ = new ContextPanel(tr("Operação"), 400, this);
    panel_->setObjectName(QStringLiteral("JournalOperationPanel"));
    detail_ = new ModelRowDetail(panel_);

    auto* promise = new QLabel(tr("O diário é append-only. Nada nesta tela escreve no disco."), panel_);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    panel_->Add(detail_);
    panel_->Add(promise);
    panel_->RestoreCollapsedState();
    panel_->Summon(false);

    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(toolbar);
    column->addWidget(operations_, 1);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(column, 1);
    layout->addWidget(panel_);

    connect(operations_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &JournalPage::ShowTheSelectedOperation);
    connect(panel_, &ContextPanel::CloseRequested, operations_->selectionModel(), &QItemSelectionModel::clearSelection);
    connect(search, &QLineEdit::textChanged, filter_, &JournalFilterModel::Search);
    connect(failuresOnly, &QCheckBox::toggled, filter_, &JournalFilterModel::ShowOnlyWhatFailed);
    connect(reload, &QPushButton::clicked, &viewModel_, &JournalViewModel::Show);
    connect(&model_, &QAbstractItemModel::modelReset, this, &JournalPage::UpdateSummary);

    UpdateSummary();
}

void JournalPage::ShowTheSelectedOperation() const
{
    const QModelIndexList rows = operations_->selectionModel()->selectedRows();
    panel_->Summon(!rows.isEmpty());

    if (rows.isEmpty())
    {
        return;
    }

    const QModelIndex operation = filter_->mapToSource(rows.front());

    detail_->Show(operation);
    panel_->ShowTitle(model_.data(operation, Qt::DisplayRole).toString());
}

void JournalPage::UpdateSummary()
{
    const int entries = model_.rowCount({});

    emit SummaryChanged(
        entries == 0 ? tr("O diário ainda não registrou nenhuma mudança no disco.")
                     : tr("%n operação(ões) registrada(s), da mais recente para a mais antiga.", nullptr, entries));

    emit AsideChanged(tr("o diário nunca é apagado pelo app"));

    for (int column = 0; column < model_.columnCount({}); ++column)
    {
        operations_->resizeColumnToContents(column);
    }
}
