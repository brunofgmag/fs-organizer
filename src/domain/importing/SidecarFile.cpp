#include "domain/importing/SidecarFile.h"

#include <sstream>

namespace
{
    constexpr auto kVersionKey = "version";
    constexpr auto kWrittenVersion = "1";

    std::string WithoutTheCarriageReturn(std::string line)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        return line;
    }
}

std::filesystem::path SidecarBeside(const std::filesystem::path& item, const std::string_view suffix)
{
    std::filesystem::path sidecar = item;
    sidecar += suffix;

    return sidecar;
}

std::string SidecarHeader()
{
    return SidecarLine(kVersionKey, kWrittenVersion);
}

std::string SidecarLine(const std::string_view key, const std::string& value)
{
    std::string line;
    line.append(key).append("=").append(value).append("\n");

    return line;
}

std::optional<std::map<std::string, std::string>> FieldsOfTheSidecar(const std::string& text)
{
    std::istringstream lines(text);

    std::map<std::string, std::string> fields;

    for (std::string line; std::getline(lines, line);)
    {
        const std::string entry = WithoutTheCarriageReturn(std::move(line));
        const std::size_t separator = entry.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        fields.insert_or_assign(entry.substr(0, separator), entry.substr(separator + 1));
    }

    if (FieldNamed(fields, kVersionKey) != kWrittenVersion)
    {
        return std::nullopt;
    }

    return fields;
}

std::string FieldNamed(const std::map<std::string, std::string>& fields, const std::string_view key)
{
    const auto found = fields.find(std::string(key));

    return found == fields.end() ? std::string{} : found->second;
}
