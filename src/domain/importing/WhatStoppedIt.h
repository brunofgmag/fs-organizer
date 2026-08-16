#ifndef FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_STOPPED_IT_H
#define FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_STOPPED_IT_H

#include <filesystem>

#include "domain/model/FileResult.h"
#include "domain/ports/FilesystemProbe.h"

[[nodiscard]] inline FileResult
WhatStoppedIt(const FilesystemProbe& filesystemProbe, const std::filesystem::path& folder, const FileResult otherwise)
{
    return filesystemProbe.SomethingIsHoldingItOpen(folder) ? FileResult::AnotherProgramIsHoldingIt : otherwise;
}

#endif // FS_ORGANIZER_DOMAIN_IMPORTING_WHAT_STOPPED_IT_H
