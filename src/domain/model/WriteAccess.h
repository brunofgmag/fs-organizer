#ifndef FS_ORGANIZER_DOMAIN_MODEL_WRITE_ACCESS_H
#define FS_ORGANIZER_DOMAIN_MODEL_WRITE_ACCESS_H

#include <array>
#include <cstddef>

enum class WriteAccess : int
{
    ItAccepts = 0,
    TheFolderIsNotThere = 1,
    PermissionIsDenied = 2,
    TheVolumeIsReadOnly = 3,
    ItRefusedForAnotherReason = 4,
};

inline constexpr std::array kAllWriteAccesses{
    WriteAccess::ItAccepts,           WriteAccess::TheFolderIsNotThere,       WriteAccess::PermissionIsDenied,
    WriteAccess::TheVolumeIsReadOnly, WriteAccess::ItRefusedForAnotherReason,
};

static_assert(kAllWriteAccesses.size() == static_cast<std::size_t>(WriteAccess::ItRefusedForAnotherReason) + 1,
              "Every WriteAccess belongs in kAllWriteAccesses, and the last one carries the highest value.");

[[nodiscard]] constexpr bool ItAcceptsWrites(const WriteAccess access)
{
    return access == WriteAccess::ItAccepts;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_WRITE_ACCESS_H
