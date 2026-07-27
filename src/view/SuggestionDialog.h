#ifndef FS_ORGANIZER_VIEW_SUGGESTION_DIALOG_H
#define FS_ORGANIZER_VIEW_SUGGESTION_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "viewmodel/CategorySuggestionModel.h"

class SuggestionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SuggestionDialog(const std::vector<CategorySuggestion>& suggestions, QWidget* parent = nullptr);

    [[nodiscard]] std::vector<CategorySuggestion> Chosen() const;

    [[nodiscard]] bool HasAnythingToShow() const;

private:
    CategorySuggestionModel model_;
};

#endif // FS_ORGANIZER_VIEW_SUGGESTION_DIALOG_H
