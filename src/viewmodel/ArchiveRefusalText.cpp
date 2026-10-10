#include "viewmodel/ArchiveRefusalText.h"

#include <QtCore/QObject>

#include "viewmodel/FailureText.h"

namespace
{
    [[nodiscard]] bool IsOfAFormatWindows10DoesNotRead(const QString& fileName)
    {
        return fileName.endsWith(QStringLiteral(".7z"), Qt::CaseInsensitive)
            || fileName.endsWith(QStringLiteral(".rar"), Qt::CaseInsensitive);
    }

    [[nodiscard]] bool IsAFolder(const SourceKind source)
    {
        return source == SourceKind::Folder;
    }

    [[nodiscard]] QString WhyItCouldNotBeOpened(const QString& fileName, const SourceKind source)
    {
        if (IsAFolder(source))
        {
            return QObject::tr("FS Organizer could not read this folder. It is a link, a drive root, or a folder it "
                               "cannot open.");
        }

        if (IsOfAFormatWindows10DoesNotRead(fileName))
        {
            return QObject::tr("Windows could not open it: it is damaged, or this Windows does not read it. Windows 10 "
                               "does not read .7z or older .rar.");
        }

        return QObject::tr("Windows could not open it: it is damaged or is not an archive.");
    }

    [[nodiscard]] QString ItNeedsAPassword()
    {
        return QObject::tr("It needs a password, and FS Organizer does not open archives that have one.");
    }

    [[nodiscard]] QString YouCancelledIt()
    {
        return QObject::tr("You cancelled, so nothing was installed.");
    }

    [[nodiscard]] QString TheCopyOrExtractionFailed(const SourceKind source)
    {
        return IsAFolder(source) ? QObject::tr("Copying the files failed. Nothing was installed.")
                                 : QObject::tr("Extracting the files failed. Nothing was installed.");
    }

    [[nodiscard]] QString ItTookTooLong(const SourceKind source)
    {
        return IsAFolder(source)
            ? QObject::tr("FS Organizer stopped reading or copying it because it took too long.")
            : QObject::tr("FS Organizer stopped reading or extracting it because it took too long.");
    }

    [[nodiscard]] QString ThereIsNoRoomFor(const SourceKind source)
    {
        return IsAFolder(source) ? QObject::tr("There is not enough free space in the library to copy it.")
                                 : QObject::tr("There is not enough free space in the library to extract it.");
    }

    [[nodiscard]] QString TheCopyDoesNotMatch(const SourceKind source)
    {
        return IsAFolder(source)
            ? QObject::tr("The copied files do not match what was in the folder. Nothing was installed.")
            : QObject::tr("The extracted files do not match the archive's contents. Nothing was installed.");
    }

    [[nodiscard]] QString TheDiskChanged(const SourceKind source)
    {
        return IsAFolder(source)
            ? QObject::tr("The library or the folder changed after FS Organizer read them. Nothing was installed. "
                          "Drop the folder again to read it again.")
            : QObject::tr("The library or the archive changed after FS Organizer read them. Nothing was installed. "
                          "Drop the archive again to read it again.");
    }

    [[nodiscard]] QString NothingSafeToExtractOrCopy(const SourceKind source)
    {
        return IsAFolder(source)
            ? QObject::tr("It holds something unsafe to copy: a link or junction, or a name Windows cannot write.")
            : QObject::tr("It holds an entry that is not safe to extract: a link, an absolute path, .. or a name "
                          "Windows cannot write.");
    }
}

QString WhyTheArchiveIsRefused(const ArchiveFailure failure, const QString& fileName, const SourceKind source)
{
    switch (failure)
    {
    case ArchiveFailure::None: return {};
    case ArchiveFailure::CouldNotOpen: return WhyItCouldNotBeOpened(fileName, source);
    case ArchiveFailure::NeedsAPassword: return ItNeedsAPassword();
    case ArchiveFailure::CouldNotExtract: return TheCopyOrExtractionFailed(source);
    case ArchiveFailure::TookTooLong: return ItTookTooLong(source);
    case ArchiveFailure::TheToolIsMissing:
        return QObject::tr("This Windows does not have tar.exe, which FS Organizer uses to open archives.");
    case ArchiveFailure::Cancelled: return YouCancelledIt();
    }

    return {};
}

QString WhyTheArchiveIsRefused(const NothingToInstall refusal, const SourceKind source)
{
    switch (refusal)
    {
    case NothingToInstall::None: return {};
    case NothingToInstall::NoPackage: return QObject::tr("It holds no addon: no folder in it has a manifest.json.");
    case NothingToInstall::OnlyAnInstaller:
        return QObject::tr("It holds an installer (.exe or .msi) and no addon. FS Organizer never runs an installer.");
    case NothingToInstall::AnEntryIsNotSafe: return NothingSafeToExtractOrCopy(source);
    case NothingToInstall::ThePackageHasNoFolder:
        return QObject::tr("The addon sits loose at the top of the archive, without a folder of its own to be "
                           "installed under.");
    case NothingToInstall::TwoPackagesShareAName:
        return QObject::tr("Two addons in it have the same folder name, and a library holds only one of each.");
    case NothingToInstall::ThePackageNameIsReserved:
        return QObject::tr("An addon in it has a folder name that FS Organizer keeps for its own files: "
                           "_fsorganizer-quarantine, or any name ending in .fsorg-partial or .fsorg-swap.");
    }

    return {};
}

QString TheExtractionFolderStayed(const SourceKind source)
{
    return IsAFolder(source)
        ? QObject::tr("FS Organizer could not delete the temporary folder it copied into. On the next start it lists "
                      "that folder under \"Unfinished imports\", where you can discard it.")
        : QObject::tr("FS Organizer could not delete the temporary folder it extracted into. On the next start it "
                      "lists that folder under \"Unfinished imports\", where you can discard it.");
}

QString WhyItWasNotInstalled(const FileResult result, const SourceKind source)
{
    switch (result)
    {
    case FileResult::NotEnoughFreeSpace: return ThereIsNoRoomFor(source);
    case FileResult::CouldNotExtract: return TheCopyOrExtractionFailed(source);
    case FileResult::VerificationFailed: return TheCopyDoesNotMatch(source);
    case FileResult::TheSimulatorIsRunning: return QObject::tr("The simulator is running, so it was not installed.");
    case FileResult::CouldNotDiscard:
        return QObject::tr("FS Organizer could not delete a temporary folder that an earlier installation left in "
                           "this category, so nothing was installed.");
    case FileResult::TheOriginIsOccupied:
        return QObject::tr("The category already has a folder with this name that is not an addon. FS Organizer does "
                           "not replace it.");
    case FileResult::TheDiskDisagreesWithTheScan: return TheDiskChanged(source);
    case FileResult::TheSourceIsInsideWhatWouldBeRemoved:
        return QObject::tr("What you dropped is inside a folder that installing would remove, or contains one. It was "
                           "not installed. Move it somewhere else and try again.");
    case FileResult::Cancelled: return YouCancelledIt();
    case FileResult::TheArchiveNeedsAPassword: return ItNeedsAPassword();
    default: return Explain(result);
    }
}
