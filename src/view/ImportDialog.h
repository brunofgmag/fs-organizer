#ifndef FS_ORGANIZER_VIEW_IMPORT_DIALOG_H
#define FS_ORGANIZER_VIEW_IMPORT_DIALOG_H

#include <cstdint>
#include <filesystem>
#include <vector>

#include <QtWidgets/QDialog>

#include "domain/model/ImportRequest.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"

class QComboBox;
class QLabel;

class ImportDialog final : public QDialog
{
    Q_OBJECT

public:
    ImportDialog(std::vector<std::filesystem::path> folders,
                 const std::vector<TreeNode>& libraries,
                 const SimulatorProfile& profile,
                 std::uintmax_t totalBytes,
                 QWidget* parent = nullptr);

    [[nodiscard]] std::vector<ImportRequest> ChosenRequests() const;

private:
    void ShowCategoriesOfTheChosenLibrary();

    std::vector<std::filesystem::path> folders_;
    const std::vector<TreeNode>& libraries_;
    QComboBox* library_ = nullptr;
    QComboBox* category_ = nullptr;
    QLabel* landing_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_IMPORT_DIALOG_H
