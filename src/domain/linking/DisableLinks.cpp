#include "domain/linking/DisableLinks.h"

bool DisableEveryLink(const LinkingEngine& linking,
                      const OperationLog& log,
                      const std::vector<std::filesystem::path>& links,
                      const AddonId& addon,
                      const std::filesystem::path& folder)
{
    for (const std::filesystem::path& link : links)
    {
        const LinkOutcome outcome = linking.Disable(link);

        log.RecordLink(OperationKind::DisableAddon, addon, link, folder, outcome.Failure());

        if (!outcome.Succeeded())
        {
            return false;
        }
    }

    return true;
}
