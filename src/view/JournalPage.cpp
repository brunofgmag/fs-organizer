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

    summary_ = new QLabel(this);

    auto* search = new QLineEdit(this);
    search->setPlaceholderText(tr("Buscar addon, caminho ou operação..."));
    search->setClearButtonEnabled(true);
    search->setMaximumWidth(280);

    auto* failuresOnly = new QCheckBox(tr("Só o que falhou"), this);
    auto* reload = new QPushButton(tr("Reler o diário"), this);

    auto* bar = new QHBoxLayout;
    bar->addWidget(summary_, 1);
    bar->addWidget(search);
    bar->addWidget(failuresOnly);
    bar->addWidget(reload);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bar);
    layout->addWidget(operations_, 1);

    connect(search, &QLineEdit::textChanged, filter_, &JournalFilterModel::Search);
    connect(failuresOnly, &QCheckBox::toggled, filter_, &JournalFilterModel::ShowOnlyWhatFailed);
    connect(reload, &QPushButton::clicked, &viewModel_, &JournalViewModel::Show);
    connect(&model_, &QAbstractItemModel::modelReset, this, &JournalPage::UpdateSummary);

    UpdateSummary();
}

void JournalPage::UpdateSummary()
{
    const int entries = model_.rowCount({});

    summary_->setText(entries == 0
                          ? tr("O diário ainda não registrou nenhuma mudança no disco.")
                          : tr("%n operação(ões) registrada(s), da mais recente para a mais antiga.",
                               nullptr, entries));

    for (int column = 0; column < model_.columnCount({}); ++column)
    {
        operations_->resizeColumnToContents(column);
    }

    emit StatusChanged(summary_->text());
}
