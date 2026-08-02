#include "viewmodel/CategorySuggestionModel.h"

#include <algorithm>

#include "support/PathText.h"
#include "viewmodel/FailureText.h"

CategorySuggestionModel::CategorySuggestionModel(QObject* parent) : QAbstractTableModel(parent)
{
}

void CategorySuggestionModel::Show(const std::vector<CategorySuggestion>& suggestions)
{
    beginResetModel();

    rows_.clear();
    for (const CategorySuggestion& suggestion : suggestions)
    {
        if (suggestion.WouldMove())
        {
            rows_.push_back(Row{suggestion, TrustedOnItsOwn(suggestion.rule)});
        }
    }

    endResetModel();
}

std::vector<CategorySuggestion> CategorySuggestionModel::Chosen() const
{
    std::vector<CategorySuggestion> chosen;

    for (const Row& row : rows_)
    {
        if (row.chosen)
        {
            chosen.push_back(row.suggestion);
        }
    }

    return chosen;
}

void CategorySuggestionModel::ChooseAll(const bool chosen)
{
    if (rows_.empty())
    {
        return;
    }

    for (Row& row : rows_)
    {
        row.chosen = chosen;
    }

    emit dataChanged(index(0, AddonColumn), index(static_cast<int>(rows_.size()) - 1, AddonColumn),
                     {Qt::CheckStateRole});
}

Qt::CheckState CategorySuggestionModel::ChosenState() const
{
    const auto chosen = static_cast<std::size_t>(std::ranges::count_if(rows_,
                                                                       [](const Row& row)
                                                                       {
                                                                           return row.chosen;
                                                                       }));

    if (chosen == 0)
    {
        return Qt::Unchecked;
    }

    return chosen == rows_.size() ? Qt::Checked : Qt::PartiallyChecked;
}

int CategorySuggestionModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int CategorySuggestionModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant CategorySuggestionModel::data(const QModelIndex& position, const int role) const
{
    if (!position.isValid() || position.row() >= static_cast<int>(rows_.size()))
    {
        return {};
    }

    const Row& row = rows_[static_cast<std::size_t>(position.row())];

    if (role == Qt::CheckStateRole)
    {
        return position.column() == AddonColumn ? QVariant(row.chosen ? Qt::Checked : Qt::Unchecked) : QVariant();
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (position.column())
    {
    case AddonColumn: return AsText(row.suggestion.addonFolder.filename());
    case CurrentCategoryColumn: return AsText(row.suggestion.currentCategory.filename());
    case SuggestedCategoryColumn: return AsText(row.suggestion.suggestedCategory.filename());
    case RuleColumn: return Explain(row.suggestion.rule);
    default: break;
    }

    return {};
}

bool CategorySuggestionModel::setData(const QModelIndex& position, const QVariant& value, const int role)
{
    if (role != Qt::CheckStateRole || !position.isValid() || position.column() != AddonColumn)
    {
        return false;
    }

    rows_[static_cast<std::size_t>(position.row())].chosen = value.value<Qt::CheckState>() == Qt::Checked;

    emit dataChanged(position, position, {Qt::CheckStateRole});

    return true;
}

QVariant CategorySuggestionModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case AddonColumn: return tr("Addon");
    case CurrentCategoryColumn: return tr("Current category");
    case SuggestedCategoryColumn: return tr("Suggestion");
    case RuleColumn: return tr("Rule");
    default: break;
    }

    return {};
}

Qt::ItemFlags CategorySuggestionModel::flags(const QModelIndex& position) const
{
    if (!position.isValid())
    {
        return Qt::NoItemFlags;
    }

    constexpr Qt::ItemFlags common = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return position.column() == AddonColumn ? common | Qt::ItemIsUserCheckable : common;
}
