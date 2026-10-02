#include "infrastructure/sim/ExeXmlDocument.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QChar>
#include <QtCore/QLatin1StringView>
#include <QtCore/QString>
#include <QtCore/QStringConverter>
#include <QtCore/QXmlStreamReader>

#include "domain/support/CaseFolding.h"
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
    constexpr std::string_view kRootOpen = "<SimBase.Document";
    constexpr std::string_view kRootClose = "</SimBase.Document>";
    constexpr std::string_view kCommentOpen = "<!--";
    constexpr std::string_view kCommentClose = "-->";
    constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";
    constexpr std::string_view kUtf16LittleEndianMark = "\xFF\xFE";
    constexpr std::string_view kUtf16BigEndianMark = "\xFE\xFF";
    constexpr std::string_view kSwitchedOff = "True";
    constexpr std::string_view kSwitchedOn = "False";
    constexpr std::string_view kDefaultEol = "\r\n";
    constexpr std::string_view kDefaultIndent = "  ";

    constexpr char16_t kWindows1252Controls[32] = {0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                                   0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
                                                   0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                                   0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};

    enum class Codec
    {
        Utf8,
        Windows1252,
        Utf16LittleEndian,
        Utf16BigEndian,
        Unsupported,
    };

    struct TextRange
    {
        std::size_t from = std::string_view::npos;
        std::size_t to = std::string_view::npos;
    };

    struct Element
    {
        TextRange whole{};
        TextRange text{};
        bool selfClosing = false;
    };

    struct EntryBlock
    {
        TextRange tags{};
        TextRange lines{};
        TextRange content{};
        TextRange path{};
    };

    struct Style
    {
        std::string eol{};
        std::string entryIndent{};
        std::string childIndent{};

        [[nodiscard]] bool operator==(const Style& other) const = default;
    };

    struct Replacement
    {
        TextRange range{};
        std::string text{};
    };

    [[nodiscard]] bool WasFound(const TextRange& range)
    {
        return range.from != std::string_view::npos;
    }

    [[nodiscard]] std::string_view Slice(const std::string_view document, const TextRange& range)
    {
        return document.substr(range.from, range.to - range.from);
    }

    [[nodiscard]] bool IsAscii(const std::string_view text)
    {
        return std::ranges::none_of(text,
                                    [](const char letter)
                                    {
                                        return (static_cast<unsigned char>(letter) & 0x80U) != 0;
                                    });
    }

    [[nodiscard]] bool IsValidUtf8(const std::string_view text)
    {
        QStringDecoder decoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
        const QString decoded = decoder(QByteArrayView(text.data(), static_cast<qsizetype>(text.size())));

        return !decoder.hasError();
    }

    [[nodiscard]] std::string_view WithoutByteOrderMark(const std::string_view bytes)
    {
        return bytes.starts_with(kByteOrderMark) ? bytes.substr(kByteOrderMark.size()) : bytes;
    }

    [[nodiscard]] Codec CodecNamed(const std::string& declared)
    {
        const std::string name = LoweredForComparison(declared);

        if (name == "utf-8" || name == "utf8")
        {
            return Codec::Utf8;
        }

        if (name == "windows-1252" || name == "cp1252" || name == "iso-8859-1" || name == "latin1")
        {
            return Codec::Windows1252;
        }

        return Codec::Unsupported;
    }

    [[nodiscard]] Codec CodecOfADocumentThatDeclaresNone(const std::string_view document)
    {
        return IsValidUtf8(document) ? Codec::Utf8 : Codec::Unsupported;
    }

    [[nodiscard]] Codec CodecOf(const std::string_view document)
    {
        if (document.starts_with(kUtf16LittleEndianMark))
        {
            return Codec::Utf16LittleEndian;
        }

        if (document.starts_with(kUtf16BigEndianMark))
        {
            return Codec::Utf16BigEndian;
        }

        const std::string_view start = WithoutByteOrderMark(document);
        if (!start.starts_with("<?xml"))
        {
            return CodecOfADocumentThatDeclaresNone(document);
        }

        const std::string_view declaration = start.substr(0, start.find("?>"));
        const std::size_t key = declaration.find("encoding");
        const std::size_t quote = key == std::string_view::npos ? key : declaration.find_first_of("\"'", key);
        if (quote == std::string_view::npos)
        {
            return CodecOfADocumentThatDeclaresNone(document);
        }

        const std::size_t end = declaration.find(declaration[quote], quote + 1);
        if (end == std::string_view::npos)
        {
            return CodecOfADocumentThatDeclaresNone(document);
        }

        return CodecNamed(std::string(declaration.substr(quote + 1, end - quote - 1)));
    }

    [[nodiscard]] QString Utf16TextOf(const QStringConverter::Encoding encoding, const std::string_view bytes)
    {
        const std::string_view text = bytes.substr(kUtf16LittleEndianMark.size());
        QStringDecoder decoder(encoding);

        return decoder(QByteArrayView(text.data(), static_cast<qsizetype>(text.size())));
    }

    [[nodiscard]] QString TextOf(const Codec codec, const std::string_view bytes)
    {
        if (codec == Codec::Utf16LittleEndian)
        {
            return Utf16TextOf(QStringConverter::Utf16LE, bytes);
        }

        if (codec == Codec::Utf16BigEndian)
        {
            return Utf16TextOf(QStringConverter::Utf16BE, bytes);
        }

        if (codec != Codec::Windows1252)
        {
            const std::string_view text = WithoutByteOrderMark(bytes);

            return QString::fromUtf8(QByteArrayView(text.data(), static_cast<qsizetype>(text.size())));
        }

        QString text;
        text.reserve(static_cast<qsizetype>(bytes.size()));

        for (const char letter : bytes)
        {
            const auto byte = static_cast<unsigned char>(letter);
            const bool isControl = byte >= 0x80 && byte < 0xA0;

            text.append(QChar(isControl ? kWindows1252Controls[byte - 0x80] : static_cast<char16_t>(byte)));
        }

        return text;
    }

    [[nodiscard]] std::string Utf8Of(const Codec codec, const std::string_view bytes)
    {
        if (codec != Codec::Windows1252)
        {
            return std::string(bytes);
        }

        const QByteArray utf8 = TextOf(codec, bytes).toUtf8();

        return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
    }

    [[nodiscard]] std::optional<char> Windows1252ByteOf(const QChar letter)
    {
        const char16_t unit = letter.unicode();

        if (unit < 0x80 || (unit >= 0xA0 && unit <= 0xFF))
        {
            return static_cast<char>(unit);
        }

        const auto* const found = std::ranges::find(kWindows1252Controls, unit);

        return found == std::ranges::end(kWindows1252Controls)
            ? std::nullopt
            : std::optional<char>(static_cast<char>(0x80 + (found - std::ranges::begin(kWindows1252Controls))));
    }

    [[nodiscard]] std::optional<std::string> BytesIn(const Codec codec, const std::string& utf8)
    {
        if (codec == Codec::Utf8 || IsAscii(utf8))
        {
            return utf8;
        }

        if (codec == Codec::Unsupported || !IsValidUtf8(utf8))
        {
            return std::nullopt;
        }

        std::string bytes;

        for (const QChar letter : QString::fromUtf8(QByteArrayView(utf8.data(), static_cast<qsizetype>(utf8.size()))))
        {
            const std::optional<char> byte = Windows1252ByteOf(letter);
            if (!byte.has_value())
            {
                return std::nullopt;
            }

            bytes.push_back(*byte);
        }

        return bytes;
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

    [[nodiscard]] std::optional<Element>
    ElementIn(const std::string_view document, const std::string_view name, const TextRange& within)
    {
        const std::string open = "<" + std::string(name);
        const std::string close = "</" + std::string(name) + ">";

        for (std::size_t at = within.from;;)
        {
            const std::size_t opened = document.find(open, at);
            if (opened == std::string_view::npos || opened >= within.to)
            {
                return std::nullopt;
            }

            const std::size_t afterName = opened + open.size();
            const std::size_t tagEnd = document.find('>', afterName);
            if (tagEnd == std::string_view::npos || tagEnd >= within.to)
            {
                return std::nullopt;
            }

            const std::string_view between = document.substr(afterName, tagEnd - afterName);
            const std::size_t lastLetter = between.find_last_not_of(" \t");

            if (between.empty())
            {
                const std::size_t closed = document.find(close, tagEnd);
                if (closed == std::string_view::npos || closed >= within.to)
                {
                    return std::nullopt;
                }

                return Element{.whole = {opened, closed + close.size()}, .text = {tagEnd + 1, closed}};
            }

            if (lastLetter != std::string_view::npos && between[lastLetter] == '/'
                && between.find_first_not_of(" \t") == lastLetter)
            {
                return Element{.whole = {opened, tagEnd + 1}, .text = {tagEnd, tagEnd}, .selfClosing = true};
            }

            at = tagEnd;
        }
    }

    [[nodiscard]] std::string FullElement(const std::string_view name, const std::string_view text)
    {
        return std::string("<")
            .append(name)
            .append(">")
            .append(EscapedXmlText(text))
            .append("</")
            .append(name)
            .append(">");
    }

    [[nodiscard]] std::size_t
    NextOutsideComments(const std::string_view document, const std::string_view wanted, const std::size_t from)
    {
        for (std::size_t at = from;;)
        {
            const std::size_t found = document.find(wanted, at);
            const std::size_t comment = document.find(kCommentOpen, at);

            if (found == std::string_view::npos || comment == std::string_view::npos || found < comment)
            {
                return found;
            }

            const std::size_t commentEnd = document.find(kCommentClose, comment + kCommentOpen.size());
            if (commentEnd == std::string_view::npos)
            {
                return std::string_view::npos;
            }

            at = commentEnd + kCommentClose.size();
        }
    }

    [[nodiscard]] std::optional<std::size_t> LineStartBefore(const std::string_view document,
                                                             const std::size_t position)
    {
        std::size_t start = position;

        while (start > 0 && (document[start - 1] == ' ' || document[start - 1] == '\t'))
        {
            --start;
        }

        return start == 0 || document[start - 1] == '\n' ? std::optional<std::size_t>(start) : std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> LineEndAfter(const std::string_view document, const std::size_t position)
    {
        std::size_t end = position;

        while (end < document.size() && (document[end] == ' ' || document[end] == '\t'))
        {
            ++end;
        }

        if (end == document.size())
        {
            return end;
        }

        if (document[end] == '\n')
        {
            return end + 1;
        }

        return document.substr(end).starts_with("\r\n") ? std::optional<std::size_t>(end + 2) : std::nullopt;
    }

    [[nodiscard]] TextRange WholeLines(const std::string_view document, const TextRange& range)
    {
        const std::optional<std::size_t> start = LineStartBefore(document, range.from);
        const std::optional<std::size_t> end = LineEndAfter(document, range.to);

        return start.has_value() && end.has_value() ? TextRange{.from = *start, .to = *end} : range;
    }

    [[nodiscard]] std::vector<EntryBlock> BlocksIn(const std::string_view document)
    {
        std::vector<EntryBlock> blocks;

        for (std::size_t at = 0;;)
        {
            const std::size_t opened = NextOutsideComments(document, kEntryOpen, at);
            if (opened == std::string_view::npos)
            {
                return blocks;
            }

            const std::size_t closed = document.find(kEntryClose, opened);
            if (closed == std::string_view::npos)
            {
                return blocks;
            }

            const std::size_t contentStart = opened + kEntryOpen.size();
            const TextRange tags{.from = opened, .to = closed + kEntryClose.size()};

            blocks.push_back(EntryBlock{.tags = tags,
                                        .lines = WholeLines(document, tags),
                                        .content = {contentStart, closed},
                                        .path = RangeOf(document, kPathOpen, kPathClose, contentStart, closed)});

            at = tags.to;
        }
    }

    [[nodiscard]] std::string TargetIn(const std::string_view document, const Codec codec, const TextRange& path)
    {
        std::string target = UnescapedXmlText(Utf8Of(codec, Slice(document, path)));

        return IsValidUtf8(target) ? target : std::string{};
    }

    [[nodiscard]] std::string
    ComparableTargetIn(const std::string_view document, const Codec codec, const TextRange& path)
    {
        const std::string target = TargetIn(document, codec, path);

        return target.empty() ? std::string{} : ComparablePath(PathFromUtf8(target));
    }

    [[nodiscard]] std::optional<std::size_t> IndexOfEntry(const std::string_view document,
                                                          const Codec codec,
                                                          const std::vector<EntryBlock>& blocks,
                                                          const std::filesystem::path& entryPath)
    {
        const std::string wanted = ComparablePath(entryPath);

        for (std::size_t at = 0; at < blocks.size(); ++at)
        {
            if (WasFound(blocks[at].path) && !wanted.empty()
                && ComparableTargetIn(document, codec, blocks[at].path) == wanted)
            {
                return at;
            }
        }

        return std::nullopt;
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
            else if (reader.name() == QLatin1StringView("CommandLine"))
            {
                entry.commandLine = reader.readElementText().toStdString();
            }
            else if (reader.name() == QLatin1StringView("Disabled"))
            {
                entry.enabled = !SaysTrue(reader.readElementText());
            }
        }

        return entry;
    }

    [[nodiscard]] std::vector<StartupEntry> EntriesIn(const Codec codec, const std::string_view bytes)
    {
        std::vector<StartupEntry> entries;

        QXmlStreamReader reader(TextOf(codec, bytes));

        while (!reader.atEnd())
        {
            if (reader.readNext() != QXmlStreamReader::StartElement
                || reader.name() != QLatin1StringView("Launch.Addon"))
            {
                continue;
            }

            entries.push_back(EntryUnder(reader));
        }

        return entries;
    }

    [[nodiscard]] StartupEntry EntryOf(const Codec codec, const std::string_view block)
    {
        const std::vector<StartupEntry> entries = EntriesIn(codec, block);

        return entries.empty() ? StartupEntry{} : entries.front();
    }

    [[nodiscard]] std::string
    WithTheSwitchSet(const std::string_view document, const TextRange& content, const bool enabled)
    {
        const std::string_view value = enabled ? kSwitchedOn : kSwitchedOff;

        if (const TextRange existing = RangeOf(document, kSwitchOpen, kSwitchClose, content.from, content.to);
            WasFound(existing))
        {
            return std::string(document.substr(0, existing.from)).append(value).append(document.substr(existing.to));
        }

        const std::size_t firstChild = document.find('<', content.from);
        const std::string_view lead = document.substr(content.from, firstChild - content.from);

        return std::string(document.substr(0, firstChild))
            .append(kSwitchOpen)
            .append(value)
            .append(kSwitchClose)
            .append(lead)
            .append(document.substr(firstChild));
    }

    [[nodiscard]] std::string EolOf(const std::string_view document)
    {
        const std::size_t newline = document.find('\n');

        if (newline == std::string_view::npos)
        {
            return std::string(kDefaultEol);
        }

        return newline > 0 && document[newline - 1] == '\r' ? "\r\n" : "\n";
    }

    [[nodiscard]] std::string IndentOf(const std::string_view document, const std::size_t position)
    {
        std::size_t start = position;

        while (start > 0 && (document[start - 1] == ' ' || document[start - 1] == '\t'))
        {
            --start;
        }

        return std::string(document.substr(start, position - start));
    }

    [[nodiscard]] std::optional<std::string> FirstChildIndent(const std::string_view document, const std::size_t from)
    {
        const std::size_t child = document.find_first_not_of(" \t\r\n", from);
        const std::string_view run = document.substr(from, child == std::string_view::npos ? 0 : child - from);
        const std::size_t newline = run.rfind('\n');

        return newline == std::string_view::npos ? std::nullopt : std::optional<std::string>(run.substr(newline + 1));
    }

    [[nodiscard]] std::string Deeper(const std::string& indent)
    {
        return indent.empty() ? std::string(kDefaultIndent) : indent + indent;
    }

    [[nodiscard]] Style StyleOf(const std::string_view document, const std::vector<EntryBlock>& blocks)
    {
        Style style{.eol = EolOf(document)};

        if (!blocks.empty())
        {
            style.entryIndent = IndentOf(document, blocks.front().tags.from);
            style.childIndent =
                FirstChildIndent(document, blocks.front().content.from).value_or(Deeper(style.entryIndent));

            return style;
        }

        const std::size_t root = document.find(kRootOpen);
        const std::size_t rootEnd = root == std::string_view::npos ? root : document.find('>', root);

        style.entryIndent = rootEnd == std::string_view::npos
            ? std::string(kDefaultIndent)
            : FirstChildIndent(document, rootEnd + 1).value_or(std::string(kDefaultIndent));
        style.childIndent = Deeper(style.entryIndent);

        return style;
    }

    [[nodiscard]] std::string IndentIn(const std::string_view indent, const Style& from, const Style& to)
    {
        if (indent == from.entryIndent)
        {
            return to.entryIndent;
        }

        return indent == from.childIndent ? to.childIndent : std::string(indent);
    }

    [[nodiscard]] std::string InStyle(const std::string_view block, const Style& from, const Style& to)
    {
        if (from == to)
        {
            return std::string(block);
        }

        std::string restyled;

        for (std::size_t at = 0; at < block.size();)
        {
            const std::size_t newline = block.find('\n', at);
            const bool terminated = newline != std::string_view::npos;
            const std::size_t end = terminated ? newline : block.size();
            const std::size_t textEnd = terminated && end > at && block[end - 1] == '\r' ? end - 1 : end;
            const std::string_view line = block.substr(at, textEnd - at);
            const std::size_t indentSize = std::min(line.find_first_not_of(" \t"), line.size());

            restyled.append(IndentIn(line.substr(0, indentSize), from, to)).append(line.substr(indentSize));

            if (terminated)
            {
                restyled.append(to.eol);
            }

            at = end + (terminated ? 1 : 0);
        }

        return restyled;
    }

    [[nodiscard]] std::string NewBlock(const StartupAddition& addition, const Style& style)
    {
        const auto line = [&style](const std::string& indent, const std::string& text)
        {
            return std::string(indent).append(text).append(style.eol);
        };

        std::string block = line(style.entryIndent, std::string(kEntryOpen));
        block.append(line(style.childIndent, FullElement("Name", addition.label)));
        block.append(line(style.childIndent, FullElement("Disabled", kSwitchedOn)));
        block.append(line(style.childIndent, FullElement("Path", AsUtf8(addition.path))));

        if (!addition.commandLine.empty())
        {
            block.append(line(style.childIndent, FullElement("CommandLine", addition.commandLine)));
        }

        return block.append(line(style.entryIndent, std::string(kEntryClose)));
    }

    struct Insertion
    {
        std::size_t at = 0;
        std::string lead{};
    };

    [[nodiscard]] std::optional<Insertion> AtTheEndOf(const std::string_view document, const Style& style)
    {
        const std::size_t close = document.rfind(kRootClose);
        if (close == std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::optional<std::size_t> lineStart = LineStartBefore(document, close);

        return lineStart.has_value() ? Insertion{.at = *lineStart} : Insertion{.at = close, .lead = style.eol};
    }

    [[nodiscard]] std::string
    Inserted(const std::string_view document, const Insertion& where, const std::string_view text)
    {
        return std::string(document.substr(0, where.at))
            .append(where.lead)
            .append(text)
            .append(document.substr(where.at));
    }

    [[nodiscard]] std::string Applied(const std::string_view document, std::vector<Replacement> replacements)
    {
        std::ranges::sort(replacements, std::ranges::greater{},
                          [](const Replacement& replacement)
                          {
                              return std::pair(replacement.range.from, replacement.range.to);
                          });

        std::string result(document);

        for (const Replacement& replacement : replacements)
        {
            result.replace(replacement.range.from, replacement.range.to - replacement.range.from, replacement.text);
        }

        return result;
    }

    [[nodiscard]] std::optional<Replacement> TextReplacement(const std::string_view document,
                                                             const Codec codec,
                                                             const TextRange& text,
                                                             const std::string& wanted)
    {
        if (UnescapedXmlText(Utf8Of(codec, Slice(document, text))) == wanted)
        {
            return std::nullopt;
        }

        return Replacement{.range = text, .text = EscapedXmlText(wanted)};
    }

    [[nodiscard]] std::optional<Replacement> NameReplacement(const std::string_view document,
                                                             const Codec codec,
                                                             const EntryBlock& block,
                                                             const std::string& label)
    {
        const std::optional<Element> name = ElementIn(document, "Name", block.content);

        if (name.has_value() && !name->selfClosing)
        {
            return TextReplacement(document, codec, name->text, label);
        }

        if (label.empty())
        {
            return std::nullopt;
        }

        if (name.has_value())
        {
            return Replacement{.range = name->whole, .text = FullElement("Name", label)};
        }

        const std::size_t firstChild = document.find('<', block.content.from);
        const std::string_view lead = document.substr(block.content.from, firstChild - block.content.from);

        return Replacement{.range = {block.content.from, block.content.from},
                           .text = std::string(lead).append(FullElement("Name", label))};
    }

    [[nodiscard]] std::optional<Replacement> CommandLineReplacement(const std::string_view document,
                                                                    const Codec codec,
                                                                    const EntryBlock& block,
                                                                    const std::string& commandLine,
                                                                    const Style& style)
    {
        const std::optional<Element> existing = ElementIn(document, "CommandLine", block.content);

        if (existing.has_value() && commandLine.empty())
        {
            return Replacement{.range = WholeLines(document, existing->whole), .text = {}};
        }

        if (existing.has_value() && existing->selfClosing)
        {
            return Replacement{.range = existing->whole, .text = FullElement("CommandLine", commandLine)};
        }

        if (existing.has_value())
        {
            return TextReplacement(document, codec, existing->text, commandLine);
        }

        if (commandLine.empty())
        {
            return std::nullopt;
        }

        const std::size_t afterPath = block.path.to + kPathClose.size();
        const std::string indent = IndentOf(document, block.path.from - kPathOpen.size());

        return Replacement{.range = {afterPath, afterPath},
                           .text =
                               std::string(style.eol).append(indent).append(FullElement("CommandLine", commandLine))};
    }

    [[nodiscard]] std::filesystem::path PathOfTheEntryAfter(const std::string_view document,
                                                            const Codec codec,
                                                            const std::vector<EntryBlock>& blocks,
                                                            const std::size_t index)
    {
        for (std::size_t at = index + 1; at < blocks.size(); ++at)
        {
            const std::string target =
                WasFound(blocks[at].path) ? TargetIn(document, codec, blocks[at].path) : std::string{};

            if (!target.empty())
            {
                return PathFromUtf8(target);
            }
        }

        return {};
    }

    [[nodiscard]] RemovedStartupEntry TakenFrom(const std::string_view document,
                                                const Codec codec,
                                                const std::vector<EntryBlock>& blocks,
                                                const std::size_t index)
    {
        const StartupEntry entry = EntryOf(codec, Slice(document, blocks[index].tags));

        return RemovedStartupEntry{.label = entry.label,
                                   .path = entry.path,
                                   .commandLine = entry.commandLine,
                                   .enabled = entry.enabled,
                                   .block = Utf8Of(codec, Slice(document, blocks[index].lines)),
                                   .followedBy = PathOfTheEntryAfter(document, codec, blocks, index)};
    }

    [[nodiscard]] std::vector<Replacement> Present(const std::initializer_list<std::optional<Replacement>> candidates)
    {
        std::vector<Replacement> present;

        for (const std::optional<Replacement>& candidate : candidates)
        {
            if (candidate.has_value())
            {
                present.push_back(*candidate);
            }
        }

        return present;
    }

    [[nodiscard]] StartupDocumentChange Refused(const FileResult result)
    {
        return StartupDocumentChange{.result = result};
    }

    [[nodiscard]] bool AnyNeedsUtf8(const Codec codec, const std::initializer_list<std::string_view> texts)
    {
        return codec != Codec::Utf8 && !std::ranges::all_of(texts, IsAscii);
    }
}

std::vector<StartupEntry> StartupEntriesIn(const std::string_view document)
{
    return EntriesIn(CodecOf(document), document);
}

bool StartupDocumentIsUtf16(const std::string_view document)
{
    const Codec codec = CodecOf(document);

    return codec == Codec::Utf16LittleEndian || codec == Codec::Utf16BigEndian;
}

std::optional<std::string>
WithStartupEntrySwitched(const std::string_view document, const std::filesystem::path& entryPath, const bool enabled)
{
    const Codec codec = CodecOf(document);
    const std::vector<EntryBlock> blocks = BlocksIn(document);

    const std::optional<std::size_t> index = IndexOfEntry(document, codec, blocks, entryPath);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    return WithTheSwitchSet(document, blocks[*index].content, enabled);
}

std::string NewStartupDocument()
{
    return std::string(kByteOrderMark)
        .append("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n")
        .append("<SimBase.Document Type=\"Launch\" version=\"1,0\">\r\n")
        .append("  <Descr>Launch</Descr>\r\n")
        .append("  <Filename>EXE.xml</Filename>\r\n")
        .append("  <Disabled>False</Disabled>\r\n")
        .append("  <Launch.ManualLoad>False</Launch.ManualLoad>\r\n")
        .append("</SimBase.Document>\r\n");
}

StartupDocumentChange WithStartupEntryAdded(const std::string_view document, const StartupAddition& addition)
{
    const Codec codec = CodecOf(document);
    const std::string path = AsUtf8(addition.path);

    if (AnyNeedsUtf8(codec, {addition.label, path, addition.commandLine}))
    {
        return Refused(FileResult::TheStartupFileIsNotUtf8);
    }

    const std::vector<EntryBlock> blocks = BlocksIn(document);

    if (IndexOfEntry(document, codec, blocks, addition.path).has_value())
    {
        return Refused(FileResult::TheStartupEntryIsAlreadyThere);
    }

    const Style style = StyleOf(document, blocks);
    const std::optional<Insertion> where = AtTheEndOf(document, style);
    if (!where.has_value())
    {
        return Refused(FileResult::CouldNotReadTheStartupFile);
    }

    return StartupDocumentChange{.document = Inserted(document, *where, NewBlock(addition, style))};
}

StartupDocumentChange WithStartupEntryRemoved(const std::string_view document, const std::filesystem::path& entryPath)
{
    const Codec codec = CodecOf(document);
    const std::vector<EntryBlock> blocks = BlocksIn(document);

    const std::optional<std::size_t> index = IndexOfEntry(document, codec, blocks, entryPath);
    if (!index.has_value())
    {
        return Refused(FileResult::TheDiskDisagreesWithTheScan);
    }

    const TextRange lines = blocks[*index].lines;

    return StartupDocumentChange{.document = Applied(document, {Replacement{.range = lines, .text = {}}}),
                                 .was = EntryOf(codec, Slice(document, blocks[*index].tags)),
                                 .taken = TakenFrom(document, codec, blocks, *index)};
}

StartupDocumentChange WithStartupEntryEdited(const std::string_view document, const StartupEditing& editing)
{
    const Codec codec = CodecOf(document);
    const std::vector<EntryBlock> blocks = BlocksIn(document);

    const std::optional<std::size_t> index = IndexOfEntry(document, codec, blocks, editing.path);
    if (!index.has_value())
    {
        return Refused(FileResult::TheDiskDisagreesWithTheScan);
    }

    const bool movesTheEntry = ComparablePath(editing.newPath) != ComparablePath(editing.path);
    if (movesTheEntry && IndexOfEntry(document, codec, blocks, editing.newPath).has_value())
    {
        return Refused(FileResult::TheStartupEntryIsAlreadyThere);
    }

    const EntryBlock& block = blocks[*index];
    const Style style = StyleOf(document, blocks);

    std::vector<Replacement> replacements =
        Present({NameReplacement(document, codec, block, editing.label),
                 TextReplacement(document, codec, block.path, AsUtf8(editing.newPath)),
                 CommandLineReplacement(document, codec, block, editing.commandLine, style)});

    if (std::ranges::any_of(replacements,
                            [codec](const Replacement& replacement)
                            {
                                return AnyNeedsUtf8(codec, {replacement.text});
                            }))
    {
        return Refused(FileResult::TheStartupFileIsNotUtf8);
    }

    return StartupDocumentChange{.document = Applied(document, std::move(replacements)),
                                 .was = EntryOf(codec, Slice(document, block.tags)),
                                 .taken = movesTheEntry
                                     ? std::optional<RemovedStartupEntry>(TakenFrom(document, codec, blocks, *index))
                                     : std::nullopt};
}

StartupDocumentChange WithStartupEntryRestored(const std::string_view document, const RemovedStartupEntry& removed)
{
    const Codec codec = CodecOf(document);
    const std::vector<EntryBlock> blocks = BlocksIn(document);

    if (IndexOfEntry(document, codec, blocks, removed.path).has_value())
    {
        return Refused(FileResult::TheStartupEntryIsAlreadyThere);
    }

    const std::optional<std::string> kept = BytesIn(codec, removed.block);
    if (!kept.has_value())
    {
        return Refused(FileResult::TheStartupFileIsNotUtf8);
    }

    const Style style = StyleOf(document, blocks);
    const std::string text = InStyle(*kept, StyleOf(*kept, BlocksIn(*kept)), style);

    const std::optional<std::size_t> before = IndexOfEntry(document, codec, blocks, removed.followedBy);
    if (before.has_value())
    {
        return StartupDocumentChange{.document = Inserted(document, {.at = blocks[*before].lines.from}, text)};
    }

    const std::optional<Insertion> where = AtTheEndOf(document, style);
    if (!where.has_value())
    {
        return Refused(FileResult::CouldNotReadTheStartupFile);
    }

    return StartupDocumentChange{.document = Inserted(document, *where, text)};
}
