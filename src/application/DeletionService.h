#ifndef FS_ORGANIZER_APPLICATION_DELETION_SERVICE_H
#define FS_ORGANIZER_APPLICATION_DELETION_SERVICE_H

#include <string>
#include <vector>

#include "application/SizeService.h"
#include "application/model/DeletionPlan.h"
#include "application/ports/ProcessProbe.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/model/TreeNode.h"
#include "domain/ports/FileOperations.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/SidecarStore.h"

class DeletionService
{
public:
    DeletionService(const FilesystemProbe& filesystemProbe,
                    FileOperations& files,
                    SidecarStore& sidecars,
                    const LinkingEngine& linking,
                    const EntryClassifier& classifier,
                    const ProcessProbe& processProbe,
                    const OperationLog& log,
                    const SizeService& sizes);

    [[nodiscard]] DeletionPlan Plan(const SimulatorProfile& profile,
                                    const std::vector<SimulatorProfile>& everyProfile,
                                    const std::vector<const TreeNode*>& nodes) const;

    [[nodiscard]] std::vector<DeletionResult>
    Delete(const std::vector<SimulatorProfile>& everyProfile, const DeletionPlan& plan, DeletionRoute route) const;

private:
    struct LinksNow
    {
        std::string profileId{};
        std::vector<DestinationEntry> entries{};
    };

    [[nodiscard]] std::vector<LinksNow> ReadLinksNow(const std::vector<SimulatorProfile>& everyProfile) const;

    [[nodiscard]] static std::vector<EnabledSomewhere> WhereItIsEnabled(const std::vector<LinksNow>& seen,
                                                                        const std::filesystem::path& folder);

    [[nodiscard]] std::vector<VolumeRoom> RoomOnEachVolume(const std::vector<AddonToDelete>& addons) const;

    [[nodiscard]] static FileResult
    TheRouteRefuses(const DeletionPlan& plan, const AddonToDelete& addon, DeletionRoute route);

    [[nodiscard]] DeletionResult
    DeleteOne(const AddonToDelete& addon, const std::vector<LinksNow>& seen, DeletionRoute route) const;

    const FilesystemProbe& filesystemProbe_;
    FileOperations& files_;
    SidecarStore& sidecars_;
    const LinkingEngine& linking_;
    const EntryClassifier& classifier_;
    const ProcessProbe& processProbe_;
    const OperationLog& log_;
    const SizeService& sizes_;
};

#endif // FS_ORGANIZER_APPLICATION_DELETION_SERVICE_H
