#ifndef FS_ORGANIZER_VIEWMODEL_CATEGORY_SUGGESTION_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_CATEGORY_SUGGESTION_MODEL_H

#include <vector>

#include <QtCore/QAbstractTableModel>

#include "domain/tree/CategorySuggester.h"

class CategorySuggestionModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    void Retranslated()
    {
        emit layoutAboutToBeChanged();
        emit layoutChanged();
    }

    enum Column
    {
        AddonColumn = 0,
        CurrentCategoryColumn,
        SuggestedCategoryColumn,
        RuleColumn,
        ColumnCount,
    };

    explicit CategorySuggestionModel(QObject* parent = nullptr);

    void Show(const std::vector<CategorySuggestion>& suggestions);

    [[nodiscard]] std::vector<CategorySuggestion> Chosen() const;

    void ChooseAll(bool chosen);

    [[nodiscard]] Qt::CheckState ChosenState() const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    bool setData(const QModelIndex& position, const QVariant& value, int role) override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& position) const override;

private:
    struct Row
    {
        CategorySuggestion suggestion;
        bool chosen = false;
    };

    std::vector<Row> rows_;
};

#endif // FS_ORGANIZER_VIEWMODEL_CATEGORY_SUGGESTION_MODEL_H
