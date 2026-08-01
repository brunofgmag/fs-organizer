#ifndef FS_ORGANIZER_INFRASTRUCTURE_LEGACY_INI_LEGACY_CONFIG_READER_H
#define FS_ORGANIZER_INFRASTRUCTURE_LEGACY_INI_LEGACY_CONFIG_READER_H

#include <filesystem>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringDecoder>

#include "domain/legacy/LegacyInstallation.h"

[[nodiscard]] std::optional<QString> DecodeLegacyText(const QByteArray& bytes,
                                                      QStringDecoder::Encoding whenThereIsNoBom);

[[nodiscard]] std::optional<LegacyInstallation> ReadLegacyIni(const std::filesystem::path& file);

#endif // FS_ORGANIZER_INFRASTRUCTURE_LEGACY_INI_LEGACY_CONFIG_READER_H
