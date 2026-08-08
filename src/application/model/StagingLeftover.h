#ifndef FS_ORGANIZER_APPLICATION_MODEL_STAGING_LEFTOVER_H
#define FS_ORGANIZER_APPLICATION_MODEL_STAGING_LEFTOVER_H

#include <filesystem>

struct StagingLeftover
{
    std::filesystem::path staging{};
    std::filesystem::path target{};
    std::filesystem::path source{};
    std::filesystem::path externalSource{};

    [[nodiscard]] bool CanBeResumed() const
    {
        return !source.empty();
    }
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_STAGING_LEFTOVER_H
