#ifndef FS_ORGANIZER_TESTS_SUPPORT_UTF16_TEXT_H
#define FS_ORGANIZER_TESTS_SUPPORT_UTF16_TEXT_H

#include <cstddef>
#include <string>
#include <string_view>

#include <QtCore/QChar>
#include <QtCore/QString>

[[nodiscard]] inline std::string Utf16WithByteOrderMarkOf(const std::string_view utf8, const bool bigEndian)
{
    const QString text = QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));

    std::string bytes = bigEndian ? "\xFE\xFF" : "\xFF\xFE";
    for (const QChar letter : text)
    {
        const auto low = static_cast<char>(letter.unicode() & 0xFFU);
        const auto high = static_cast<char>(letter.unicode() >> 8U);

        bytes.push_back(bigEndian ? high : low);
        bytes.push_back(bigEndian ? low : high);
    }

    return bytes;
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_UTF16_TEXT_H
