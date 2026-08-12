#include "domain/documents/DocumentClassification.h"

#include "domain/documents/ChartFileNaming.h"
#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"

namespace
{
    constexpr auto kFolderThatMeansCharts = "charts";
}

ClassifiedDocument ClassifyDocument(const std::filesystem::path& relativePath,
                                    const std::vector<std::string>& knownCodes)
{
    if (ReadTheChartFileName(relativePath).namesADocument)
    {
        return {.kind = DocumentKind::Document, .code = {}};
    }

    const std::string folder = ComparableFileName(relativePath.parent_path());

    for (const std::string& code : knownCodes)
    {
        if (folder == LoweredForComparison(code))
        {
            return {.kind = DocumentKind::Chart, .code = code};
        }
    }

    if (folder == kFolderThatMeansCharts)
    {
        return {.kind = DocumentKind::Chart, .code = {}};
    }

    return {.kind = DocumentKind::Document, .code = {}};
}
