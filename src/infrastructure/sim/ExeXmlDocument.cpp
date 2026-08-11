#include "infrastructure/sim/ExeXmlDocument.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QLatin1StringView>
#include <QtCore/QString>
#include <QtCore/QXmlStreamReader>

#include "domain/support/PathUtils.h"
#include "infrastructure/sim/XmlEscaping.h"

namespace
{
    constexpr std::string_view kEntryOpen = "<Launch.Addon>";
    constexpr std::string_view kEntryClose = "</Launch.Addon>";
    constexpr std::string_view kSwitchOpen = "<Disabled>";
    constexpr std::string_view kSwitchClose = "</Disabled>";
    constexpr std::string_view kPathOpen = "<Path>";
    constexpr std::string_view kPathClose = "</Path>";
    constexpr std::string_view kSwitchedOff = "True";
    constexpr std::string_view kSwitchedOn = "False";

    struct TextRange
    {
        std::size_t from = std::string_view::npos;
        std::size_t to = std::string_view::npos;
    };

    [[nodiscard]] bool WasFound(const TextRange& range)
    {
        return range.from != std::string_view::npos;
    }

    [[nodiscard]] TextRange RangeOf(const std::string_view document,
                                    const std::string_view open,
                                    const std::string_view close,
                                    const std::size_t from,
                                    const std::size_t to)
    {
        const std::size_t opened = document.find(open, from);
        if (opened == std::string_view::npos || opened >= to)
        {
            return {};
        }

        const std::size_t closed = document.find(close, opened + open.size());
        if (closed == std::string_view::npos || closed >= to)
        {
            return {};
        }

        return TextRange{.from = opened + open.size(), .to = closed};
    }

    [[nodiscard]] std::string ComparableTargetIn(const std::string_view document, const TextRange& path)
    {
        return ComparablePath(PathFromUtf8(UnescapedXmlText(document.substr(path.from, path.to - path.from))));
    }

    [[nodiscard]] bool SaysTrue(const QString& text)
    {
        return text.trimmed().compare(QLatin1StringView("true"), Qt::CaseInsensitive) == 0;
    }

    [[nodiscard]] StartupEntry EntryUnder(QXmlStreamReader& reader)
    {
        StartupEntry entry;

        while (!reader.atEnd())
        {
            const QXmlStreamReader::TokenType token = reader.readNext();

            if (token == QXmlStreamReader::EndElement && reader.name() == QLatin1StringView("Launch.Addon"))
            {
                break;
            }

            if (token != QXmlStreamReader::StartElement)
            {
                continue;
            }

            if (reader.name() == QLatin1StringView("Name"))
            {
                entry.label = reader.readElementText().toStdString();
            }
            else if (reader.name() == QLatin1StringView("Path"))
            {
                entry.path = PathFromUtf8(reader.readElementText().toStdString());
            }
            else if (reader.name() == QLatin1StringView("Disabled"))
            {
                entry.enabled = !SaysTrue(reader.readElementText());
            }
        }

        return entry;
    }

    [[nodiscard]] std::string WithTheSwitchSet(const std::string_view document,
                                               const std::size_t contentStart,
                                               const std::size_t contentEnd,
                                               const bool enabled)
    {
        const std::string_view value = enabled ? kSwitchedOn : kSwitchedOff;

        if (const TextRange existing = RangeOf(document, kSwitchOpen, kSwitchClose, contentStart, contentEnd);
            WasFound(existing))
        {
            return std::string(document.substr(0, existing.from)).append(value).append(document.substr(existing.to));
        }

        const std::size_t firstChild = document.find('<', contentStart);
        const std::string_view lead = document.substr(contentStart, firstChild - contentStart);

        return std::string(document.substr(0, firstChild))
            .append(kSwitchOpen)
            .append(value)
            .append(kSwitchClose)
            .append(lead)
            .append(document.substr(firstChild));
    }
}

std::vector<StartupEntry> StartupEntriesIn(const std::string_view document)
{
    std::vector<StartupEntry> entries;

    QXmlStreamReader reader(QByteArray::fromRawData(document.data(), static_cast<qsizetype>(document.size())));

    while (!reader.atEnd())
    {
        if (reader.readNext() != QXmlStreamReader::StartElement || reader.name() != QLatin1StringView("Launch.Addon"))
        {
            continue;
        }

        entries.push_back(EntryUnder(reader));
    }

    return entries;
}

std::optional<std::string>
WithStartupEntrySwitched(const std::string_view document, const std::filesystem::path& entryPath, const bool enabled)
{
    const std::string wanted = ComparablePath(entryPath);

    std::size_t at = 0;

    while (true)
    {
        const std::size_t opened = document.find(kEntryOpen, at);
        if (opened == std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::size_t closed = document.find(kEntryClose, opened);
        if (closed == std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::size_t contentStart = opened + kEntryOpen.size();
        const TextRange path = RangeOf(document, kPathOpen, kPathClose, contentStart, closed);

        if (WasFound(path) && ComparableTargetIn(document, path) == wanted)
        {
            return WithTheSwitchSet(document, contentStart, closed, enabled);
        }

        at = closed + kEntryClose.size();
    }
}
