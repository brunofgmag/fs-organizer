#ifndef FS_ORGANIZER_VIEWMODEL_FAILURE_TEXT_H
#define FS_ORGANIZER_VIEWMODEL_FAILURE_TEXT_H

#include <QtCore/QString>

#include "application/model/LinkOperationResult.h"
#include "domain/model/ImportResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"

[[nodiscard]] QString Explain(LinkFailure failure);

[[nodiscard]] QString Explain(ImportResult result);

[[nodiscard]] QString Describe(const LinkOperationResult& result);

[[nodiscard]] QString NameOfImportStep(OperationKind kind);

#endif // FS_ORGANIZER_VIEWMODEL_FAILURE_TEXT_H
