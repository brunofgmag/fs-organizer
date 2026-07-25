#ifndef FS_ORGANIZER_VIEW_FAILURE_TEXT_H
#define FS_ORGANIZER_VIEW_FAILURE_TEXT_H

#include <QtCore/QString>

#include "application/model/LinkOperationResult.h"
#include "domain/model/LinkFailure.h"

[[nodiscard]] QString Explain(LinkFailure failure);

[[nodiscard]] QString Describe(const LinkOperationResult& result);

#endif // FS_ORGANIZER_VIEW_FAILURE_TEXT_H
