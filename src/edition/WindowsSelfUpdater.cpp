#include "edition/SelfUpdater.h"

#include "infrastructure/update/GithubUpdateService.h"

SelfUpdater BuildTheSelfUpdater(const QString& updateFeed, const QString& currentVersion)
{
    return SelfUpdater{
        .service = std::make_unique<GithubUpdateService>(updateFeed, currentVersion,
                                                         GithubUpdateService::DefaultUpdatesFolder()),
        .delivery = UpdateDelivery::SelfUpdate,
    };
}
