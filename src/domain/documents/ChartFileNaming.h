#ifndef FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_FILE_NAMING_H
#define FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_FILE_NAMING_H

#include <filesystem>
#include <string>

struct WhatTheFileNameSays
{
    std::string code{};
    std::string type{};
    bool namesADocument = false;
};

[[nodiscard]] WhatTheFileNameSays ReadTheChartFileName(const std::filesystem::path& relativePath);

#endif // FS_ORGANIZER_DOMAIN_DOCUMENTS_CHART_FILE_NAMING_H
