#ifndef FS_ORGANIZER_VIEW_LIBRARY_COVERAGE_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_COVERAGE_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "viewmodel/CoverageViewModel.h"

class CoverageDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CoverageDialog(const std::vector<CoverageLine>& covered, QWidget* parent = nullptr);
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_COVERAGE_DIALOG_H
