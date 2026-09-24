#include "viewmodel/FailureText.h"

#include <QtCore/QCoreApplication>

#include "support/PathText.h"

QString Explain(const LinkFailure failure)
{
    switch (failure)
    {
    case LinkFailure::DestinationHoldsRealFolder:
        return QObject::tr("a folder with that name already exists in the destination");
    case LinkFailure::DestinationHoldsLiveLink:
        return QObject::tr("the destination already has a working link from another program");
    case LinkFailure::UnreadableLinkTarget: return QObject::tr("the link already in the destination could not be read");
    case LinkFailure::CouldNotReplaceStaleLink:
        return QObject::tr("the broken link already in the destination could not be removed");
    case LinkFailure::CouldNotCreateLink: return QObject::tr("the link could not be created");
    case LinkFailure::PrivilegeNotHeld:
        return QObject::tr("Windows needs privilege to create a symbolic link: turn on Developer Mode, or set the link "
                           "type back to directory junction in Options");
    case LinkFailure::PathIsNotAReparsePoint: return QObject::tr("the path is not a link, so nothing was removed");
    case LinkFailure::CouldNotRemoveLink: return QObject::tr("the link could not be removed");
    case LinkFailure::TheOutcomeIsUnknown: return QObject::tr("the journal does not say how this operation ended");
    case LinkFailure::None: break;
    }

    return {};
}

QString Explain(const CategoryRule rule)
{
    switch (rule)
    {
    case CategoryRule::TheNameSaysAirport: return QObject::tr("the folder name says \"airport\"");
    case CategoryRule::TheNameSaysTraffic: return QObject::tr("the folder name says \"traffic\"");
    case CategoryRule::TheContentTypeIsScenery: return QObject::tr("the manifest declares content_type SCENERY");
    case CategoryRule::TheContentTypeIsSound: return QObject::tr("the manifest declares content_type SOUND");
    case CategoryRule::TheContentTypeIsLivery: return QObject::tr("the manifest declares content_type LIVERY");
    case CategoryRule::None: break;
    }

    return {};
}

QString Explain(const WriteAccess access)
{
    switch (access)
    {
    case WriteAccess::TheFolderIsNotThere: return QObject::tr("that folder is no longer there");
    case WriteAccess::PermissionIsDenied:
        return QObject::tr("Windows denied access; running the app as administrator may help");
    case WriteAccess::TheVolumeIsReadOnly: return QObject::tr("that drive is read-only");
    case WriteAccess::ItRefusedForAnotherReason: return QObject::tr("Windows refused the operation");
    case WriteAccess::ItAccepts: break;
    }

    return {};
}

