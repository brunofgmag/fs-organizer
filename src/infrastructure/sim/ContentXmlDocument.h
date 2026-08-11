#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_DOCUMENT_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_DOCUMENT_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/PackageList.h"

[[nodiscard]] std::vector<PackageEntry> PackageEntriesIn(std::string_view document);

[[nodiscard]] std::optional<std::string>
WithPackageSwitched(std::string_view document, std::string_view packageName, bool activated);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_CONTENT_XML_DOCUMENT_H
