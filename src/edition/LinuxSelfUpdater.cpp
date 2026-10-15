#include "edition/SelfUpdater.h"

#include <QtCore/QCoreApplication>

#include "infrastructure/update/AppImageUpdateService.h"
#include "infrastructure/update/NoticeOnlyUpdateService.h"
#include "infrastructure/update/ReplaceableAppImage.h"

SelfUpdater BuildTheSelfUpdater(const QString& updateFeed, const QString& currentVersion)
{
    const QString appImage = ReplaceableAppImage(qEnvironmentVariable("APPIMAGE"));

    if (appImage.isEmpty())
    {
        return SelfUpdater{
            .service = std::make_unique<NoticeOnlyUpdateService>(updateFeed, currentVersion),
            .delivery = UpdateDelivery::NoticeOnlyOnGithub,
        };
    }

    return SelfUpdater{
        .service = std::make_unique<AppImageUpdateService>(updateFeed, currentVersion, appImage,
                                                           QCoreApplication::applicationPid()),
        .delivery = UpdateDelivery::SelfUpdate,
    };
}
