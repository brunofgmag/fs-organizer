#include "view/panels/ModelRowDetail.h"

#include <QtCore/QEvent>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>

namespace
{
    constexpr int kFieldNameWidth = 92;
}

ModelRowDetail::ModelRowDetail(QWidget* parent) : QWidget(parent)
{
    rows_ = new QGridLayout(this);
    rows_->setContentsMargins(0, 0, 0, 0);
    rows_->setHorizontalSpacing(10);
    rows_->setVerticalSpacing(7);
    rows_->setColumnMinimumWidth(0, kFieldNameWidth);
    rows_->setColumnStretch(1, 1);

    Show({});
}

void ModelRowDetail::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Show(shown_, alongside_);
    }

    QWidget::changeEvent(event);
}

void ModelRowDetail::Show(const QModelIndex& index, const QList<Field>& alongside)
{
    shown_ = index;
    alongside_ = alongside;

    if (!index.isValid())
    {
        Clear();

        auto* placeholder = new QLabel(tr("Nothing selected."), this);
        placeholder->setObjectName(QStringLiteral("DetailPlaceholder"));
        placeholder->setWordWrap(true);
        rows_->addWidget(placeholder, 0, 0, 1, 2);
        return;
    }

    const QAbstractItemModel* model = index.model();
    QList<Field> fields;

    for (int column = 0; column < model->columnCount(index.parent()); ++column)
    {
        const QString value = model->data(index.siblingAtColumn(column), Qt::DisplayRole).toString();

        if (!value.isEmpty())
        {
            fields.append({model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString(), value});
        }
    }

    fields.append(alongside);

    ShowFields(fields);
}

void ModelRowDetail::ShowFields(const QList<Field>& fields)
{
    Clear();

    int row = 0;

    for (const Field& field : fields)
    {
        auto* name = new QLabel(field.first, this);
        name->setObjectName(QStringLiteral("DetailFieldName"));
        name->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        auto* value = new QLabel(field.second, this);
        value->setObjectName(QStringLiteral("DetailFieldValue"));
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);

        rows_->addWidget(name, row, 0);
        rows_->addWidget(value, row, 1);
        ++row;
    }
}

void ModelRowDetail::Clear()
{
    while (const QLayoutItem* item = rows_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
}
