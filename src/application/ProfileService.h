#ifndef FS_ORGANIZER_APPLICATION_PROFILE_SERVICE_H
#define FS_ORGANIZER_APPLICATION_PROFILE_SERVICE_H

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "application/model/LibraryReport.h"
#include "application/model/LinkOperationResult.h"
#include "application/model/ProfileSnapshot.h"
#include "application/ports/LibraryIdGenerator.h"
#include "domain/journal/OperationLog.h"
#include "domain/linking/EntryClassifier.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/linking/RepairPlan.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/Clock.h"
#include "domain/ports/OperationJournal.h"

class ProfileService
{
public:
    ProfileService(const CatalogScanner& catalog,
                   const EntryClassifier& classifier,
                   const LinkingEngine& linking,
                   const OperationLog& log,
                   const LibraryIdGenerator& identities,
                   LinkType linkType);

    [[nodiscard]] ProfileSnapshot Scan(const SimulatorProfile& profile) const;

    [[nodiscard]] LibraryReport RegisterLibrary(SimulatorProfile& profile, const std::filesystem::path& path) const;

    [[nodiscard]] std::vector<DestinationEntry> ResolveEntries(const SimulatorProfile& profile) const;

    [[nodiscard]] std::vector<LinkOperationResult> SetEnabled(const SimulatorProfile& profile,
                                                              const ProfileSnapshot& snapshot,
                                                              const std::vector<const TreeNode*>& nodes,
                                                              bool enable);

    [[nodiscard]] std::vector<LinkOperationResult> Repair(const SimulatorProfile& profile,
                                                          const std::vector<RepairRequest>& requests);

    [[nodiscard]] bool CanUndo() const;

    void ForgetUndo();

    [[nodiscard]] std::vector<LinkOperationResult> UndoLastBatch();

private:
    struct Step
    {
        OperationKind kind = OperationKind::EnableAddon;
        AddonId addonId;
        std::filesystem::path addonFolder;
        std::filesystem::path linkPath;
    };

    [[nodiscard]] static std::vector<Step> PlanSteps(const SimulatorProfile& profile,
                                                     const ProfileSnapshot& snapshot,
                                                     const std::vector<const TreeNode*>& nodes,
                                                     bool enable);

    [[nodiscard]] static std::vector<Step>
    StepsFor(const SimulatorProfile& profile,
             const std::multimap<std::string, const DestinationEntry*>& linksByTarget,
             const TreeNode& addon,
             bool enable);

    [[nodiscard]] static Step Inverse(const Step& step);

    [[nodiscard]] static std::optional<Step> PlanRepair(const SimulatorProfile& profile, const RepairRequest& request);

    [[nodiscard]] static std::vector<Step> Inverse(const SimulatorProfile& profile, const RepairRequest& request);

    [[nodiscard]] LinkOperationResult Run(const Step& step) const;

    const CatalogScanner& catalog_;
    const EntryClassifier& classifier_;
    const LinkingEngine& linking_;
    const OperationLog& log_;
    const LibraryIdGenerator& identities_;
    LinkType linkType_;
    std::vector<Step> undo_;
};

#endif // FS_ORGANIZER_APPLICATION_PROFILE_SERVICE_H
