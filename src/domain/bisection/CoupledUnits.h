#ifndef FS_ORGANIZER_DOMAIN_BISECTION_COUPLED_UNITS_H
#define FS_ORGANIZER_DOMAIN_BISECTION_COUPLED_UNITS_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct CouplingFacts
{
    std::filesystem::path folder{};
    std::vector<std::string> modelFolders{};
    std::vector<std::filesystem::path> writesInside{};
    std::vector<std::string> declaredPackages{};
    bool declaresABaseContainer = false;
};

enum class Coupling : int
{
    NotYetMeasured = 0,
    Alone = 1,
    Merge = 2,
    Shadowing = 3,
    OnlyTheSharedModelFolder = 4,
};

struct SearchUnit
{
    std::vector<std::filesystem::path> addons{};
    std::optional<std::filesystem::path> base{};
    Coupling coupling = Coupling::NotYetMeasured;
};

[[nodiscard]] std::vector<SearchUnit> UnitsFrom(const std::vector<CouplingFacts>& facts);

#endif // FS_ORGANIZER_DOMAIN_BISECTION_COUPLED_UNITS_H
