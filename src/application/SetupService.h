#ifndef FS_ORGANIZER_APPLICATION_SETUP_SERVICE_H
#define FS_ORGANIZER_APPLICATION_SETUP_SERVICE_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "application/model/DestinationCheck.h"
#include "application/model/LibraryReport.h"
#include "application/model/RegisteredLibrary.h"
#include "application/ports/LibraryIdGenerator.h"
#include "application/ports/SimulatorLocator.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/FilesystemProbe.h"

using KeepTheProfile = std::function<bool(const SimulatorProfile&)>;

class SetupService
{
public:
    SetupService(const SimulatorLocator& locator,
                 const FilesystemProbe& filesystemProbe,
                 const LibraryIdGenerator& identities,
                 const CatalogScanner& catalog,
                 std::vector<SimulatorProfile> existing,
                 KeepTheProfile keep);

    void Detect();

    [[nodiscard]] const std::vector<SimulatorCandidate>& Candidates() const;

    [[nodiscard]] DestinationCheck CheckDestination(const std::filesystem::path& path) const;

    void AddManualCandidate(const std::filesystem::path& destination, SimulatorVariant variant);

    void ChooseCandidate(std::size_t index);

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path, const std::string& label);

    [[nodiscard]] const std::vector<RegisteredLibrary>& Libraries() const;

    [[nodiscard]] bool Complete() const;

private:
    const SimulatorLocator& locator_;
    const FilesystemProbe& filesystemProbe_;
    const LibraryIdGenerator& identities_;
    const CatalogScanner& catalog_;
    std::vector<SimulatorProfile> existing_;
    KeepTheProfile keep_;
    std::vector<SimulatorCandidate> candidates_;
    std::vector<RegisteredLibrary> libraries_;
    std::size_t chosen_ = 0;
};

#endif // FS_ORGANIZER_APPLICATION_SETUP_SERVICE_H
