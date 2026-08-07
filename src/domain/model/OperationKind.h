#ifndef FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H
#define FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H

#include <array>
#include <cstddef>

enum class OperationKind : int
{
    EnableAddon = 0,
    DisableAddon = 1,
    RemoveBrokenLink = 2,
    RepointLink = 3,
    ImportCopyToStaging = 4,
    ImportVerifyStaging = 9,
    ImportMoveIntoPlace = 5,
    ImportRemoveSource = 6,
    QuarantineFromDestination = 7,
    QuarantineFromLibrary = 8,
    RestoreFromQuarantine = 10,
    DiscardFromQuarantine = 11,
    DiscardStaging = 12,
    MoveAddon = 13,
    CreateCategory = 14,
    RenameCategory = 15,
    RemoveCategory = 16,
    RecycleFromLibrary = 17,
    DeleteFromLibrary = 18,
};

inline constexpr std::array kAllOperationKinds{
    OperationKind::EnableAddon,
    OperationKind::DisableAddon,
    OperationKind::RemoveBrokenLink,
    OperationKind::RepointLink,
    OperationKind::ImportCopyToStaging,
    OperationKind::ImportVerifyStaging,
    OperationKind::ImportMoveIntoPlace,
    OperationKind::ImportRemoveSource,
    OperationKind::QuarantineFromDestination,
    OperationKind::QuarantineFromLibrary,
    OperationKind::RestoreFromQuarantine,
    OperationKind::DiscardFromQuarantine,
    OperationKind::DiscardStaging,
    OperationKind::MoveAddon,
    OperationKind::CreateCategory,
    OperationKind::RenameCategory,
    OperationKind::RemoveCategory,
    OperationKind::RecycleFromLibrary,
    OperationKind::DeleteFromLibrary,
};

static_assert(kAllOperationKinds.size() == static_cast<std::size_t>(OperationKind::DeleteFromLibrary) + 1,
              "Every OperationKind belongs in kAllOperationKinds, and the last one carries the highest value.");

[[nodiscard]] constexpr bool CarriesAFileReason(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::ImportCopyToStaging:
    case OperationKind::ImportVerifyStaging:
    case OperationKind::ImportMoveIntoPlace:
    case OperationKind::ImportRemoveSource:
    case OperationKind::QuarantineFromDestination:
    case OperationKind::QuarantineFromLibrary:
    case OperationKind::RestoreFromQuarantine:
    case OperationKind::DiscardFromQuarantine:
    case OperationKind::DiscardStaging:
    case OperationKind::MoveAddon:
    case OperationKind::CreateCategory:
    case OperationKind::RenameCategory:
    case OperationKind::RemoveCategory:
    case OperationKind::RecycleFromLibrary:
    case OperationKind::DeleteFromLibrary: return true;
    case OperationKind::EnableAddon:
    case OperationKind::DisableAddon:
    case OperationKind::RemoveBrokenLink:
    case OperationKind::RepointLink: return false;
    }

    return false;
}

[[nodiscard]] constexpr bool CreatesALink(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::EnableAddon:
    case OperationKind::RepointLink: return true;
    case OperationKind::DisableAddon:
    case OperationKind::RemoveBrokenLink:
    case OperationKind::ImportCopyToStaging:
    case OperationKind::ImportVerifyStaging:
    case OperationKind::ImportMoveIntoPlace:
    case OperationKind::ImportRemoveSource:
    case OperationKind::QuarantineFromDestination:
    case OperationKind::QuarantineFromLibrary:
    case OperationKind::RestoreFromQuarantine:
    case OperationKind::DiscardFromQuarantine:
    case OperationKind::DiscardStaging:
    case OperationKind::MoveAddon:
    case OperationKind::CreateCategory:
    case OperationKind::RenameCategory:
    case OperationKind::RemoveCategory:
    case OperationKind::RecycleFromLibrary:
    case OperationKind::DeleteFromLibrary: return false;
    }

    return false;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H
