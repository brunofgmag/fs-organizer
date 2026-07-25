#ifndef FS_ORGANIZER_APPLICATION_ADDON_SERVICE_H
#define FS_ORGANIZER_APPLICATION_ADDON_SERVICE_H

#include <map>
#include <string>
#include <vector>

#include "application/model/LibraryReport.h"
#include "application/model/LinkOperationResult.h"
#include "application/model/TreeSnapshot.h"
#include "application/ports/Clock.h"
#include "application/ports/LibraryIdGenerator.h"
#include "application/ports/OperationJournal.h"
#include "domain/linking/EnabledStateResolver.h"
#include "domain/linking/LinkingEngine.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/CatalogScanner.h"

class AddonService
{
public:
    AddonService(const CatalogScanner& catalog,
                 const EnabledStateResolver& resolver,
                 const LinkingEngine& linking,
                 OperationJournal& journal,
                 const Clock& clock,
                 const LibraryIdGenerator& identities,
                 LinkType linkType);

    [[nodiscard]] TreeSnapshot Scan(const SimulatorProfile& profile) const;

    [[nodiscard]] LibraryReport RegisterLibrary(SimulatorProfile& profile,
                                                const std::filesystem::path& path) const;

    [[nodiscard]] std::vector<DestinationEntry> ResolveEntries(const SimulatorProfile& profile) const;

    [[nodiscard]] std::vector<LinkOperationResult> SetEnabled(const SimulatorProfile& profile,
                                                              const TreeSnapshot& snapshot,
                                                              const std::vector<const TreeNode*>& nodes,
                                                              bool enable);

    [[nodiscard]] bool CanUndo() const;

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
                                                     const TreeSnapshot& snapshot,
                                                     const std::vector<const TreeNode*>& nodes,
                                                     bool enable);

    [[nodiscard]] static std::vector<Step> StepsFor(
        const SimulatorProfile& profile,
        const std::multimap<std::string, const DestinationEntry*>& linksByTarget,
        const TreeNode& addon,
        bool enable);

    [[nodiscard]] static Step Inverse(const Step& step);

    [[nodiscard]] LinkOperationResult Run(const Step& step) const;

    const CatalogScanner& catalog_;
    const EnabledStateResolver& resolver_;
    const LinkingEngine& linking_;
    OperationJournal& journal_;
    const Clock& clock_;
    const LibraryIdGenerator& identities_;
    LinkType linkType_;
    std::vector<Step> undo_;
};

#endif // FS_ORGANIZER_APPLICATION_ADDON_SERVICE_H
