#ifndef FS_ORGANIZER_DOMAIN_LINKING_DISABLE_LINKS_H
#define FS_ORGANIZER_DOMAIN_LINKING_DISABLE_LINKS_H

#include <filesystem>
#include <vector>

#include "domain/journal/OperationLog.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/model/AddonId.h"

[[nodiscard]] bool DisableEveryLink(const LinkingEngine& linking,
                                    const OperationLog& log,
                                    const std::vector<std::filesystem::path>& links,
                                    const AddonId& addon,
                                    const std::filesystem::path& folder);

#endif // FS_ORGANIZER_DOMAIN_LINKING_DISABLE_LINKS_H
