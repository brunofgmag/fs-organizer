#include "infrastructure/legacy/LegacyPresetReader.h"

#include <algorithm>
#include <set>
#include <string>
#include <system_error>

#include <QtCore/QFile>
#include <QtCore/QStringList>

#include "domain/support/StringUtils.h"
#include "infrastructure/legacy/IniLegacyConfigReader.h"
#include "support/PathText.h"

namespace
{
    constexpr auto kPresetExtension = ".preset";
    constexpr auto kDisablingMark = u'*';

    bool NamesAFolder(const QString& line)
    {
        return line.contains(QLatin1Char('\\')) || line.contains(QLatin1Char('/')) || line.contains(QLatin1Char(':'));
    }

    void ApplyLine(LegacyPresetSelection& selection, const QString& line)
    {
        if (line.startsWith(kDisablingMark))
        {
            QString name = line;
            name.remove(kDisablingMark);

            selection.disabledAddonNames.push_back(name.toStdString());
            return;
        }

        if (NamesAFolder(line))
        {
            selection.enabledFolders.push_back(AsPath(line));
            return;
        }

        selection.enabledAddonNames.push_back(line.toStdString());
    }
}

std::optional<LegacyPresetSelection> ReadLegacyPreset(const std::filesystem::path& file,
                                                      const QStringDecoder::Encoding whenThereIsNoBom)
{
    QFile source(AsText(file));

    if (!source.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    const std::optional<QString> text = DecodeLegacyText(source.readAll(), whenThereIsNoBom);

    if (!text.has_value())
    {
        return std::nullopt;
    }

    LegacyPresetSelection selection;
    selection.name = file.stem().string();

    std::set<QString> seen;

    for (const QString& line : text->split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        const QString entry = line.trimmed();

        if (!entry.isEmpty() && seen.insert(entry.toLower()).second)
        {
            ApplyLine(selection, entry);
        }
    }

    return selection;
}

std::vector<LegacyPresetSelection> ReadLegacyPresetsIn(const std::filesystem::path& folder,
                                                       const QStringDecoder::Encoding whenThereIsNoBom)
{
    std::error_code failure;
    std::vector<LegacyPresetSelection> presets;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(folder, failure))
    {
        if (entry.is_directory(failure) || !EqualsIgnoringCase(entry.path().extension().string(), kPresetExtension))
        {
            continue;
        }

        if (const std::optional<LegacyPresetSelection> preset = ReadLegacyPreset(entry.path(), whenThereIsNoBom))
        {
            presets.push_back(*preset);
        }
    }

    std::ranges::sort(presets,
                      [](const LegacyPresetSelection& left, const LegacyPresetSelection& right)
                      {
                          return left.name < right.name;
                      });

    return presets;
}
