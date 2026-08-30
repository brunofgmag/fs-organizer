#include "infrastructure/sim/ContentXmlPackages.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

#include <QtCore/QByteArray>
#include <QtCore/QLatin1StringView>
#include <QtCore/QXmlStreamReader>

#include "infrastructure/sim/PackageNaming.h"

namespace
{
    [[nodiscard]] std::string Lowered(std::string text)
    {
        std::ranges::transform(text, text.begin(),
                               [](const unsigned char character)
                               {
                                   return static_cast<char>(std::tolower(character));
                               });

        return text;
    }
}

ContentXmlPackages::ContentXmlPackages(const FilesystemProbe& filesystemProbe, std::filesystem::path listPath)
    : filesystemProbe_(filesystemProbe)
{
    ReadAgain(std::move(listPath));
}

void ContentXmlPackages::Forget(std::filesystem::path listPath)
{
    listPath_ = std::move(listPath);
    names_.clear();
    takenAt_.reset();
    entries_ = 0;
    listWasRead_ = false;
}

void ContentXmlPackages::ReadAgain(std::filesystem::path listPath)
{
    if (listWasRead_ && listPath == listPath_ && filesystemProbe_.LastWriteTime(listPath) == takenAt_)
    {
        return;
    }

    Forget(std::move(listPath));

    const std::optional<std::string> contents = filesystemProbe_.ContentsOf(listPath_);
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
        names_.insert(WithoutTheGenerationPrefix(Lowered(name)));
        ++entries_;
    }

    listWasRead_ = !reader.hasError() && entries_ > 0;

    if (listWasRead_)
    {
        takenAt_ = filesystemProbe_.LastWriteTime(listPath_);
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

std::string ContentXmlPackages::ListAccountFolder() const
{
    return {};
}
