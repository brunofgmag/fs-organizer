#ifndef FS_ORGANIZER_APPLICATION_SIZE_SERVICE_H
#define FS_ORGANIZER_APPLICATION_SIZE_SERVICE_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "application/model/SizeReport.h"
#include "application/ports/BackgroundRunner.h"
#include "domain/ports/CatalogScanner.h"
#include "domain/ports/Clock.h"
#include "domain/ports/FilesystemProbe.h"

enum class Freshness : int
{
    ReuseWhatIsKnown = 0,
    MeasureAgain = 1,
};

struct MeasurementCaller
{
    int id = 0;
};

class SizeService
{
public:
    SizeService(const CatalogScanner& catalog,
                const FilesystemProbe& filesystemProbe,
                const Clock& clock,
                BackgroundRunner& runner);

    [[nodiscard]] MeasurementCaller NewCaller();

    void Measure(const std::vector<std::filesystem::path>& libraryRoots,
                 MeasurementCaller caller,
                 Freshness freshness,
                 std::function<bool(const SizeProgress&)> onProgress,
                 std::function<void(const SizeReport&)> onMeasured);

    [[nodiscard]] std::optional<std::uintmax_t> BytesOf(const std::filesystem::path& folder) const;

private:
    [[nodiscard]] SizeReport MeasureLibraries(const std::vector<std::filesystem::path>& libraryRoots,
                                              const std::map<std::string, std::uintmax_t>& known,
                                              std::map<std::string, std::uintmax_t>& fresh,
                                              const std::function<bool(const SizeProgress&)>& onProgress) const;

    void Adopt(MeasurementCaller caller,
               int request,
               const std::map<std::string, std::uintmax_t>& fresh,
               SizeReport& report,
               const std::function<void(const SizeReport&)>& onMeasured);

    const CatalogScanner& catalog_;
    const FilesystemProbe& filesystemProbe_;
    const Clock& clock_;
    BackgroundRunner& runner_;
    std::map<std::string, std::uintmax_t> bytes_;
    std::map<int, int> asked_;
    int callers_ = 0;
};

#endif // FS_ORGANIZER_APPLICATION_SIZE_SERVICE_H
