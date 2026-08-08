#include "infrastructure/fileops/WindowsFileOperations.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <shellapi.h>

#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "domain/model/RecycleLimits.h"
#include "infrastructure/fileops/ExtendedPaths.h"

namespace
{
    std::wstring NativePath(const std::filesystem::path& path)
    {
        return WithExtendedPrefix(path).wstring();
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

    struct WalkedFile
    {
        std::filesystem::path path;
        std::uintmax_t bytes = 0;
    };

    struct Walk
    {
        std::vector<WalkedFile> files;
        std::uintmax_t totalBytes = 0;
    };

    std::optional<Walk> FilesUnder(const std::filesystem::path& root)
    {
        std::error_code error;
        std::filesystem::recursive_directory_iterator entry(
            root, std::filesystem::directory_options::skip_permission_denied, error);
        if (error)
        {
            return std::nullopt;
        }

        Walk walk;
        const std::filesystem::recursive_directory_iterator end;

        while (entry != end)
        {
            const bool isFile = entry->is_regular_file(error);
            if (error)
            {
                return std::nullopt;
            }

            if (isFile)
            {
                const std::uintmax_t bytes = entry->file_size(error);
                if (error)
                {
                    return std::nullopt;
                }

                walk.files.push_back({.path = entry->path(), .bytes = bytes});
                walk.totalBytes += bytes;
            }

            entry.increment(error);
            if (error)
            {
                return std::nullopt;
            }
        }

        return walk;
    }

    struct CopyState
    {
        const std::function<bool(const CopyProgress&)>* onProgress = nullptr;
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

        const CopyProgress progress{.copiedBytes =
                                        state->copiedBefore + static_cast<std::uintmax_t>(transferred.QuadPart),
                                    .totalBytes = state->totalBytes};

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
                                            const std::function<bool(const CopyProgress&)>& onProgress)
{
    const std::filesystem::path reachableSource = WithExtendedPrefix(source);
    const std::filesystem::path reachableDestination = WithExtendedPrefix(destination);

    const std::optional<Walk> walk = FilesUnder(reachableSource);
    if (!walk.has_value())
    {
        return CopyOutcome::Failed;
    }

    std::error_code error;
    std::filesystem::create_directories(reachableDestination, error);
    if (error)
    {
        return CopyOutcome::Failed;
    }

    CopyState state{.onProgress = &onProgress, .copiedBefore = 0, .totalBytes = walk->totalBytes, .cancelled = false};

    for (const WalkedFile& file : walk->files)
    {
        const std::filesystem::path landing = reachableDestination / file.path.lexically_relative(reachableSource);

        std::filesystem::create_directories(landing.parent_path(), error);
        if (error)
        {
            return CopyOutcome::Failed;
        }

        BOOL cancelFlag = FALSE;
        if (CopyFileExW(NativePath(file.path).c_str(), NativePath(landing).c_str(), ReportProgress, &state, &cancelFlag,
                        COPY_FILE_FAIL_IF_EXISTS)
            == FALSE)
        {
            return state.cancelled ? CopyOutcome::Cancelled : CopyOutcome::Failed;
        }

        state.copiedBefore += file.bytes;
    }

    return CopyOutcome::Completed;
}

bool WindowsFileOperations::CreateFolder(const std::filesystem::path& path)
{
    if (AttributesWithoutFollowingLinks(path) != INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(WithExtendedPrefix(path), error);

    return !error;
}

bool WindowsFileOperations::WriteHiddenFile(const std::filesystem::path& path)
{
    const HANDLE file =
        CreateFileW(NativePath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);

    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    return CloseHandle(file) != FALSE;
}

bool WindowsFileOperations::WriteTextFile(const std::filesystem::path& path, const std::string& contents)
{
    const HANDLE file =
        CreateFileW(NativePath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD written = 0;
    const bool wrote =
        WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) != FALSE;
    const bool closed = CloseHandle(file) != FALSE;

    return closed && wrote && static_cast<std::size_t>(written) == contents.size();
}

bool WindowsFileOperations::Move(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    std::filesystem::create_directories(WithExtendedPrefix(destination).parent_path(), error);
    if (error)
    {
        return false;
    }

    return MoveFileExW(NativePath(source).c_str(), NativePath(destination).c_str(), 0) != FALSE;
}

bool WindowsFileOperations::RemoveEmptyFolder(const std::filesystem::path& path)
{
    const DWORD attributes = AttributesWithoutFollowingLinks(path);

    if (!IsDirectory(attributes) || IsReparsePoint(attributes))
    {
        return false;
    }

    return RemoveDirectoryW(NativePath(path).c_str()) != FALSE;
}

bool WindowsFileOperations::Recycle(const std::filesystem::path& path)
{
    if (AttributesWithoutFollowingLinks(path) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    if (!TheRecycleBinReaches(LongestEntryUnder(path)))
    {
        return false;
    }

    std::wstring called = WithoutExtendedPrefix(path).wstring();
    called.push_back(L'\0');
    called.push_back(L'\0');

    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = called.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    return SHFileOperationW(&operation) == 0 && operation.fAnyOperationsAborted == FALSE;
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
    std::filesystem::directory_iterator child(WithExtendedPrefix(path),
                                              std::filesystem::directory_options::skip_permission_denied, error);
    if (error)
    {
        return false;
    }

    const std::filesystem::directory_iterator end;

    while (child != end)
    {
        if (!RemoveTree(child->path()))
        {
            return false;
        }

        child.increment(error);
        if (error)
        {
            return false;
        }
    }

    return RemoveDirectoryW(NativePath(path).c_str()) != FALSE;
}
