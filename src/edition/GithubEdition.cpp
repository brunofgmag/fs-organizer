#include "edition/Edition.h"

#include <utility>

#include "edition/SelfUpdater.h"
#include "infrastructure/manual/GithubManual.h"

EditionParts BuildTheEdition(const QString& updateFeed, const QString& currentVersion, const AppFolders& folders)
{
    SelfUpdater updater = BuildTheSelfUpdater(updateFeed, currentVersion);

    return EditionParts{
        .updates = std::move(updater.service),
        .manual = std::make_unique<GithubManual>(currentVersion.toStdString(), folders.manualFolder),
        .updateDelivery = updater.delivery,
        .manualDelivery = ManualDelivery::Download,
    };
}
