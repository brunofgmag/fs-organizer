#include "view/library/SuggestionDialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include "view/delegates/PlainTextDelegate.h"
#include "view/TableColumns.h"
#include "view/theme/ModernistMetrics.h"

SuggestionDialog::SuggestionDialog(const std::vector<CategorySuggestion>& suggestions, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Category suggestions"));

    model_.Show(suggestions);

    auto* explanation = new QLabel(tr("The rules that get it right on their own come checked. The livery rule comes "
                                      "unchecked because it is often wrong: check it before applying."),
                                   this);
    explanation->setWordWrap(true);

    auto* table = new QTableView(this);
    table->setModel(&model_);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setItemDelegate(new PlainTextDelegate(table));
    table->verticalHeader()->setVisible(false);
    LetTheColumnsBeDraggedAndStillFillTheTable(table);

    auto* all = new QCheckBox(tr("Check all"), this);
    all->setTristate(true);
    all->setCheckState(model_.ChosenState());

    connect(all, &QCheckBox::clicked, this,
            [this]
            {
                model_.ChooseAll(model_.ChosenState() != Qt::Checked);
            });

    connect(&model_, &QAbstractItemModel::dataChanged, this,
            [this, all]
            {
                all->setCheckState(model_.ChosenState());
            });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* apply = buttons->addButton(tr("Move the checked ones"), QDialogButtonBox::AcceptRole);
    apply->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(all);
    layout->addWidget(table, 1);
    layout->addWidget(buttons);

    resize(760, 480);
}

std::vector<CategorySuggestion> SuggestionDialog::Chosen() const
{
    return model_.Chosen();
}

bool SuggestionDialog::HasAnythingToShow() const
{
    return model_.rowCount({}) > 0;
}
