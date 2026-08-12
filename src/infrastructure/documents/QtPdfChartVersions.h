#ifndef FS_ORGANIZER_INFRASTRUCTURE_DOCUMENTS_QT_PDF_CHART_VERSIONS_H
#define FS_ORGANIZER_INFRASTRUCTURE_DOCUMENTS_QT_PDF_CHART_VERSIONS_H

#include "domain/ports/ChartVersions.h"

class QtPdfChartVersions final : public ChartVersions
{
public:
    [[nodiscard]] long long VersionOf(const std::filesystem::path& chart) const override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_DOCUMENTS_QT_PDF_CHART_VERSIONS_H
