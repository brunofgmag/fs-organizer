#ifndef FS_ORGANIZER_INFRASTRUCTURE_BISECTION_JSON_BISECTION_STORE_H
#define FS_ORGANIZER_INFRASTRUCTURE_BISECTION_JSON_BISECTION_STORE_H

#include <filesystem>

#include "application/ports/BisectionStore.h"

class JsonBisectionStore final : public BisectionStore
{
public:
    explicit JsonBisectionStore(std::filesystem::path root);

    [[nodiscard]] std::optional<BisectionRun> Load(const std::string& profileId) const override;

    [[nodiscard]] bool Save(const std::string& profileId, const BisectionRun& run) override;

    void Forget(const std::string& profileId) override;

    [[nodiscard]] std::optional<std::filesystem::path> FileOf(const std::string& profileId) const;

private:
    std::filesystem::path root_;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_BISECTION_JSON_BISECTION_STORE_H