QString Explain(const FileResult result)
{
    switch (result)
    {
    case FileResult::Completed: return {};
    case FileResult::Cancelled: return QObject::tr("cancelled by you");
    case FileResult::TheSimulatorIsRunning: return QObject::tr("the simulator is running");
    case FileResult::CouldNotQuarantine:
        return QObject::tr("the copy you did not keep could not be moved to the quarantine");
    case FileResult::SourceIsNotUnderADestination:
        return QObject::tr("the folder is not inside a destination of the profile");
    case FileResult::SourceIsAReparsePoint: return QObject::tr("the entry is a link, not a real folder");
    case FileResult::CouldNotCheckFreeSpace:
        return QObject::tr("the free space on the destination drive could not be checked");
    case FileResult::NotEnoughFreeSpace: return QObject::tr("there is not enough free space in the library");
    case FileResult::CouldNotCopy:
        return QObject::tr("the copy failed; what was already copied is kept so the import can resume");
    case FileResult::VerificationFailed:
        return QObject::tr("the copy does not match the source, so nothing was removed");
    case FileResult::CouldNotMoveIntoPlace: return QObject::tr("the copy could not be put in its final place");
    case FileResult::CouldNotRemoveSource: return QObject::tr("the source folder could not be removed");
    case FileResult::CouldNotCreateLink:
        return QObject::tr("the files are already in the library, but the link could not be created");
    case FileResult::TheOriginIsUnknown: return QObject::tr("its origin is unknown");
    case FileResult::CouldNotRestore: return QObject::tr("the folder could not be moved back");
    case FileResult::TheOriginIsOccupied:
        return QObject::tr("something with that name is already in the place this came from");
    case FileResult::CouldNotDiscard: return QObject::tr("it could not be discarded");
    case FileResult::CouldNotRemoveTheLink:
        return QObject::tr("one of the links pointing at the library copy could not be removed");
    case FileResult::TheIdentityIsTaken: return QObject::tr("this library already has an addon with that folder name");
    case FileResult::TheTargetIsNotInALibrary: return QObject::tr("the target is not inside a library of this profile");
    case FileResult::CouldNotCreateTheCategory: return QObject::tr("the category could not be created");
    case FileResult::TheCategoryStillHoldsAddons: return QObject::tr("only empty categories can be deleted");
    case FileResult::CouldNotRemoveTheCategory: return QObject::tr("the category could not be deleted");
    case FileResult::TheOutcomeIsUnknown: return QObject::tr("the journal does not say how this operation ended");
    case FileResult::CouldNotReadTheSource:
        return QObject::tr("the source folder could not be read, so nothing was copied");
    case FileResult::TheRecycleBinIsTooSmall:
        return QObject::tr("the selection does not fit in the Recycle Bin of that volume");
    case FileResult::TheRecycleBinCannotReachIt:
        return QObject::tr("this addon has a path longer than the 260 characters the Recycle Bin accepts");
    case FileResult::CouldNotDelete: return QObject::tr("the folder could not be deleted");
    case FileResult::CouldNotRecordTheOrigin:
        return QObject::tr("the origin record could not be saved, so nothing was moved");
    case FileResult::CannotWriteInTheOtherProgramsFolder:
        return QObject::tr("you cannot write to the other program's folder, so nothing was moved from it");
    case FileResult::TheDiskDisagreesWithTheScan:
        return QObject::tr("the entry changed since the last scan, so nothing was changed");
    case FileResult::CouldNotReadTheStartupFile:
        return QObject::tr("the startup file of the simulator could not be read");
    case FileResult::CouldNotWriteTheStartupFile:
        return QObject::tr("the startup file of the simulator could not be written, so nothing changed");
    case FileResult::TheStartupEntriesAreLeftLoose:
        return QObject::tr("startup entries are not managed, so that file is not read or changed");
    case FileResult::TheAddonWasNeverMeasured:
        return QObject::tr("this addon was not measured, so it is unknown whether it fits in the Recycle Bin");
    case FileResult::ThePathIsTooLong: return QObject::tr("the name is too long for a folder name");
    case FileResult::CouldNotReadThePackageList:
        return QObject::tr("the package list of the simulator could not be read");
    case FileResult::CouldNotWriteThePackageList:
        return QObject::tr("the package list of the simulator could not be written, so nothing changed");
    case FileResult::ThePackageListIsLeftLoose:
        return QObject::tr("the package list is not managed, so that file is not read or changed");
    case FileResult::AnotherProgramIsHoldingIt:
        return QObject::tr("another program has that folder open; close it and try again");
    case FileResult::TheQuarantineIsOccupied:
        return QObject::tr("something with that name is already in the quarantine");
    case FileResult::ThereIsNowhereToQuarantineIt:
        return QObject::tr(
            "this copy is outside the profile's destinations and libraries, so it cannot go to the quarantine");
    }

    return {};
}

namespace
{
    QString WhereTheOccupantIs(const std::filesystem::path& occupant)
    {
        return occupant.empty() ? QString{} : QObject::tr("\n    the occupant is in: %1").arg(AsText(occupant));
    }
}

