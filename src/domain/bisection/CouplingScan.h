#ifndef FS_ORGANIZER_DOMAIN_BISECTION_COUPLING_SCAN_H
#define FS_ORGANIZER_DOMAIN_BISECTION_COUPLING_SCAN_H

#include <filesystem>
#include <vector>

#include "domain/bisection/CoupledUnits.h"
#include "domain/ports/FilesystemProbe.h"

class CouplingScan
{
public:
    explicit CouplingScan(const FilesystemProbe& filesystemProbe);

    [[nodiscard]] std::vector<CouplingFacts> FactsAbout(const std::vector<std::filesystem::path>& addonFolders) const;

    [[nodiscard]] std::vector<SearchUnit> WithTheKindOfEachGroup(std::vector<SearchUnit> units) const;

private:
    [[nodiscard]] Coupling TheKindOf(const SearchUnit& unit) const;

    [[nodiscard]] bool SomeFileIsClaimedTwice(const SearchUnit& unit) const;

    [[nodiscard]] CouplingFacts FactsAboutOne(const std::filesystem::path& addonFolder) const;

    void ReadTheLiveriesUnder(const std::filesystem::path& modelFolder, CouplingFacts& facts) const;

    const FilesystemProbe& filesystemProbe_;
};

#endif // FS_ORGANIZER_DOMAIN_BISECTION_COUPLING_SCAN_H
