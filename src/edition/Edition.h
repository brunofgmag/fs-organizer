#ifndef FS_ORGANIZER_EDITION_EDITION_H
#define FS_ORGANIZER_EDITION_EDITION_H

#include <memory>

#include <QtCore/QString>

#include "application/model/ManualDelivery.h"
#include "application/model/UpdateDelivery.h"
#include "application/ports/ManualSource.h"
#include "application/ports/UpdateService.h"

struct EditionParts
{
    std::unique_ptr<UpdateService> updates{};
    std::unique_ptr<ManualSource> manual{};
    UpdateDelivery updateDelivery{};
    ManualDelivery manualDelivery{};
};

[[nodiscard]] EditionParts BuildTheEdition(const QString& updateFeed, const QString& currentVersion);

#endif // FS_ORGANIZER_EDITION_EDITION_H
