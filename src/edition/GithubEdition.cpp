#include "edition/Edition.h"

#include "infrastructure/manual/GithubManual.h"
#include "infrastructure/platform/WindowsKnownFolders.h"
#include "infrastructure/update/GithubUpdateService.h"

EditionParts BuildTheEdition(const QString& updateFeed, const QString& currentVersion)
{
    return EditionParts{
        .updates = std::make_unique<GithubUpdateService>(updateFeed, currentVersion,
                                                         GithubUpdateService::DefaultUpdatesFolder()),
        .manual = std::make_unique<GithubManual>(currentVersion.toStdString(), ManualFolderPath()),
        .updateDelivery = UpdateDelivery::SelfUpdate,
        .manualDelivery = ManualDelivery::Download,
    };
}
