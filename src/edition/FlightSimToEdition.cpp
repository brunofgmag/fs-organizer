#include "edition/Edition.h"

#include "infrastructure/update/NoticeOnlyUpdateService.h"

EditionParts BuildTheEdition(const QString& updateFeed, const QString& currentVersion)
{
    return EditionParts{
        .updates = std::make_unique<NoticeOnlyUpdateService>(updateFeed, currentVersion),
        .manual = std::make_unique<NoManualToFetch>(),
        .updateDelivery = UpdateDelivery::NoticeOnly,
        .manualDelivery = ManualDelivery::NotShipped,
    };
}
