#ifndef FS_ORGANIZER_APPLICATION_BISECTION_SERVICE_H
#define FS_ORGANIZER_APPLICATION_BISECTION_SERVICE_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "application/ProfileService.h"
#include "application/model/LinkOperationResult.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/BisectionStore.h"
#include "domain/bisection/BisectionDrift.h"
#include "domain/bisection/BisectionRounds.h"
#include "domain/bisection/CouplingScan.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/FilesystemProbe.h"

enum class BisectionRefusal : int
{
    None = 0,
    NothingIsEnabledToSearch = 1,
    TheStateCouldNotBeWritten = 2,
    TheDiskMovedSinceTheLastRound = 3,
    NoProcedureIsRunning = 4,
    ThisUnitDoesNotSplit = 5,
};

enum class ResumeChoice : int
{
    CarryOnFromWhereItStopped = 0,
    PutBackTheStartingConfiguration = 1,
    ForgetItAndLeaveTheDiskAsItIs = 2,
};

struct BisectionReport
{
    BisectionRefusal refusal = BisectionRefusal::None;
    std::vector<Divergence> drift{};
    std::vector<LinkOperationResult> results{};
    std::vector<std::filesystem::path> addonsTurnedOn{};
    std::vector<std::filesystem::path> whatIsLeft{};
    BisectionOutcome outcome = BisectionOutcome::StillSearching;
    std::size_t round = 0;
    std::size_t units = 0;
    std::size_t roundsInTheWorstCase = 0;
    std::size_t outOfReach = 0;
    bool aSecondPassIsPossible = false;
};

class BisectionService
{
public:
    BisectionService(ProfileService& profiles,
                     const CouplingScan& coupling,
                     const FilesystemProbe& filesystemProbe,
                     BisectionStore& store);

    [[nodiscard]] BisectionReport Begin(const SimulatorProfile& profile, const ProfileSnapshot& shown);

    [[nodiscard]] BisectionReport Answer(const SimulatorProfile& profile, BisectionAnswer answer);

    [[nodiscard]] BisectionReport Refine(const SimulatorProfile& profile);

    [[nodiscard]] BisectionReport Stop(const SimulatorProfile& profile);

    [[nodiscard]] std::optional<BisectionRun> WhatWasInterrupted(const std::string& profileId) const;

    [[nodiscard]] BisectionReport Resume(const SimulatorProfile& profile, ResumeChoice choice);

private:
    struct Reading
    {
        ProfileSnapshot snapshot{};
        DiskAsItWas disk{};
    };

    [[nodiscard]] Reading ReadTheDisk(const SimulatorProfile& profile) const;

    [[nodiscard]] std::size_t WhatCarriesOnOutOfReach(const std::vector<DestinationEntry>& entries) const;

    [[nodiscard]] BisectionReport
    ApplyTheRound(const SimulatorProfile& profile, const BisectionRun& run, const Reading& reading);

    [[nodiscard]] BisectionReport TellAbout(const BisectionRun& run, const Reading& reading) const;

    [[nodiscard]] BisectionReport
    Refusing(const BisectionRun& run, const Reading& reading, BisectionRefusal refusal) const;

    [[nodiscard]] BisectionReport TakeTheNextRound(const SimulatorProfile& profile,
                                                   const BisectionRun& run,
                                                   const BisectionRun& next,
                                                   const Reading& reading);

    [[nodiscard]] static std::vector<std::filesystem::path> TheSearchSpaceOf(const BisectionRun& run);

    [[nodiscard]] std::vector<LinkOperationResult> PutBack(const SimulatorProfile& profile,
                                                           const std::vector<PresetEntry>& configuration);

    [[nodiscard]] std::vector<Divergence> WhatMovedSince(const DiskAsItWas& now) const;

    void AdoptAsTheBaseline(const DiskAsItWas& disk);

    ProfileService& profiles_;
    const CouplingScan& coupling_;
    const FilesystemProbe& filesystemProbe_;
    BisectionStore& store_;
    DiskAsItWas leftBehind_;
    bool weLeftARound_ = false;
};

#endif // FS_ORGANIZER_APPLICATION_BISECTION_SERVICE_H
