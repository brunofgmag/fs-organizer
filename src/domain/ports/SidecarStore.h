#ifndef FS_ORGANIZER_DOMAIN_PORTS_SIDECAR_STORE_H
#define FS_ORGANIZER_DOMAIN_PORTS_SIDECAR_STORE_H

#include <filesystem>
#include <optional>
#include <string>

class SidecarStore
{
public:
    virtual ~SidecarStore() = default;

    [[nodiscard]] virtual bool Write(const std::filesystem::path& path, const std::string& contents) = 0;

    [[nodiscard]] virtual std::optional<std::string> Read(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual bool Forget(const std::filesystem::path& path) = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_SIDECAR_STORE_H
