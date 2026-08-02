#include "view/JournalPage.h"

#include <QtCore/QEvent>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "view/delegates/RowDelegate.h"
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
    auto* rows = new RowDelegate(operations_);
    rows->KeepRowsAtLeast(0);
    operations_->setItemDelegate(rows);
    DressTheHeaderOf(operations_->header());

    search_ = new QLineEdit(this);
    search_->setClearButtonEnabled(true);
    search_->setMinimumWidth(220);
    search_->setMaximumWidth(280);

    failuresOnly_ = new QCheckBox(this);
    reload_ = new QPushButton(this);

    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(search_);
    bar->addWidget(failuresOnly_);
    bar->addStretch();
    bar->addWidget(reload_);

    panel_ = new ContextPanel(tr("Operation"), 400, this);
    panel_->setObjectName(QStringLiteral("JournalOperationPanel"));
    detail_ = new ModelRowDetail(panel_);

    promise_ = new QLabel(panel_);
    promise_->setObjectName(QStringLiteral("PanelPromise"));
    promise_->setWordWrap(true);

    panel_->Add(detail_);
    panel_->Add(promise_);
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
    connect(search_, &QLineEdit::textChanged, filter_, &JournalFilterModel::Search);
    connect(failuresOnly_, &QCheckBox::toggled, filter_, &JournalFilterModel::ShowOnlyWhatFailed);
    connect(reload_, &QPushButton::clicked, &viewModel_, &JournalViewModel::Show);
    connect(&model_, &QAbstractItemModel::modelReset, this, &JournalPage::UpdateSummary);

    RetranslateUi();
    UpdateSummary();
}

void JournalPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        model_.Retranslated();
        UpdateSummary();
        ShowTheSelectedOperation();
    }

    QWidget::changeEvent(event);
}

void JournalPage::RetranslateUi()
{
    search_->setPlaceholderText(tr("Search addon, path or operation…"));
    failuresOnly_->setText(tr("Only what failed"));
    reload_->setText(tr("Read the journal again"));
    promise_->setText(tr("The journal is append-only. Nothing on this screen writes to the disk."));
    panel_->RenameTheFallback(tr("Operation"));
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

    emit SummaryChanged(entries == 0 ? tr("The journal has not recorded any change on the disk yet.")
                                     : tr("%n operation recorded, from the newest to the oldest.", nullptr, entries));

    emit AsideChanged(tr("the journal is never deleted by the app"));

    for (int column = 0; column < model_.columnCount({}); ++column)
    {
        operations_->resizeColumnToContents(column);
    }
}
