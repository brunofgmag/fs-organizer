#include "edition/SelfUpdater.h"

#include "infrastructure/update/AppImageUpdateService.h"

SelfUpdater BuildTheSelfUpdater(const QString& updateFeed, const QString& currentVersion)
{
    return SelfUpdater{
        .service = std::make_unique<AppImageUpdateService>(updateFeed, currentVersion),
        .delivery = UpdateDelivery::NoticeOnly,
    };
}
