#include "view/CommunityPage.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include "view/FailureText.h"
#include "view/RepairDialog.h"

CommunityPage::CommunityPage(CommunityViewModel& viewModel, CommunityModel& model, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), model_(model)
{
    filter_ = new CommunityFilterModel(this);
    filter_->setSourceModel(&model_);

    table_ = new QTableView(this);
    table_->setModel(filter_);
    table_->setSortingEnabled(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);

    summary_ = new QLabel(this);

    classes_ = new QComboBox(this);
    classes_->addItem(tr("Todas as classes"), QVariant());
    classes_->addItem(tr("Gerenciada"), static_cast<int>(EntryClassification::Managed));
    classes_->addItem(tr("Externa"), static_cast<int>(EntryClassification::External));
    classes_->addItem(tr("Quebrada"), static_cast<int>(EntryClassification::Broken));
    classes_->addItem(tr("Indisponível"), static_cast<int>(EntryClassification::Unavailable));
    classes_->addItem(tr("Não gerenciada"), static_cast<int>(EntryClassification::Unmanaged));
    classes_->addItem(tr("Duplicada"), static_cast<int>(EntryClassification::Duplicated));

    repair_ = new QPushButton(tr("Reparar quebrados..."), this);
    repair_->setEnabled(false);

    auto* bar = new QHBoxLayout;
    bar->addWidget(summary_, 1);
    bar->addWidget(classes_);
    bar->addWidget(repair_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bar);
    layout->addWidget(table_, 1);

    connect(classes_, &QComboBox::activated, this, &CommunityPage::OnFilterChanged);
    connect(repair_, &QPushButton::clicked, this, &CommunityPage::StartRepair);
    connect(&model_, &QAbstractItemModel::modelReset, this, &CommunityPage::UpdateSummary);
    connect(&viewModel_, &CommunityViewModel::RepairFinished, this,
            &CommunityPage::OnRepairFinished);

    UpdateSummary();
}

void CommunityPage::OnFilterChanged(const int index) const
{
    const QVariant chosen = classes_->itemData(index);

    filter_->ShowOnly(chosen.isValid()
                          ? std::optional(static_cast<EntryClassification>(chosen.toInt()))
                          : std::nullopt);
}

void CommunityPage::StartRepair()
{
    const std::vector<RepairCandidate> candidates = viewModel_.PlanRepairs();

    if (candidates.empty())
    {
        emit StatusChanged(tr("Nenhum link quebrado para reparar."));
        return;
    }

    RepairDialog dialog(candidates, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    viewModel_.Repair(dialog.ChosenRequests());
}

void CommunityPage::OnRepairFinished(const std::vector<LinkOperationResult>& results)
{
    QStringList failed;
    for (const LinkOperationResult& result : results)
    {
        if (!result.outcome.Succeeded())
        {
            failed.append(Describe(result));
        }
    }

    const auto done = static_cast<int>(results.size()) - static_cast<int>(failed.size());

    if (failed.isEmpty())
    {
        emit StatusChanged(tr("%n reparo(s) concluído(s).", nullptr, done));
        return;
    }

    QMessageBox report(QMessageBox::Warning, tr("Nem tudo foi reparado"),
                       tr("%n reparo(s) falhou(aram).", nullptr,
                          static_cast<int>(failed.size())),
                       QMessageBox::Ok, this);
    report.setInformativeText(tr("%n reparo(s) concluído(s).", nullptr, done));
    report.setDetailedText(failed.join('\n'));
    report.exec();

    emit StatusChanged(tr("%1 · %2")
        .arg(tr("%n reparo(s) concluído(s)", nullptr, done),
             tr("%n falhou(aram)", nullptr, static_cast<int>(failed.size()))));
}

void CommunityPage::UpdateSummary() const
{
    const int rows = model_.rowCount({});
    int broken = 0;
    int managed = 0;

    for (int row = 0; row < rows; ++row)
    {
        const auto classification = static_cast<EntryClassification>(
            model_.data(model_.index(row, 0), CommunityModel::ClassificationRole).toInt());

        broken += classification == EntryClassification::Broken ? 1 : 0;
        managed += classification == EntryClassification::Managed ? 1 : 0;
    }

    summary_->setText(tr("%1 · %2 · %3")
        .arg(tr("%n entrada(s)", nullptr, rows),
             tr("%n gerenciada(s)", nullptr, managed),
             tr("%n quebrada(s)", nullptr, broken)));

    repair_->setEnabled(broken > 0);
}
