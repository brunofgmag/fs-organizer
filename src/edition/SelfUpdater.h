#ifndef FS_ORGANIZER_EDITION_SELF_UPDATER_H
#define FS_ORGANIZER_EDITION_SELF_UPDATER_H

#include <memory>

#include <QtCore/QString>

#include "application/model/UpdateDelivery.h"
#include "application/ports/UpdateService.h"

struct SelfUpdater
{
    std::unique_ptr<UpdateService> service{};
    UpdateDelivery delivery{};
};

[[nodiscard]] SelfUpdater BuildTheSelfUpdater(const QString& updateFeed, const QString& currentVersion);

#endif // FS_ORGANIZER_EDITION_SELF_UPDATER_H
