#ifndef FS_ORGANIZER_VIEWMODEL_SIZE_SUMMARY_H
#define FS_ORGANIZER_VIEWMODEL_SIZE_SUMMARY_H

#include <QtCore/QString>

#include "support/SizeText.h"
#include "viewmodel/SelectionSize.h"

[[nodiscard]] QString SizeOfTheSelection(const SelectionSize& size);

#endif // FS_ORGANIZER_VIEWMODEL_SIZE_SUMMARY_H
