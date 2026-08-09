#ifndef FS_ORGANIZER_APPLICATION_MODEL_INTERRUPTED_SWAP_H
#define FS_ORGANIZER_APPLICATION_MODEL_INTERRUPTED_SWAP_H

#include <filesystem>

struct InterruptedSwap
{
    std::filesystem::path room{};
    std::filesystem::path folder{};
    std::filesystem::path libraryCopy{};
};

#endif // FS_ORGANIZER_APPLICATION_MODEL_INTERRUPTED_SWAP_H