QString Describe(const LinkOperationResult& result)
{
    if (const FileResult* file = result.outcome.File(); file != nullptr)
    {
        return QStringLiteral("%1: %2").arg(AsText(result.addonFolder.filename()), Explain(*file));
    }

    QString line =
        QStringLiteral("%1: %2").arg(AsText(result.addonFolder.filename()), Explain(result.outcome.Failure()));

    if (const CopyConflict* conflict = result.outcome.Conflict(); conflict != nullptr)
    {
        line += QObject::tr("\n    folder in the destination: %1\n    addon in the library: %2")
                    .arg(AsText(conflict->provenancePath), AsText(conflict->libraryPath));
    }

    if (const OccupiedDestination* occupation = result.outcome.Occupation(); occupation != nullptr)
    {
        line += QObject::tr("\n    the current link points at: %1").arg(AsText(occupation->existingTarget));
    }

    return line;
}

namespace
{
    QString WhatStoppedTheWrite(const WriteAccess access)
    {
        return ItAcceptsWrites(access) ? QString{} : QObject::tr("\n    what stopped it: %1").arg(Explain(access));
    }

    QString WhichFolderTheOtherProgramOwns(const ImportOperationResult& result)
    {
        if (result.result != FileResult::CannotWriteInTheOtherProgramsFolder)
        {
            return {};
        }

        return QObject::tr("\n    the folder that refused: %1").arg(AsText(result.request.externalSource))
            + WhatStoppedTheWrite(result.writeAccess);
    }
}

QString Describe(const ImportOperationResult& result)
{
    return QStringLiteral("%1: %2%3%4")
        .arg(AsText(result.request.source.filename()), Explain(result.result), WhereTheOccupantIs(result.occupant),
             WhichFolderTheOtherProgramOwns(result));
}

QString Describe(const FileOperationResult& result)
{
    return QStringLiteral("%1: %2%3%4")
        .arg(AsText(result.path.filename()), Explain(result.result), WhereTheOccupantIs(result.occupant),
             WhatStoppedTheWrite(result.writeAccess));
}

QString Describe(const SwapResult& result)
{
    if (result.Succeeded())
    {
        return QObject::tr("%1: restored, and %2 moved to the quarantine.")
            .arg(AsText(result.item.filename()), AsText(result.occupant.filename()));
    }

    const QString step = result.stoppedAt == SwapStep::QuarantineTheOccupant
        ? QObject::tr("putting %1 in the quarantine").arg(AsText(result.occupant.filename()))
        : QObject::tr("bringing %1 back").arg(AsText(result.item.filename()));

    const QString holds = result.inTheLibrary.empty()
        ? QObject::tr("\n    neither of them is in the library right now, and nothing was deleted")
        : QObject::tr("\n    the library still holds: %1").arg(AsText(result.inTheLibrary));

    return QObject::tr("%1: it stopped at %2, because %3.%4")
        .arg(AsText(result.item.filename()), step, Explain(result.result), holds);
}

namespace
{
    QString WhatTheRouteDid(const DeletionRoute route)
    {
        return route == DeletionRoute::RecycleBin ? QObject::tr("moved to the Recycle Bin")
                                                  : QObject::tr("deleted permanently");
    }

    QString WhichLinksWentAway(const std::vector<std::filesystem::path>& links)
    {
        QString said;

        for (const std::filesystem::path& link : links)
        {
            said += QObject::tr("\n    the link already removed: %1").arg(AsText(link));
        }

        return said;
    }
}

QString Describe(const DeletionResult& result, const DeletionRoute route)
{
    const QString name = AsText(result.folder.filename());

    if (Succeeded(result.result))
    {
        return QStringLiteral("%1: %2").arg(name, WhatTheRouteDid(route));
    }

    return QStringLiteral("%1: %2%3").arg(name, Explain(result.result), WhichLinksWentAway(result.linksRemoved));
}

QString NameOfImportStep(const OperationKind kind)
{
    switch (kind)
    {
    case OperationKind::ImportCopyToStaging: return QObject::tr("Copying to the library…");
    case OperationKind::ImportVerifyStaging: return QObject::tr("Verifying the copy…");
    case OperationKind::ImportMoveIntoPlace: return QObject::tr("Moving the copy into place…");
    case OperationKind::ImportRemoveSource: return QObject::tr("Removing the source folder…");
    case OperationKind::EnableAddon: return QObject::tr("Creating the link in the destination…");
    default: return {};
    }
}
