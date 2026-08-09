#ifndef FS_ORGANIZER_VIEW_LIBRARY_LIBRARY_ROOT_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_LIBRARY_ROOT_DIALOG_H

#include <filesystem>

#include <QtWidgets/QDialog>

#include "domain/model/RecycleLimits.h"

class QGridLayout;

class LibraryRootDialog final : public QDialog
{
    Q_OBJECT

public:
    LibraryRootDialog(const std::filesystem::path& root, const RootDepth& depth, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    static void AddTheSide(QGridLayout& grid, int column, const QString& title, const QString& said);
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_LIBRARY_ROOT_DIALOG_H
