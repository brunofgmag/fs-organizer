#ifndef FS_ORGANIZER_VIEW_LIBRARY_DELETE_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_DELETE_DIALOG_H

#include <QtWidgets/QDialog>

#include "application/model/DeletionPlan.h"
#include "viewmodel/DeletionViewModel.h"

class QLabel;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

class DeleteDialog final : public QDialog
{
    Q_OBJECT

public:
    DeleteDialog(DeletionPlan plan, DeletionViewModel& viewModel, QWidget* parent = nullptr);

private:
    [[nodiscard]] DeletionRoute ChosenRoute() const;

    [[nodiscard]] QString WhatWasSelected() const;

    [[nodiscard]] QString WhatTheRecycleBinWillNotTake() const;

    [[nodiscard]] QString WhereTheLinksAre() const;

    [[nodiscard]] QRadioButton* AddRoute(QVBoxLayout& column, const QString& title, const QString& detail);

    void ShowTheChosenRoute() const;

    void DeleteThem();

    DeletionPlan plan_;
    DeletionViewModel& viewModel_;
    QRadioButton* recycle_ = nullptr;
    QRadioButton* forGood_ = nullptr;
    QPushButton* confirm_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_DELETE_DIALOG_H
