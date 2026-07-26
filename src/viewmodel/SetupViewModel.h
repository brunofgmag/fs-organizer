#ifndef FS_ORGANIZER_VIEWMODEL_SETUP_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_SETUP_VIEW_MODEL_H

#include <vector>

#include <QtCore/QObject>

#include "application/ports/LibraryIdGenerator.h"
#include "application/ports/SettingsRepository.h"
#include "domain/model/Library.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/FilesystemProbe.h"
#include "domain/ports/SimulatorLocator.h"
#include "viewmodel/DestinationCheck.h"
#include "application/model/LibraryReport.h"
#include "viewmodel/RegisteredLibrary.h"

class SetupViewModel final : public QObject
{
    Q_OBJECT

public:
    SetupViewModel(const SimulatorLocator& locator,
                   const FilesystemProbe& filesystemProbe,
                   SettingsRepository& settings,
                   const LibraryIdGenerator& identities,
                   const CatalogScanner& catalog,
                   QObject* parent = nullptr);

    void Detect();

    [[nodiscard]] std::vector<SimulatorCandidate> Candidates() const;

    [[nodiscard]] DestinationCheck CheckDestination(const std::filesystem::path& path) const;

    void AddManualCandidate(const std::filesystem::path& destination, SimulatorVariant variant);

    void ChooseCandidate(std::size_t index);

    [[nodiscard]] LibraryReport RegisterLibrary(const std::filesystem::path& path, const std::string& label);

    [[nodiscard]] std::vector<RegisteredLibrary> Libraries() const;

    void Complete() const;

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

#endif // FS_ORGANIZER_VIEWMODEL_SETUP_VIEW_MODEL_H
