#ifndef FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H
#define FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H

enum class OperationKind : int
{
    EnableAddon = 0,
    DisableAddon = 1,
    RemoveBrokenLink = 2,
    RepointLink = 3,
};

#endif // FS_ORGANIZER_DOMAIN_MODEL_OPERATION_KIND_H
