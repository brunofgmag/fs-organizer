#ifndef FS_ORGANIZER_DOMAIN_BISECTION_BISECTION_ROUNDS_H
#define FS_ORGANIZER_DOMAIN_BISECTION_BISECTION_ROUNDS_H

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "domain/bisection/CoupledUnits.h"
#include "domain/model/Preset.h"

enum class BisectionPass : int
{
    OverTheUnits = 0,
    InsideTheGroup = 1,
};

enum class BisectionAnswer : int
{
    ItCrashed = 0,
    ItRanFine = 1,
};

enum class BisectionOutcome : int
{
    StillSearching = 0,
    OneAddonLeft = 1,
    AnIrreducibleSet = 2,
    NotAmongTheManagedOnes = 3,
};

struct AnsweredRound
{
    std::size_t number = 0;
    BisectionPass pass = BisectionPass::OverTheUnits;
    std::size_t unitsOn = 0;
    BisectionAnswer answer = BisectionAnswer::ItRanFine;
    std::size_t unitsCleared = 0;
    std::size_t unitsLeft = 0;
    std::chrono::system_clock::time_point at{};
};

struct BisectionRun
{
    std::string profileId{};
    std::vector<SearchUnit> units{};
    std::vector<std::size_t> suspects{};
    std::vector<std::size_t> cleared{};
    std::vector<std::filesystem::path> alwaysOn{};
    std::size_t round = 0;
    BisectionPass pass = BisectionPass::OverTheUnits;
    bool theReferenceRoundCrashed = false;
    std::vector<PresetEntry> startingConfiguration{};
    std::chrono::system_clock::time_point startedAt{};
    std::vector<AnsweredRound> story{};
};

struct BisectionRound
{
    std::size_t number = 0;
    std::vector<std::size_t> unitsOn{};
    std::vector<std::filesystem::path> addonsOn{};
};

[[nodiscard]] BisectionRun RunOver(const std::vector<SearchUnit>& units);

[[nodiscard]] BisectionRound TheRound(const BisectionRun& run);

[[nodiscard]] BisectionRun
AfterAnswering(const BisectionRun& run, BisectionAnswer answer, std::chrono::system_clock::time_point at);

[[nodiscard]] BisectionOutcome OutcomeOf(const BisectionRun& run);

[[nodiscard]] std::vector<std::filesystem::path> WhatIsLeft(const BisectionRun& run);

[[nodiscard]] std::size_t RoundsInTheWorstCase(std::size_t units);

[[nodiscard]] bool ItIsTheReferenceRound(const AnsweredRound& answered);

[[nodiscard]] std::size_t LaunchesBehind(const BisectionRun& run);

[[nodiscard]] bool ASecondPassIsPossible(const BisectionRun& run);

[[nodiscard]] BisectionRun IntoTheSecondPass(const BisectionRun& run);

#endif // FS_ORGANIZER_DOMAIN_BISECTION_BISECTION_ROUNDS_H
