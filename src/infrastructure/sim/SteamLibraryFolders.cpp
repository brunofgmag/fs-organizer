#include "infrastructure/sim/SteamLibraryFolders.h"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "domain/support/PathUtils.h"

namespace
{
    constexpr auto kPathKey = "path";

    enum class TokenKind : int
    {
        Text = 0,
        Opening = 1,
        Closing = 2,
    };

    struct Token
    {
        TokenKind kind = TokenKind::Text;
        std::string text{};
    };

    char Unescaped(const char escaped)
    {
        switch (escaped)
        {
        case 'n': return '\n';
        case 't': return '\t';
        default: return escaped;
        }
    }

    std::size_t EndOfTheLine(const std::string& document, const std::size_t from)
    {
        const std::size_t end = document.find('\n', from);

        return end == std::string::npos ? document.size() : end;
    }

    Token QuotedTextAt(const std::string& document, std::size_t& at)
    {
        Token token;

        for (++at; at < document.size() && document[at] != '"'; ++at)
        {
            if (document[at] == '\\' && at + 1 < document.size())
            {
                ++at;
                token.text.push_back(Unescaped(document[at]));
                continue;
            }

            token.text.push_back(document[at]);
        }

        return token;
    }

    std::vector<Token> TokensOf(const std::string& document)
    {
        std::vector<Token> tokens;

        for (std::size_t at = 0; at < document.size(); ++at)
        {
            if (document.compare(at, 2, "//") == 0)
            {
                at = EndOfTheLine(document, at);
            }
            else if (document[at] == '"')
            {
                tokens.push_back(QuotedTextAt(document, at));
            }
            else if (document[at] == '{')
            {
                tokens.push_back(Token{.kind = TokenKind::Opening});
            }
            else if (document[at] == '}')
            {
                tokens.push_back(Token{.kind = TokenKind::Closing});
            }
        }

        return tokens;
    }

    bool IsThePathKey(const std::string& key)
    {
        return LoweredForComparison(key) == kPathKey;
    }
}

std::vector<std::filesystem::path> SteamLibraryFoldersIn(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    const std::string document((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const std::vector<Token> tokens = TokensOf(document);

    std::vector<std::filesystem::path> folders;

    for (std::size_t at = 0; at + 1 < tokens.size();)
    {
        const bool isAPair = tokens[at].kind == TokenKind::Text && tokens[at + 1].kind == TokenKind::Text;

        if (isAPair && IsThePathKey(tokens[at].text) && !tokens[at + 1].text.empty())
        {
            folders.push_back(PathFromUtf8(tokens[at + 1].text));
        }

        at += isAPair ? 2 : 1;
    }

    return folders;
}
