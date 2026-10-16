#include "edition/Edition.h"

#include "infrastructure/update/NoticeOnlyUpdateService.h"

EditionParts BuildTheEdition(const QString& updateFeed, const QString& currentVersion, const AppFolders& folders)
{
    static_cast<void>(folders);

    return EditionParts{
        .updates = std::make_unique<NoticeOnlyUpdateService>(updateFeed, currentVersion),
        .manual = std::make_unique<NoManualToFetch>(),
        .updateDelivery = UpdateDelivery::NoticeOnly,
        .manualDelivery = ManualDelivery::NotShipped,
    };
}
