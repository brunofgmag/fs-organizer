#ifndef FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_BIN_ROOM_H
#define FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_BIN_ROOM_H

#include <cstdint>

struct RecycleBinRoom
{
    std::uintmax_t quota = 0;
    bool itRecycles = true;
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_RECYCLE_BIN_ROOM_H
