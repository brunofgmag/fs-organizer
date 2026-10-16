#include "infrastructure/platform/AppImageDesktopEntry.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

#include "domain/support/PathUtils.h"
#include "support/FileWriting.h"

namespace
{
    constexpr std::string_view kExecKey = "Exec=";
    constexpr std::string_view kTryExecKey = "TryExec=";
    constexpr std::string_view kReservedInsideQuotes = "\"`$\\";

    [[nodiscard]] std::string EscapedAsAString(const std::string_view value)
    {
        std::string escaped;

        for (const char character : value)
        {
            switch (character)
            {
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\t': escaped += "\\t"; break;
            case '\r': escaped += "\\r"; break;
            default: escaped += character; break;
            }
        }

        return escaped;
    }

    [[nodiscard]] std::string QuotedAsAnArgument(const std::string_view value)
    {
        std::string quoted = "\"";

        for (const char character : value)
        {
            if (kReservedInsideQuotes.find(character) != std::string_view::npos)
            {
                quoted += '\\';
            }

            quoted += character;

            if (character == '%')
            {
                quoted += '%';
            }
        }

        return quoted + "\"";
    }

    [[nodiscard]] std::optional<std::string> ContentOf(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            return std::nullopt;
        }

        std::ostringstream read;
        read << stream.rdbuf();

        return read.str();
    }

    enum class Holding
    {
        AlreadyHeld,
        Written,
        Failed,
    };

    [[nodiscard]] Holding MakeItHold(const std::filesystem::path& file, const std::string_view wanted)
    {
        if (ContentOf(file) == std::optional<std::string>(wanted))
        {
            return Holding::AlreadyHeld;
        }

        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);

        return !error && WriteFileReplacing(file, wanted) ? Holding::Written : Holding::Failed;
    }
}

std::string DesktopEntryLaunching(const std::string_view shippedEntry, const std::filesystem::path& appImage)
{
    const std::string image = AsUtf8(appImage);
    std::string entry;
    std::size_t from = 0;

    while (from < shippedEntry.size())
    {
        const std::size_t end = shippedEntry.find('\n', from);
        const std::size_t stop = end == std::string_view::npos ? shippedEntry.size() : end;
        const std::string_view line = shippedEntry.substr(from, stop - from);

        if (line.starts_with(kExecKey))
        {
            entry += std::string(kExecKey) + EscapedAsAString(QuotedAsAnArgument(image)) + "\n";
            entry += std::string(kTryExecKey) + EscapedAsAString(image) + "\n";
        }
        else if (!line.starts_with(kTryExecKey))
        {
            entry += std::string(line) + (end == std::string_view::npos ? "" : "\n");
        }

        from = stop + 1;
    }

    return entry;
}

std::filesystem::path DesktopEntryUnder(const std::filesystem::path& dataHome)
{
    return dataHome / "applications" / "fs-organizer.desktop";
}

std::filesystem::path DesktopIconUnder(const std::filesystem::path& dataHome)
{
    return dataHome / "icons" / "hicolor" / "256x256" / "apps" / "fs-organizer.png";
}

DesktopEntryInstall InstallTheDesktopEntry(const std::filesystem::path& appImage,
                                           const std::filesystem::path& dataHome,
                                           const std::string_view shippedEntry,
                                           const std::string_view icon)
{
    const Holding iconHeld = MakeItHold(DesktopIconUnder(dataHome), icon);
    const Holding entryHeld = MakeItHold(DesktopEntryUnder(dataHome), DesktopEntryLaunching(shippedEntry, appImage));

    return DesktopEntryInstall{
        .held = iconHeld != Holding::Failed && entryHeld != Holding::Failed,
        .iconWritten = iconHeld == Holding::Written,
    };
}
