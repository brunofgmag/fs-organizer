#include "domain/model/PackageVersion.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <vector>

namespace
{
    [[nodiscard]] std::vector<unsigned long long> NumbersIn(const std::string& version)
    {
        std::vector<unsigned long long> numbers;

        for (std::size_t at = 0; at < version.size();)
        {
            if (std::isdigit(static_cast<unsigned char>(version[at])) == 0)
            {
                ++at;
                continue;
            }

            unsigned long long number = 0;
            while (at < version.size() && std::isdigit(static_cast<unsigned char>(version[at])) != 0)
            {
                number = number * 10 + static_cast<unsigned long long>(version[at] - '0');
                ++at;
            }

            numbers.push_back(number);
        }

        return numbers;
    }

    [[nodiscard]] unsigned long long At(const std::vector<unsigned long long>& numbers, const std::size_t position)
    {
        return position < numbers.size() ? numbers[position] : 0;
    }
}

VersionOrder HowTheVersionCompares(const std::string& version, const std::string& against)
{
    const std::vector<unsigned long long> mine = NumbersIn(version);
    const std::vector<unsigned long long> theirs = NumbersIn(against);

    if (mine.empty() || theirs.empty())
    {
        return VersionOrder::NoOneCanTell;
    }

    for (std::size_t position = 0; position < std::max(mine.size(), theirs.size()); ++position)
    {
        if (At(mine, position) > At(theirs, position))
        {
            return VersionOrder::Newer;
        }

        if (At(mine, position) < At(theirs, position))
        {
            return VersionOrder::Older;
        }
    }

    return VersionOrder::TheSame;
}

bool TakingItBackIsWorthOffering(const std::string& atTheDestination, const std::string& inTheLibrary)
{
    return HowTheVersionCompares(atTheDestination, inTheLibrary) != VersionOrder::Older;
}
