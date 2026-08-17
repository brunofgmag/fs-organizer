#ifndef FS_ORGANIZER_VIEW_LIBRARY_SHARED_AIRPORTS_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_SHARED_AIRPORTS_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "viewmodel/CoverageViewModel.h"

class SharedAirportsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SharedAirportsDialog(const std::vector<SharedAirportsLine>& shared, QWidget* parent = nullptr);
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_SHARED_AIRPORTS_DIALOG_H
