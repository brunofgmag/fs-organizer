#ifndef FS_ORGANIZER_VIEW_LEGACY_LEGACY_IMPORT_DIALOG_H
#define FS_ORGANIZER_VIEW_LEGACY_LEGACY_IMPORT_DIALOG_H

#include <QtWidgets/QDialog>

#include "application/model/LegacyImport.h"
#include "viewmodel/LegacyImportViewModel.h"

class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class LegacyImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit LegacyImportDialog(LegacyImportViewModel& viewModel, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

private:
    void Fill();

    void Import();

    [[nodiscard]] LegacyImportRequest WhatWasChecked() const;

    [[nodiscard]] std::vector<std::filesystem::path> PresetFoldersChecked() const;

    void RefreshTheImportButton() const;

    LegacyImportViewModel& viewModel_;
    QTreeWidget* tree_ = nullptr;
    QPushButton* import_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_LEGACY_LEGACY_IMPORT_DIALOG_H
