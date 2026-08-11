#include "infrastructure/sim/ContentXmlDocument.h"

#include <cstddef>

#include <QtCore/QByteArray>
#include <QtCore/QLatin1StringView>
#include <QtCore/QXmlStreamReader>

#include "infrastructure/sim/XmlEscaping.h"

namespace
{
    constexpr std::string_view kEntryOpen = "<Package";
    constexpr std::string_view kName = "name";
    constexpr std::string_view kActivation = "active";
    constexpr std::string_view kActivated = "Activated";
    constexpr std::string_view kUserDisabled = "UserDisabled";
    constexpr std::string_view kSystemDisabled = "SystemDisabled";

    struct TextRange
    {
        std::size_t from = std::string_view::npos;
        std::size_t to = std::string_view::npos;
    };

    [[nodiscard]] bool WasFound(const TextRange& range)
    {
        return range.from != std::string_view::npos;
    }

    [[nodiscard]] bool ItIsNameCharacter(const char character)
    {
        return character != '=' && character != '>' && character != '/' && character != '<'
            && static_cast<unsigned char>(character) > ' ';
    }

    [[nodiscard]] std::size_t PastTheBlanksFrom(const std::string_view document, std::size_t at)
    {
        while (at < document.size() && static_cast<unsigned char>(document[at]) <= ' ')
        {
            ++at;
        }

        return at;
    }

    [[nodiscard]] TextRange AttributeIn(const std::string_view document,
                                        const std::string_view attribute,
                                        const std::size_t from,
                                        const std::size_t to)
    {
        for (std::size_t at = from; at < to;)
        {
            const std::size_t named = document.find(attribute, at);
            if (named == std::string_view::npos || named + attribute.size() >= to)
            {
                return {};
            }

            at = named + attribute.size();

            if (ItIsNameCharacter(document[named - 1]) || ItIsNameCharacter(document[at]))
            {
                continue;
            }

            const std::size_t equals = PastTheBlanksFrom(document, at);
            if (equals >= to || document[equals] != '=')
            {
                continue;
            }

            const std::size_t quoted = PastTheBlanksFrom(document, equals + 1);
            if (quoted >= to || (document[quoted] != '"' && document[quoted] != '\''))
            {
                continue;
            }

            const std::size_t closed = document.find(document[quoted], quoted + 1);

            return closed < to ? TextRange{.from = quoted + 1, .to = closed} : TextRange{};
        }

        return {};
    }

    [[nodiscard]] PackageActivation ActivationCalled(const QStringView value)
    {
        if (value == QLatin1StringView(kActivated.data(), static_cast<qsizetype>(kActivated.size())))
        {
            return PackageActivation::Activated;
        }

        if (value == QLatin1StringView(kUserDisabled.data(), static_cast<qsizetype>(kUserDisabled.size())))
        {
            return PackageActivation::UserDisabled;
        }

        if (value == QLatin1StringView(kSystemDisabled.data(), static_cast<qsizetype>(kSystemDisabled.size())))
        {
            return PackageActivation::SystemDisabled;
        }

        return PackageActivation::ItSaysSomethingElse;
    }
}

std::vector<PackageEntry> PackageEntriesIn(const std::string_view document)
{
    std::vector<PackageEntry> entries;

    QXmlStreamReader reader(QByteArray::fromRawData(document.data(), static_cast<qsizetype>(document.size())));

    while (!reader.atEnd())
    {
        if (reader.readNext() != QXmlStreamReader::StartElement || reader.name() != QLatin1StringView("Package"))
        {
            continue;
        }

        const QXmlStreamAttributes attributes = reader.attributes();

        entries.push_back(
            {.name = attributes.value(QLatin1StringView(kName.data(), static_cast<qsizetype>(kName.size())))
                         .toString()
                         .toStdString(),
             .activation = ActivationCalled(
                 attributes.value(QLatin1StringView(kActivation.data(), static_cast<qsizetype>(kActivation.size()))))});
    }

    return entries;
}

std::optional<std::string>
WithPackageSwitched(const std::string_view document, const std::string_view packageName, const bool activated)
{
    for (std::size_t at = 0;;)
    {
        const std::size_t opened = document.find(kEntryOpen, at);
        if (opened == std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::size_t closed = document.find('>', opened);
        if (closed == std::string_view::npos)
        {
            return std::nullopt;
        }

        at = closed + 1;

        const std::size_t attributes = opened + kEntryOpen.size();

        const TextRange name = AttributeIn(document, kName, attributes, closed);
        if (!WasFound(name) || UnescapedXmlText(document.substr(name.from, name.to - name.from)) != packageName)
        {
            continue;
        }

        const TextRange activation = AttributeIn(document, kActivation, attributes, closed);
        if (!WasFound(activation))
        {
            return std::nullopt;
        }

        return std::string(document.substr(0, activation.from))
            .append(activated ? kActivated : kUserDisabled)
            .append(document.substr(activation.to));
    }
}
