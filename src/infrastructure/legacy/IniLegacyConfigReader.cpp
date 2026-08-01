#include "infrastructure/legacy/IniLegacyConfigReader.h"

#include <QtCore/QFile>
#include <QtCore/QStringList>

#include "support/PathText.h"

namespace
{
    constexpr auto kAddonPath = "MyAddons_Path";
    constexpr auto kCommunityPath = "MSFSCommunity_Path";
    constexpr auto kPresetsPath = "Presets_Path";
    constexpr auto kLinkType = "Link_Type";

    constexpr auto kUtf8Bom = "\xEF\xBB\xBF";
    constexpr auto kUtf16LittleEndianBom = "\xFF\xFE";
    constexpr auto kUtf16BigEndianBom = "\xFE\xFF";

    std::optional<QStringDecoder::Encoding> EncodingOfTheBom(const QByteArray& bytes)
    {
        if (bytes.startsWith(kUtf8Bom))
        {
            return QStringDecoder::Utf8;
        }

        if (bytes.startsWith(kUtf16LittleEndianBom) || bytes.startsWith(kUtf16BigEndianBom))
        {
            return QStringDecoder::Utf16;
        }

        return std::nullopt;
    }

    QString TrimmedOfTrailingSeparators(QString value)
    {
        while (value.size() > 1 && (value.endsWith(QLatin1Char('\\')) || value.endsWith(QLatin1Char('/')))
               && !value.endsWith(QLatin1String(":\\")) && !value.endsWith(QLatin1String(":/")))
        {
            value.chop(1);
        }

        return value;
    }

    void ApplyLine(LegacyInstallation& installation, const QString& line)
    {
        const QString key = line.section(QLatin1Char('='), 0, 0).trimmed();
        const QString value = TrimmedOfTrailingSeparators(line.section(QLatin1Char('='), 1).trimmed());

        if (value.isEmpty())
        {
            return;
        }

        if (key == QLatin1String(kAddonPath))
        {
            installation.addonPaths.push_back(AsPath(value));
        }
        else if (key == QLatin1String(kCommunityPath))
        {
            installation.communityPath = AsPath(value);
        }
        else if (key == QLatin1String(kPresetsPath))
        {
            installation.presetsPath = AsPath(value);
        }
        else if (key == QLatin1String(kLinkType))
        {
            installation.linkType = value.toStdString();
        }
    }
}

std::optional<QString> DecodeLegacyText(const QByteArray& bytes, const QStringDecoder::Encoding whenThereIsNoBom)
{
    const std::optional<QStringDecoder::Encoding> announced = EncodingOfTheBom(bytes);
    QStringDecoder decoder(announced.value_or(whenThereIsNoBom));

    QString text = decoder.decode(bytes);

    if (decoder.hasError())
    {
        return std::nullopt;
    }

    if (text.startsWith(QChar(0xFEFF)))
    {
        text.remove(0, 1);
    }

    return text;
}

std::optional<LegacyInstallation> ReadLegacyIni(const std::filesystem::path& file)
{
    QFile source(AsText(file));

    if (!source.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    const std::optional<QString> text = DecodeLegacyText(source.readAll(), QStringDecoder::System);

    if (!text.has_value())
    {
        return std::nullopt;
    }

    LegacyInstallation installation;
    installation.folder = file.parent_path();

    for (const QString& line : text->split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        if (line.contains(QLatin1Char('=')))
        {
            ApplyLine(installation, line);
        }
    }

    return installation;
}
