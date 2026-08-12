#include "infrastructure/sim/LoadingReportText.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QtCore/QDateTime>
#include <QtCore/QString>

#include "infrastructure/sim/PackageNaming.h"

namespace
{
    constexpr std::string_view kEngine = "[Engine]";
    constexpr std::string_view kModules = "[Wasm_Modules]";
    constexpr std::string_view kPackages = "[FlightSimulator_Packages]";
    constexpr std::string_view kInstantKey = "TimeUTC=";
    constexpr std::string_view kFormatKey = "Format=";
    constexpr std::string_view kModuleColumn = "DebugName";
    constexpr std::string_view kPackageColumn = "PackageName";
    constexpr std::string_view kMemoryColumn = "MemorySize";

    [[nodiscard]] std::string_view Trimmed(std::string_view text)
    {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
        {
            text.remove_prefix(1);
        }

        while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        {
            text.remove_suffix(1);
        }

        return text;
    }

    [[nodiscard]] std::string_view Unquoted(std::string_view value)
    {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
            return value.substr(1, value.size() - 2);
        }

        return value;
    }

    [[nodiscard]] std::vector<std::string_view> FieldsOf(std::string_view list)
    {
        std::vector<std::string_view> fields;
        bool quoted = false;
        std::size_t from = 0;

        for (std::size_t at = 0; at < list.size(); ++at)
        {
            if (list[at] == '"')
            {
                quoted = !quoted;
                continue;
            }

            if (list[at] == ',' && !quoted)
            {
                fields.push_back(Unquoted(Trimmed(list.substr(from, at - from))));
                from = at + 1;
            }
        }

        fields.push_back(Unquoted(Trimmed(list.substr(from))));

        return fields;
    }

    [[nodiscard]] std::string_view BetweenBrackets(std::string_view line)
    {
        const std::size_t opens = line.find('[');
        const std::size_t closes = line.rfind(']');

        if (opens == std::string_view::npos || closes == std::string_view::npos || closes <= opens)
        {
            return {};
        }

        return line.substr(opens + 1, closes - opens - 1);
    }

    [[nodiscard]] std::size_t ColumnCalled(const std::vector<std::string_view>& columns, const std::string_view name)
    {
        const auto found = std::ranges::find(columns, name);

        return found == columns.end() ? std::string_view::npos
                                      : static_cast<std::size_t>(std::distance(columns.begin(), found));
    }

    [[nodiscard]] std::string_view FieldAt(const std::vector<std::string_view>& fields, const std::size_t column)
    {
        return column < fields.size() ? fields[column] : std::string_view{};
    }

    [[nodiscard]] std::optional<std::uintmax_t> AsBytes(const std::string_view value)
    {
        std::uintmax_t bytes = 0;
        const char* const from = value.data();
        const char* const to = from + value.size();
        const std::from_chars_result read = std::from_chars(from, to, bytes);

        if (read.ec != std::errc{} || read.ptr != to || value.empty())
        {
            return std::nullopt;
        }

        return bytes;
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> AsInstant(const std::string_view value)
    {
        const QDateTime moment =
            QDateTime::fromString(QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())), Qt::ISODate);

        if (!moment.isValid())
        {
            return std::nullopt;
        }

        return std::chrono::system_clock::time_point(std::chrono::milliseconds(moment.toMSecsSinceEpoch()));
    }

    [[nodiscard]] std::vector<std::string_view> LinesOf(const std::string_view text)
    {
        std::vector<std::string_view> lines;
        std::size_t from = 0;

        while (from <= text.size())
        {
            const std::size_t breaks = text.find('\n', from);
            lines.push_back(Trimmed(text.substr(from, breaks - from)));
            from = breaks == std::string_view::npos ? text.size() + 1 : breaks + 1;
        }

        return lines;
    }

    [[nodiscard]] LoadedModule ModuleFrom(const std::vector<std::string_view>& fields,
                                          const std::vector<std::string_view>& columns)
    {
        const std::string packageName = std::string(FieldAt(fields, ColumnCalled(columns, kPackageColumn)));

        return LoadedModule{.moduleName = std::string(FieldAt(fields, ColumnCalled(columns, kModuleColumn))),
                            .packageName = packageName,
                            .packageFolderName = WithoutTheGenerationPrefix(packageName),
                            .memoryBytes = AsBytes(FieldAt(fields, ColumnCalled(columns, kMemoryColumn)))};
    }
}

LoadingReport LoadingReportFrom(const std::string_view text)
{
    LoadingReport report;
    std::vector<std::string_view> columns;
    std::string_view section;

    for (const std::string_view line : LinesOf(text))
    {
        if (line.empty())
        {
            continue;
        }

        if (line.front() == '[')
        {
            section = line;
            continue;
        }

        if (section == kEngine && line.starts_with(kInstantKey))
        {
            report.runAt = AsInstant(Unquoted(line.substr(kInstantKey.size())));
            continue;
        }

        if (section == kPackages && line.find('=') != std::string_view::npos)
        {
            ++report.packagesRegistered;
            continue;
        }

        if (section != kModules)
        {
            continue;
        }

        if (line.starts_with(kFormatKey))
        {
            columns = FieldsOf(Unquoted(line.substr(kFormatKey.size())));
            continue;
        }

        if (!columns.empty() && line.find('=') != std::string_view::npos)
        {
            report.modules.push_back(ModuleFrom(FieldsOf(BetweenBrackets(line)), columns));
        }
    }

    return report;
}
