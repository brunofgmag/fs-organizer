#ifndef FS_ORGANIZER_TESTS_SUPPORT_ARCHIVE_BUILDERS_H
#define FS_ORGANIZER_TESTS_SUPPORT_ARCHIVE_BUILDERS_H

#include <cstdint>
#include <string>
#include <vector>

struct ArchiveItem
{
    std::string name{};
    std::string content{};
    bool folder = false;
    bool encrypted = false;
    std::uint32_t unixMode = 0;
    std::uint32_t redirection = 0;
    std::string redirectedTo{};
};

[[nodiscard]] inline std::uint32_t Crc32Of(const std::string& content)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const char letter : content)
    {
        crc ^= static_cast<unsigned char>(letter);
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }

    return ~crc;
}

inline void AppendLittleEndian(std::string& into, const std::uint64_t value, const int width)
{
    for (int shift = 0; shift < width * 8; shift += 8)
    {
        into.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}

[[nodiscard]] inline std::string ZipOf(const std::vector<ArchiveItem>& items)
{
    constexpr std::uint32_t kUtf8Names = 0x0800;
    constexpr std::uint32_t kEncrypted = 0x0001;
    constexpr std::uint32_t kMadeOnUnix = 0x0314;
    constexpr std::uint32_t kFolderMode = 040755;
    constexpr std::uint32_t kFileMode = 0100644;

    std::string local;
    std::string central;
    for (const ArchiveItem& item : items)
    {
        const std::string name = item.folder ? item.name + "/" : item.name;
        const std::uint32_t crc = Crc32Of(item.content);
        const auto size = static_cast<std::uint32_t>(item.content.size());
        const auto offset = static_cast<std::uint32_t>(local.size());
        const std::uint32_t flags = kUtf8Names | (item.encrypted ? kEncrypted : 0U);
        const std::uint32_t mode = item.unixMode != 0 ? item.unixMode : (item.folder ? kFolderMode : kFileMode);

        AppendLittleEndian(local, 0x04034B50U, 4);
        AppendLittleEndian(local, 20, 2);
        AppendLittleEndian(local, flags, 2);
        AppendLittleEndian(local, 0, 2);
        AppendLittleEndian(local, 0, 2);
        AppendLittleEndian(local, 0x5821, 2);
        AppendLittleEndian(local, crc, 4);
        AppendLittleEndian(local, size, 4);
        AppendLittleEndian(local, size, 4);
        AppendLittleEndian(local, name.size(), 2);
        AppendLittleEndian(local, 0, 2);
        local += name;
        local += item.content;

        AppendLittleEndian(central, 0x02014B50U, 4);
        AppendLittleEndian(central, kMadeOnUnix, 2);
        AppendLittleEndian(central, 20, 2);
        AppendLittleEndian(central, flags, 2);
        AppendLittleEndian(central, 0, 2);
        AppendLittleEndian(central, 0, 2);
        AppendLittleEndian(central, 0x5821, 2);
        AppendLittleEndian(central, crc, 4);
        AppendLittleEndian(central, size, 4);
        AppendLittleEndian(central, size, 4);
        AppendLittleEndian(central, name.size(), 2);
        AppendLittleEndian(central, 0, 2);
        AppendLittleEndian(central, 0, 2);
        AppendLittleEndian(central, 0, 2);
        AppendLittleEndian(central, 0, 2);
        AppendLittleEndian(central, static_cast<std::uint64_t>(mode) << 16, 4);
        AppendLittleEndian(central, offset, 4);
        central += name;
    }

    std::string end;
    AppendLittleEndian(end, 0x06054B50U, 4);
    AppendLittleEndian(end, 0, 2);
    AppendLittleEndian(end, 0, 2);
    AppendLittleEndian(end, items.size(), 2);
    AppendLittleEndian(end, items.size(), 2);
    AppendLittleEndian(end, central.size(), 4);
    AppendLittleEndian(end, local.size(), 4);
    AppendLittleEndian(end, 0, 2);

    return local + central + end;
}

[[nodiscard]] inline std::string Vint(std::uint64_t value)
{
    std::string out;
    do
    {
        auto byte = static_cast<unsigned char>(value & 0x7FU);
        value >>= 7;
        out.push_back(static_cast<char>(value != 0 ? byte | 0x80U : byte));
    } while (value != 0);

    return out;
}

[[nodiscard]] inline std::string Rar5Block(const std::string& body)
{
    const std::string sized = Vint(body.size()) + body;
    std::string block;
    AppendLittleEndian(block, Crc32Of(sized), 4);

    return block + sized;
}

[[nodiscard]] inline std::string Rar5Of(const std::vector<ArchiveItem>& items)
{
    constexpr std::uint64_t kHasData = 0x2;
    constexpr std::uint64_t kHasExtra = 0x1;
    constexpr std::uint64_t kRedirection = 5;
    constexpr std::uint64_t kIsAFolder = 0x1;
    constexpr std::uint64_t kHasACrc = 0x4;
    constexpr std::uint64_t kStoredOnWindows = 0;

    std::string archive("Rar!\x1A\x07\x01\x00", 8);
    archive += Rar5Block(Vint(1) + Vint(0) + Vint(0));

    for (const ArchiveItem& item : items)
    {
        std::string extra;
        if (item.redirection != 0)
        {
            const std::string record = Vint(kRedirection) + Vint(item.redirection) + Vint(0)
                + Vint(item.redirectedTo.size()) + item.redirectedTo;
            extra = Vint(record.size()) + record;
        }

        std::string body = Vint(2) + Vint(kHasData | (extra.empty() ? 0 : kHasExtra));
        if (!extra.empty())
        {
            body += Vint(extra.size());
        }
        body += Vint(item.content.size());
        body += Vint(item.folder ? kIsAFolder : kHasACrc) + Vint(item.content.size()) + Vint(item.folder ? 0x10 : 0x20);
        if (!item.folder)
        {
            AppendLittleEndian(body, Crc32Of(item.content), 4);
        }
        body += Vint(0) + Vint(kStoredOnWindows) + Vint(item.name.size()) + item.name + extra;

        archive += Rar5Block(body) + item.content;
    }

    return archive + Rar5Block(Vint(5) + Vint(0) + Vint(0));
}

[[nodiscard]] inline std::string Rar4Block(const std::string& afterTheCrc)
{
    std::string block;
    AppendLittleEndian(block, Crc32Of(afterTheCrc) & 0xFFFFU, 2);

    return block + afterTheCrc;
}

[[nodiscard]] inline std::string Rar4Of(const std::vector<ArchiveItem>& items)
{
    constexpr std::uint64_t kFolderFlags = 0x80E0;
    constexpr std::uint64_t kFileFlags = 0x8000;
    constexpr std::uint64_t kStoredOnWindows = 2;
    constexpr std::uint64_t kHeaderBytesBesidesTheName = 32;

    std::string archive("Rar!\x1A\x07\x00", 7);
    std::string main;
    AppendLittleEndian(main, 0x73, 1);
    AppendLittleEndian(main, 0, 2);
    AppendLittleEndian(main, 13, 2);
    AppendLittleEndian(main, 0, 6);
    archive += Rar4Block(main);

    for (const ArchiveItem& item : items)
    {
        std::string header;
        AppendLittleEndian(header, 0x74, 1);
        AppendLittleEndian(header, item.folder ? kFolderFlags : kFileFlags, 2);
        AppendLittleEndian(header, kHeaderBytesBesidesTheName + item.name.size(), 2);
        AppendLittleEndian(header, item.content.size(), 4);
        AppendLittleEndian(header, item.content.size(), 4);
        AppendLittleEndian(header, kStoredOnWindows, 1);
        AppendLittleEndian(header, Crc32Of(item.content), 4);
        AppendLittleEndian(header, 0x5821, 4);
        AppendLittleEndian(header, 20, 1);
        AppendLittleEndian(header, 0x30, 1);
        AppendLittleEndian(header, item.name.size(), 2);
        AppendLittleEndian(header, item.folder ? 0x10 : 0x20, 4);
        header += item.name;

        archive += Rar4Block(header) + item.content;
    }

    archive.append("\xC4\x3D\x7B\x00\x40\x07\x00", 7);

    return archive;
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_ARCHIVE_BUILDERS_H
