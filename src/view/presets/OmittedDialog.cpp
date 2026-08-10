#include "view/presets/OmittedDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include "view/TableColumns.h"
#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"

OmittedDialog::OmittedDialog(const QList<OmittedAddon>& omitted, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Off because Replace omits them"));

    auto* explanation = new QLabel(tr("%n addon of yours is enabled now and this preset does not name it, so Replace "
                                      "turns it off. It is part of what the plan already counts as turned off, and "
                                      "not a pile on top of it.",
                                      nullptr, static_cast<int>(omitted.size())),
                                   this);
    explanation->setWordWrap(true);

    auto* table = new QTableWidget(this);
    table->setObjectName(QStringLiteral("OmittedAddons"));
    table->setColumnCount(2);
    table->setRowCount(static_cast<int>(omitted.size()));
    table->setHorizontalHeaderLabels({tr("Addon"), tr("Category")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    table->setItemDelegate(new RowDelegate(table));
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    DressTheHeaderOf(table->horizontalHeader());
    LetTheColumnsBeDraggedAndStillFillTheTable(table, 0);

    for (int row = 0; row < omitted.size(); ++row)
    {
        table->setItem(row, 0, new QTableWidgetItem(omitted[row].name));
        table->setItem(row, 1, new QTableWidgetItem(omitted[row].category));
        table->item(row, 1)->setData(QuietRole, true);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(table, 1);
    layout->addWidget(buttons);

    SizeToTheContent(*this, 560);
}
