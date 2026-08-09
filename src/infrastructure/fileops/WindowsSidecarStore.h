#ifndef FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_SIDECAR_STORE_H
#define FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_SIDECAR_STORE_H

#include "domain/ports/SidecarStore.h"

class WindowsSidecarStore final : public SidecarStore
{
public:
    [[nodiscard]] bool Write(const std::filesystem::path& path, const std::string& contents) override;

    [[nodiscard]] std::optional<std::string> Read(const std::filesystem::path& path) const override;

    [[nodiscard]] bool Forget(const std::filesystem::path& path) override;
};

#endif // FS_ORGANIZER_INFRASTRUCTURE_FILEOPS_WINDOWS_SIDECAR_STORE_H
