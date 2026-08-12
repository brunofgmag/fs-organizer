#ifndef FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENTS_WINDOW_H
#define FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENTS_WINDOW_H

#include <filesystem>

#include <QtWidgets/QDialog>

#include "viewmodel/DocumentsViewModel.h"

class DocumentReader;
class QTreeWidget;
class QTreeWidgetItem;

class DocumentsWindow final : public QDialog
{
    Q_OBJECT

public:
    explicit DocumentsWindow(DocumentsViewModel& viewModel, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    void Rebuild();

    void Retranslate();

    void OpenWhatWasChosen(const QTreeWidgetItem* line);

    void TurnTheStarOf(QTreeWidgetItem* line, int column);

    [[nodiscard]] static std::filesystem::path DocumentOf(const QTreeWidgetItem* line);

    DocumentsViewModel& viewModel_;
    QTreeWidget* index_ = nullptr;
    DocumentReader* reader_ = nullptr;
    std::filesystem::path reading_{};
};

#endif // FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENTS_WINDOW_H
