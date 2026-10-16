#include "infrastructure/sim/PrefixPathMapper.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "domain/support/PathUtils.h"
#include "infrastructure/sim/ChildNamedIgnoringCase.h"

namespace
{
    constexpr auto kDosDevices = "dosdevices";
    constexpr char kFallbackDrive = 'Z';

    std::optional<char> DriveLetterOf(const std::string& text)
    {
        const bool hasADrive = text.size() >= 2 && text[1] == ':' && std::isalpha(static_cast<unsigned char>(text[0]));

        return hasADrive ? std::optional<char>(text[0]) : std::nullopt;
    }

    std::vector<std::string> PartsOf(const std::filesystem::path& path)
    {
        std::vector<std::string> parts;

        for (const std::filesystem::path& part : path.lexically_normal())
        {
            if (!part.empty())
            {
                parts.push_back(AsUtf8(part));
            }
        }

        return parts;
    }

    std::vector<std::string> NamesIn(const std::string& text)
    {
        std::vector<std::string> names;
        std::size_t from = 0;

        while (from <= text.size())
        {
            const std::size_t end = std::min(text.find('/', from), text.size());

            if (end > from)
            {
                names.push_back(text.substr(from, end - from));
            }

            from = end + 1;
        }

        return names;
    }

    bool StartsWith(const std::vector<std::string>& parts, const std::vector<std::string>& prefix)
    {
        return prefix.size() <= parts.size() && std::equal(prefix.begin(), prefix.end(), parts.begin());
    }

    std::filesystem::path Canonical(const std::filesystem::path& path)
    {
        std::error_code error;
        std::filesystem::path resolved = std::filesystem::weakly_canonical(path, error);

        return error ? path : resolved;
    }
}

PrefixPathMapper::PrefixPathMapper(std::filesystem::path prefix) : prefix_(std::move(prefix))
{
}

std::optional<std::filesystem::path> PrefixPathMapper::TargetOfDrive(const char letter) const
{
    const std::filesystem::path devices = prefix_ / kDosDevices;
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    std::error_code error;

    const std::filesystem::path link = devices / (std::string(1, lower) + ":");
    const std::filesystem::path target = std::filesystem::read_symlink(link, error);

    if (error)
    {
        return std::filesystem::is_directory(link, error) ? std::optional<std::filesystem::path>(link) : std::nullopt;
    }

    return (target.is_absolute() ? target : devices / target).lexically_normal();
}

std::filesystem::path PrefixPathMapper::ToHost(const std::filesystem::path& simulatorPath) const
{
    const std::string text = WithGenericSeparators(AsUtf8(simulatorPath));
    const std::optional<char> letter = DriveLetterOf(text);
    const std::optional<std::filesystem::path> target = letter.has_value() ? TargetOfDrive(*letter) : std::nullopt;

    if (!target.has_value())
    {
        return simulatorPath;
    }

    std::filesystem::path resolved = *target;

    for (const std::string& name : NamesIn(text.substr(2)))
    {
        if (name == ".")
        {
            continue;
        }

        resolved = name == ".." ? resolved.parent_path()
                                : ChildNamedIgnoringCase(resolved, name).value_or(resolved / PathFromUtf8(name));
    }

    return resolved;
}

std::optional<PrefixPathMapper::Mounted> PrefixPathMapper::BestDriveFor(const std::vector<std::string>& hostParts) const
{
    std::optional<Mounted> best;

    for (char letter = 'A'; letter <= 'Z'; ++letter)
    {
        const std::optional<std::filesystem::path> target = TargetOfDrive(letter);

        if (!target.has_value())
        {
            continue;
        }

        for (const std::filesystem::path& spelling : {*target, Canonical(*target)})
        {
            std::vector<std::string> parts = PartsOf(spelling);

            if (StartsWith(hostParts, parts) && (!best.has_value() || parts.size() > best->parts.size()))
            {
                best = Mounted{.letter = letter, .parts = std::move(parts)};
            }
        }
    }

    return best;
}

std::filesystem::path PrefixPathMapper::ToSimulator(const std::filesystem::path& hostPath) const
{
    if (!hostPath.is_absolute())
    {
        return hostPath;
    }

    std::vector<std::string> hostParts = PartsOf(hostPath);
    std::optional<Mounted> drive = BestDriveFor(hostParts);

    if (!drive.has_value() || drive->parts.size() <= 1)
    {
        std::vector<std::string> resolvedParts = PartsOf(Canonical(hostPath));

        if (const std::optional<Mounted> throughLinks = BestDriveFor(resolvedParts);
            throughLinks.has_value() && throughLinks->parts.size() > 1)
        {
            drive = throughLinks;
            hostParts = std::move(resolvedParts);
        }
    }

    const Mounted mounted = drive.value_or(Mounted{.letter = kFallbackDrive, .parts = {"/"}});
    std::string text = std::string(1, mounted.letter) + ":";

    for (std::size_t at = mounted.parts.size(); at < hostParts.size(); ++at)
    {
        text += "\\" + hostParts[at];
    }

    return PathFromUtf8(text.size() == 2 ? text + "\\" : text);
}
