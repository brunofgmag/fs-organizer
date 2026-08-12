#include "infrastructure/documents/QtPdfChartVersions.h"

#include <QtPdf/QPdfDocument>

#include "support/PathText.h"

namespace
{
    constexpr auto kVersionPrefix = "CV-";

    [[nodiscard]] long long TheVersionIn(const QString& title)
    {
        const qsizetype prefix = title.indexOf(QLatin1String(kVersionPrefix));

        if (prefix < 0)
        {
            return 0;
        }

        qsizetype digits = prefix + static_cast<qsizetype>(qstrlen(kVersionPrefix));
        const qsizetype firstDigit = digits;

        while (digits < title.size() && title.at(digits).isDigit())
        {
            ++digits;
        }

        return title.mid(firstDigit, digits - firstDigit).toLongLong();
    }
}

long long QtPdfChartVersions::VersionOf(const std::filesystem::path& chart) const
{
    QPdfDocument document;
    document.load(AsText(chart));

    return TheVersionIn(document.metaData(QPdfDocument::MetaDataField::Title).toString());
}
