#ifndef FS_ORGANIZER_SUPPORT_FILE_READING_H
#define FS_ORGANIZER_SUPPORT_FILE_READING_H

#include <cstddef>
#include <istream>
#include <optional>
#include <string>

[[nodiscard]] inline std::optional<std::string> BytesRemainingIn(std::istream& stream)
{
    constexpr std::size_t kBytesReadAtATime = 64 * 1024;

    std::string bytes;
    std::string block(kBytesReadAtATime, '\0');

    while (stream.read(block.data(), static_cast<std::streamsize>(block.size())) || stream.gcount() > 0)
    {
        bytes.append(block.data(), static_cast<std::size_t>(stream.gcount()));
    }

    if (stream.bad())
    {
        return std::nullopt;
    }

    return bytes;
}

#endif // FS_ORGANIZER_SUPPORT_FILE_READING_H
