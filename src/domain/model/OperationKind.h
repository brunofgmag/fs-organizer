#ifndef FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H
#define FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H

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
};

[[nodiscard]] constexpr bool CarriesAnImportReason(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::ImportCopyToStaging:
    case OperationKind::ImportVerifyStaging:
    case OperationKind::ImportMoveIntoPlace:
    case OperationKind::ImportRemoveSource:
    case OperationKind::QuarantineFromDestination:
    case OperationKind::QuarantineFromLibrary:
        return true;
    case OperationKind::EnableAddon:
    case OperationKind::DisableAddon:
    case OperationKind::RemoveBrokenLink:
    case OperationKind::RepointLink:
        return false;
    }

    return false;
}

#endif // FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H
