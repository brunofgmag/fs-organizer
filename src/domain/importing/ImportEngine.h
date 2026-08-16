#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <functional>

#include "domain/importing/ImportPaths.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/model/CopyOutcome.h"
#include "domain/model/ImportOutcome.h"
#include "domain/model/ImportRequest.h"
#include "domain/model/LinkType.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/Verification.h"
#include "domain/ports/Clock.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/OperationJournal.h"
#include "domain/ports/SidecarStore.h"

inline constexpr std::uintmax_t kFreeSpaceFloor = 64ULL * 1024 * 1024;

inline constexpr std::uintmax_t kRoomEachFileMayRoundUpTo = 4ULL * 1024;

[[nodiscard]] constexpr std::uintmax_t FreeSpaceNeededFor(const std::uintmax_t sourceSize, const std::size_t files)
{
    return sourceSize + kFreeSpaceFloor + static_cast<std::uintmax_t>(files) * kRoomEachFileMayRoundUpTo;
}

struct GiveBackRequest
{
    std::filesystem::path addonFolder{};
    std::filesystem::path externalPath{};
    std::vector<std::filesystem::path> links{};
};

struct QuarantineRequest
{
    AddonId addon{};
    std::filesystem::path loser{};
    std::filesystem::path quarantine{};
    OperationKind kind = OperationKind::QuarantineFromDestination;
};

struct MeasuredSource
{
    ImportOutcome outcome = ImportOutcome::Stopped(FileResult::TheOutcomeIsUnknown);
    std::vector<FileFingerprint> files{};
};

class ImportEngine
{
public:
    ImportEngine(const FilesystemProbe& filesystemProbe,
                 FileOperations& files,
                 SidecarStore& sidecars,
                 const LinkingEngine& linking,
                 const OperationLog& log,
                 LinkType linkType,
                 Verification verification);

    void UseLinkType(LinkType linkType);

    void UseVerification(Verification verification);

    [[nodiscard]] ImportOutcome Import(const SimulatorProfile& profile,
                                       const ImportRequest& request,
                                       const std::function<bool(const CopyProgress&)>& onProgress,
                                       const std::function<void(OperationKind)>& onStep = {}) const;

    [[nodiscard]] ImportOutcome GiveBack(const SimulatorProfile& profile,
                                         const GiveBackRequest& request,
                                         const std::function<bool(const CopyProgress&)>& onProgress,
                                         const std::function<void(OperationKind)>& onStep = {}) const;

    [[nodiscard]] ImportOutcome Quarantine(const QuarantineRequest& request,
                                           const std::function<bool(const CopyProgress&)>& onProgress,
                                           const std::function<void(OperationKind)>& onStep = {}) const;

private:
    [[nodiscard]] MeasuredSource MeasureTheSource(const std::filesystem::path& source,
                                                  const std::filesystem::path& roomOn) const;

    [[nodiscard]] ImportOutcome CopyAndVerify(const AddonId& addon,
                                              const std::filesystem::path& source,
                                              const std::filesystem::path& target,
                                              const std::vector<FileFingerprint>& expected,
                                              const std::function<bool(const CopyProgress&)>& onProgress,
                                              const std::function<void(OperationKind)>& onStep) const;

    [[nodiscard]] ImportOutcome CheckTheStaging(const std::filesystem::path& source,
                                                const std::filesystem::path& staging,
                                                const std::vector<FileFingerprint>& expected,
                                                const std::function<bool(const CopyProgress&)>& onProgress) const;

    [[nodiscard]] ImportOutcome CompareTheContents(const std::filesystem::path& source,
                                                   const std::filesystem::path& staging,
                                                   const std::vector<FileFingerprint>& expected,
                                                   const std::function<bool(const CopyProgress&)>& onProgress) const;

    [[nodiscard]] ImportOutcome PutIntoPlace(const AddonId& addon,
                                             const std::filesystem::path& target,
                                             const std::function<void(OperationKind)>& onStep) const;

    [[nodiscard]] FileResult WhatStoppedIt(const std::filesystem::path& folder, FileResult otherwise) const;

    [[nodiscard]] ImportOutcome CheckTheSource(const SimulatorProfile& profile, const ImportRequest& request) const;

    [[nodiscard]] ImportOutcome CheckTheFolderWeAreGivingBack(const GiveBackRequest& request) const;

    [[nodiscard]] ImportOutcome
    TheOtherProgramTakesItsFolderBack(const AddonId& addon,
                                      const GiveBackRequest& request,
                                      const std::filesystem::path& staging,
                                      const std::function<void(OperationKind)>& onStep) const;

    [[nodiscard]] ImportOutcome RepointTheLinks(const AddonId& addon, const GiveBackRequest& request) const;

    [[nodiscard]] ImportOutcome PrepareTheOtherProgramsFolder(const std::filesystem::path& externalSource,
                                                              const std::filesystem::path& target) const;

    [[nodiscard]] ImportOutcome TakeOverTheOtherProgramsFolder(const AddonId& addon,
                                                               const ImportRequest& request,
                                                               const std::filesystem::path& target) const;

    [[nodiscard]] ImportOutcome
    CheckFreeSpace(const std::filesystem::path& category, std::uintmax_t sourceSize, std::size_t files) const;

    [[nodiscard]] ImportOutcome CopyToStaging(const std::filesystem::path& source,
                                              const std::filesystem::path& staging,
                                              const std::function<bool(const CopyProgress&)>& onProgress) const;

    void RecordStep(const AddonId& addon,
                    OperationKind kind,
                    const std::filesystem::path& source,
                    const std::filesystem::path& target,
                    FileResult result) const;

    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
    SidecarStore& sidecars_;
    const LinkingEngine& linking_;
    const OperationLog& log_;
    LinkType linkType_;
    Verification verification_;
};

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_IMPORT_ENGINE_H
