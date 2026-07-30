#ifndef FS_ORGANIZER_APPLICATION_SETUP_SERVICE_H
#define FS_ORGANIZER_APPLICATION_SETUP_SERVICE_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "application/model/DestinationCheck.h"
#include "application/model/LibraryReport.h"
#include "application/model/RegisteredLibrary.h"
#include "application/ports/LibraryIdGenerator.h"
#include "application/ports/SettingsRepository.h"
#include "application/ports/SimulatorLocator.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/FilesystemProbe.h"

class SetupService
{
public:
    SetupService(const SimulatorLocator& locator,
                 const FilesystemProbe& filesystemProbe,
                 SettingsRepository& settings,
                 const LibraryIdGenerator& identities,
                 const CatalogScanner& catalog);

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
    SettingsRepository& settings_;
    const LibraryIdGenerator& identities_;
    const CatalogScanner& catalog_;
    std::vector<SimulatorCandidate> candidates_;
    std::vector<RegisteredLibrary> libraries_;
    std::size_t chosen_ = 0;
};

#endif // FS_ORGANIZER_APPLICATION_SETUP_SERVICE_H
