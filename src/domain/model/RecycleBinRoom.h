#ifndef FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_BIN_ROOM_H
#define FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_BIN_ROOM_H

#include <cstdint>
#include <optional>

struct RecycleBinRoom
{
    std::optional<std::uintmax_t> quota{};
    bool itRecycles = true;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_BIN_ROOM_H
