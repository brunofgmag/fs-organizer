#ifndef FS_ORGANIZER_DOMAIN_SUPPORT_STRING_UTILS_H
#define FS_ORGANIZER_DOMAIN_SUPPORT_STRING_UTILS_H

#include <cstddef>
#include <optional>
#include <string>

#include "domain/support/CaseFolding.h"

class LoweredBytes
{
public:
    explicit LoweredBytes(const std::string& text) : text_(text)
    {
    }

    [[nodiscard]] std::optional<char> Next()
    {
        if (readyAt_ == ready_.size())
        {
            if (at_ == text_.size())
            {
                return std::nullopt;
            }

            Lower();
        }

        return ready_[readyAt_++];
    }

private:
    void Lower()
    {
        const ReadCharacter read = CharacterAt(text_, at_);
        const char32_t code = LoweredCodePoint(read.code);

        ready_.clear();
        readyAt_ = 0;

        if (read.length == 1)
        {
            ready_ += static_cast<char>(code & 0xFFU);
        }
        else
        {
            AppendAsUtf8(ready_, code);
        }

        at_ += read.length;
    }

    const std::string& text_;
    std::string ready_;
    std::size_t readyAt_ = 0;
    std::size_t at_ = 0;
};

[[nodiscard]] inline bool EqualsIgnoringCase(const std::string& left, const std::string& right)
{
    LoweredBytes one(left);
    LoweredBytes other(right);

    for (;;)
    {
        const std::optional<char> fromLeft = one.Next();
        const std::optional<char> fromRight = other.Next();

        if (fromLeft != fromRight)
        {
            return false;
        }

        if (!fromLeft.has_value())
        {
            return true;
        }
    }
}

#endif // FS_ORGANIZER_DOMAIN_SUPPORT_STRING_UTILS_H
