#include "infrastructure/fileops/PosixFileOperations.h"

CopyOutcome PosixFileOperations::CopyTree(const std::filesystem::path&,
                                          const std::filesystem::path&,
                                          const std::function<bool(const CopyProgress&)>&)
{
    return CopyOutcome::Failed;
}

bool PosixFileOperations::CreateFolder(const std::filesystem::path&)
{
    return false;
}

bool PosixFileOperations::WriteHiddenFile(const std::filesystem::path&)
{
    return false;
}

bool PosixFileOperations::Move(const std::filesystem::path&, const std::filesystem::path&)
{
    return false;
}

bool PosixFileOperations::RemoveTree(const std::filesystem::path&)
{
    return false;
}

bool PosixFileOperations::Recycle(const std::filesystem::path&)
{
    return false;
}

bool PosixFileOperations::RemoveEmptyFolder(const std::filesystem::path&)
{
    return false;
}
