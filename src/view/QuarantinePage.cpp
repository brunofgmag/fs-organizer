#include "view/QuarantinePage.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/TableColumns.h"
#include "viewmodel/FailureText.h"

QuarantinePage::QuarantinePage(QuarantineViewModel& viewModel, QuarantineModel& model, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), model_(model)
{
    table_ = new QTableView(this);
    table_->setModel(&model_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    LetTheColumnsBeDraggedAndStillFillTheTable(table_);
    table_->verticalHeader()->setVisible(false);

    summary_ = new QLabel(this);
    summary_->setWordWrap(true);

    restore_ = new QPushButton(tr("Restaurar selecionados"), this);
    discard_ = new QPushButton(tr("Descartar selecionados"), this);
    empty_ = new QPushButton(tr("Esvaziar a quarentena"), this);

    auto* bar = new QHBoxLayout;
    bar->addWidget(summary_, 1);
    bar->addWidget(restore_);
    bar->addWidget(discard_);
    bar->addWidget(empty_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bar);
    layout->addWidget(table_, 1);

    connect(restore_, &QPushButton::clicked, this, &QuarantinePage::RestoreSelected);
    connect(discard_, &QPushButton::clicked, this, &QuarantinePage::DiscardSelected);
    connect(empty_, &QPushButton::clicked, this, &QuarantinePage::EmptyTheQuarantine);
    connect(&model_, &QAbstractItemModel::modelReset, this, &QuarantinePage::UpdateSummary);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]
            {
                UpdateSummary();
            });

    connect(&viewModel_, &QuarantineViewModel::Restored, this,
            [this](const std::vector<FileOperationResult>& results)
            {
                Report(tr("Restauração"), results);
            });
    connect(&viewModel_, &QuarantineViewModel::Discarded, this,
            [this](const std::vector<FileOperationResult>& results)
            {
                Report(tr("Descarte"), results);
            });

    UpdateSummary();
}

std::vector<QuarantinedItem> QuarantinePage::Selected() const
{
    std::vector<QuarantinedItem> items;

    for (const QModelIndex& position : table_->selectionModel()->selectedRows())
    {
        if (const QuarantinedItem* item = model_.ItemAt(position))
        {
            items.push_back(*item);
        }
    }

    return items;
}

void QuarantinePage::RestoreSelected()
{
    const std::vector<QuarantinedItem> items = Selected();
    if (items.empty())
    {
        emit StatusChanged(tr("Selecione ao menos um item da quarentena."));
        return;
    }

    viewModel_.Restore(items);
}

void QuarantinePage::DiscardSelected()
{
    const std::vector<QuarantinedItem> items = Selected();
    if (items.empty())
    {
        emit StatusChanged(tr("Selecione ao menos um item da quarentena."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Descartar da quarentena"),
        tr("%n item(ns) será(ão) apagado(s) do disco para sempre. Continuar?", nullptr, static_cast<int>(items.size())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Discard(items);
    }
}

void QuarantinePage::EmptyTheQuarantine()
{
    const std::vector<QuarantinedItem> items = model_.Items();
    if (items.empty())
    {
        emit StatusChanged(tr("A quarentena já está vazia."));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Esvaziar a quarentena"),
        tr("Tudo que está na quarentena, %n item(ns), será apagado do disco para sempre. Continuar?", nullptr,
           static_cast<int>(items.size())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes)
    {
        viewModel_.Discard(items);
    }
}

void QuarantinePage::Report(const QString& title, const std::vector<FileOperationResult>& results)
{
    QStringList failed;
    for (const FileOperationResult& result : results)
    {
        if (result.result != FileResult::Completed)
        {
            failed.append(Describe(result));
        }
    }

    const auto done = static_cast<int>(results.size()) - static_cast<int>(failed.size());

    if (failed.isEmpty())
    {
        emit StatusChanged(tr("%n item(ns) da quarentena.", nullptr, done));
        return;
    }

    QMessageBox report(QMessageBox::Warning, title,
                       tr("%n item(ns) não pôde(puderam) ser tratado(s).", nullptr, static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    report.setInformativeText(tr("%n item(ns) concluído(s).", nullptr, done));
    report.setDetailedText(failed.join('\n'));
    report.exec();
}

void QuarantinePage::UpdateSummary()
{
    const int rows = model_.rowCount({});
    const auto selected = static_cast<int>(Selected().size());

    summary_->setText(rows == 0 ? tr("A quarentena está vazia. Nada que você escolheu descartar foi apagado.")
                                : tr("%n item(ns) na quarentena.", nullptr, rows));

    restore_->setEnabled(selected > 0);
    discard_->setEnabled(selected > 0);
    empty_->setEnabled(rows > 0);
}
