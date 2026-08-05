#include "infrastructure/sim/ContentXmlPackages.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

#include <QtCore/QByteArray>
#include <QtCore/QLatin1StringView>
#include <QtCore/QXmlStreamReader>

namespace
{
    constexpr std::string_view kGenerationPrefixes[] = {"communityfs24-", "communityfs20-", "fs24-", "fs20-"};

    [[nodiscard]] std::string Lowered(std::string text)
    {
        std::ranges::transform(text, text.begin(),
                               [](const unsigned char character)
                               {
                                   return static_cast<char>(std::tolower(character));
                               });

        return text;
    }

    [[nodiscard]] std::string WithoutGenerationPrefix(const std::string& name)
    {
        for (const std::string_view prefix : kGenerationPrefixes)
        {
            if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
            {
                return name.substr(prefix.size());
            }
        }

        return name;
    }
}

ContentXmlPackages::ContentXmlPackages(const FilesystemProbe& filesystemProbe, const std::filesystem::path& listPath)
{
    const std::optional<std::string> contents = filesystemProbe.ContentsOf(listPath);
    if (!contents.has_value())
    {
        return;
    }

    QXmlStreamReader reader(QByteArray::fromRawData(contents->data(), static_cast<qsizetype>(contents->size())));

    while (!reader.atEnd())
    {
        if (reader.readNext() != QXmlStreamReader::StartElement || reader.name() != QLatin1StringView("Package"))
        {
            continue;
        }

        const std::string name = reader.attributes().value(QLatin1StringView("name")).toString().toStdString();
        names_.insert(WithoutGenerationPrefix(Lowered(name)));
        ++entries_;
    }

    listWasRead_ = !reader.hasError() && entries_ > 0;

    if (listWasRead_)
    {
        takenAt_ = filesystemProbe.LastWriteTime(listPath);
    }
}

PackagePresence ContentXmlPackages::PresenceOf(const std::string_view packageName) const
{
    if (!listWasRead_)
    {
        return PackagePresence::Unverifiable;
    }

    return names_.contains(Lowered(std::string(packageName))) ? PackagePresence::Present : PackagePresence::Absent;
}

std::optional<std::chrono::system_clock::time_point> ContentXmlPackages::ListTakenAt() const
{
    return takenAt_;
}

std::size_t ContentXmlPackages::HowManyEntries() const
{
    return entries_;
}

std::size_t ContentXmlPackages::HowManyPackages() const
{
    return names_.size();
}
