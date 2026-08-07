#ifndef FS_ORGANIZER_VIEWMODEL_FAILURE_TEXT_H
#define FS_ORGANIZER_VIEWMODEL_FAILURE_TEXT_H

#include <QtCore/QString>

#include "application/model/DeletionPlan.h"
#include "application/model/FileOperationResult.h"
#include "application/model/ImportOperationResult.h"
#include "application/model/LinkOperationResult.h"
#include "application/model/RestorePlan.h"
#include "domain/model/CategoryRule.h"
#include "domain/model/FileResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"

[[nodiscard]] QString Explain(LinkFailure failure);

[[nodiscard]] QString Explain(CategoryRule rule);

[[nodiscard]] QString Explain(FileResult result);

[[nodiscard]] QString Describe(const LinkOperationResult& result);

[[nodiscard]] QString Describe(const ImportOperationResult& result);

[[nodiscard]] QString Describe(const FileOperationResult& result);

[[nodiscard]] QString Describe(const DeletionResult& result, DeletionRoute route);

[[nodiscard]] QString Describe(const SwapResult& result);

[[nodiscard]] QString NameOfImportStep(OperationKind kind);

#endif // FS_ORGANIZER_VIEWMODEL_FAILURE_TEXT_H
