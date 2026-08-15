#ifndef FS_ORGANIZER_DOMAIN_BISECTION_BISECTION_DRIFT_H
#define FS_ORGANIZER_DOMAIN_BISECTION_BISECTION_DRIFT_H

#include <filesystem>
#include <vector>

#include "domain/model/DestinationEntry.h"

enum class DriftKind : int
{
    ALinkWeLeftIsGone = 0,
    AnEntryWeDidNotLeaveIsThere = 1,
    AnEntryPointsSomewhereElse = 2,
    AnAddonLeftTheLibrary = 3,
    AnAddonJoinedTheLibrary = 4,
};

struct Divergence
{
    DriftKind kind = DriftKind::ALinkWeLeftIsGone;
    std::filesystem::path path{};
};

struct DiskAsItWas
{
    std::vector<DestinationEntry> entries{};
    std::vector<std::filesystem::path> libraryAddons{};
};

[[nodiscard]] std::vector<Divergence> DriftBetween(const DiskAsItWas& before, const DiskAsItWas& now);

[[nodiscard]] bool NothingThatLoadedMoved(const std::vector<Divergence>& drift);

#endif // FS_ORGANIZER_DOMAIN_BISECTION_BISECTION_DRIFT_H
