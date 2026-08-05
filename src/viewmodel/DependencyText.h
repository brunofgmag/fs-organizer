#ifndef FS_ORGANIZER_VIEWMODEL_DEPENDENCY_TEXT_H
#define FS_ORGANIZER_VIEWMODEL_DEPENDENCY_TEXT_H

#include <QtCore/QString>

#include "application/DependencyReport.h"

[[nodiscard]] QString AnswerFor(const DependencyAnswer& answer);

[[nodiscard]] QString WhereTheListCameFrom(const DependencyReport& report);

#endif // FS_ORGANIZER_VIEWMODEL_DEPENDENCY_TEXT_H
