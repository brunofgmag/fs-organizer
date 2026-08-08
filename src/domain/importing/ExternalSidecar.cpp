#include "domain/importing/ExternalSidecar.h"

#include <map>

#include "domain/importing/SidecarFile.h"

namespace
{
    constexpr auto kExternalKey = "external";
}

std::filesystem::path ExternalSidecarPathFor(const std::filesystem::path& addonFolder)
{
    return SidecarBeside(addonFolder, kExternalSidecarSuffix);
}

std::string TextOfTheExternalOrigin(const std::filesystem::path& externalPath)
{
    return SidecarHeader() + SidecarLine(kExternalKey, AsUtf8(externalPath));
}

std::optional<std::filesystem::path> ExternalOriginFromText(const std::string& text)
{
    const std::optional<std::map<std::string, std::string>> fields = FieldsOfTheSidecar(text);
    if (!fields.has_value())
    {
        return std::nullopt;
    }

    const std::string external = FieldNamed(*fields, kExternalKey);
    if (external.empty())
    {
        return std::nullopt;
    }

    return PathFromUtf8(external);
}
