#include "infrastructure/fileops/WindowsFileOperations.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <system_error>
#include <vector>

namespace
{
    std::wstring NativePath(const std::filesystem::path& path)
    {
        std::filesystem::path native = path.lexically_normal();
        native.make_preferred();
        return native.wstring();
    }

    DWORD AttributesWithoutFollowingLinks(const std::filesystem::path& path)
    {
        return GetFileAttributesW(NativePath(path).c_str());
    }

    bool IsReparsePoint(const DWORD attributes)
    {
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    bool IsDirectory(const DWORD attributes)
    {
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    struct Walk
    {
        std::vector<std::filesystem::path> files;
        std::uintmax_t totalBytes = 0;
    };

    Walk FilesUnder(const std::filesystem::path& root)
    {
        Walk walk;

        std::error_code error;
        const std::filesystem::recursive_directory_iterator entries(
            root, std::filesystem::directory_options::skip_permission_denied, error);

        for (const std::filesystem::directory_entry& entry : entries)
        {
            if (!entry.is_regular_file(error))
            {
                continue;
            }

            walk.files.push_back(entry.path());
            walk.totalBytes += entry.file_size(error);
        }

        return walk;
    }

    struct CopyState
    {
        const std::function<bool(const CopyProgress &)>* onProgress = nullptr;
        std::uintmax_t copiedBefore = 0;
        std::uintmax_t totalBytes = 0;
        bool cancelled = false;
    };

    DWORD CALLBACK ReportProgress(const LARGE_INTEGER,
                                  const LARGE_INTEGER transferred,
                                  const LARGE_INTEGER,
                                  const LARGE_INTEGER,
                                  const DWORD,
                                  const DWORD,
                                  const HANDLE,
                                  const HANDLE,
                                  const LPVOID data)
    {
        auto* state = static_cast<CopyState*>(data);
        if (state->onProgress == nullptr || !*state->onProgress)
        {
            return PROGRESS_CONTINUE;
        }

        const CopyProgress progress{
            state->copiedBefore + static_cast<std::uintmax_t>(transferred.QuadPart),
            state->totalBytes
        };

        if (!(*state->onProgress)(progress))
        {
            state->cancelled = true;
            return PROGRESS_CANCEL;
        }

        return PROGRESS_CONTINUE;
    }
}

CopyOutcome WindowsFileOperations::CopyTree(const std::filesystem::path& source,
                                            const std::filesystem::path& destination,
                                            const std::function<bool(const CopyProgress &)>& onProgress)
{
    const Walk walk = FilesUnder(source);

    std::error_code error;
    std::filesystem::create_directories(destination, error);
    if (error)
    {
        return CopyOutcome::Failed;
    }

    CopyState state{&onProgress, 0, walk.totalBytes, false};

    for (const std::filesystem::path& file : walk.files)
    {
        const std::filesystem::path landing = destination / file.lexically_relative(source);

        std::filesystem::create_directories(landing.parent_path(), error);
        if (error)
        {
            return CopyOutcome::Failed;
        }

        BOOL cancelFlag = FALSE;
        if (CopyFileExW(NativePath(file).c_str(), NativePath(landing).c_str(), ReportProgress,
                        &state, &cancelFlag, COPY_FILE_FAIL_IF_EXISTS)
            == FALSE)
        {
            return state.cancelled ? CopyOutcome::Cancelled : CopyOutcome::Failed;
        }

        state.copiedBefore += std::filesystem::file_size(file, error);
    }

    return CopyOutcome::Completed;
}

bool WindowsFileOperations::Move(const std::filesystem::path& source,
                                 const std::filesystem::path& destination)
{
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
    {
        return false;
    }

    return MoveFileExW(NativePath(source).c_str(), NativePath(destination).c_str(), 0) != FALSE;
}

bool WindowsFileOperations::RemoveTree(const std::filesystem::path& path)
{
    const DWORD attributes = AttributesWithoutFollowingLinks(path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    if (IsReparsePoint(attributes))
    {
        return RemoveDirectoryW(NativePath(path).c_str()) != FALSE;
    }

    if (!IsDirectory(attributes))
    {
        return DeleteFileW(NativePath(path).c_str()) != FALSE;
    }

    std::error_code error;
    const std::filesystem::directory_iterator children(
        path, std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return false;
    }

    for (const std::filesystem::directory_entry& child : children)
    {
        if (!RemoveTree(child.path()))
        {
            return false;
        }
    }

    return RemoveDirectoryW(NativePath(path).c_str()) != FALSE;
}
