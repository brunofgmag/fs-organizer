#include "viewmodel/FailureText.h"

#include <QtCore/QCoreApplication>

#include "support/PathText.h"

QString Explain(const LinkFailure failure)
{
    switch (failure)
    {
    case LinkFailure::DestinationHoldsRealFolder:
        return QObject::tr("there is already a real folder with that name in the destination");
    case LinkFailure::DestinationHoldsLiveLink:
        return QObject::tr("the destination already holds a live link from another program");
    case LinkFailure::UnreadableLinkTarget:
        return QObject::tr("the target of the link holding the destination could not be read");
    case LinkFailure::CouldNotReplaceStaleLink:
        return QObject::tr("the dead link holding the destination could not be removed");
    case LinkFailure::CouldNotCreateLink: return QObject::tr("the link could not be created");
    case LinkFailure::PrivilegeNotHeld:
        return QObject::tr("Windows needs privilege to create a symbolic link: turn on Developer Mode, or set the link "
                           "type back to directory junction in Options");
    case LinkFailure::PathIsNotAReparsePoint: return QObject::tr("the path is not a link, so nothing was removed");
    case LinkFailure::CouldNotRemoveLink: return QObject::tr("the link could not be removed");
    case LinkFailure::TheOutcomeIsUnknown:
        return QObject::tr("the journal records this operation, but does not say how it ended");
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
        return QObject::tr("Windows denied permission there, so running the app as administrator may get past it");
    case WriteAccess::TheVolumeIsReadOnly:
        return QObject::tr("that volume is read-only, and no privilege gets past that");
    case WriteAccess::ItRefusedForAnotherReason:
        return QObject::tr("Windows refused for a reason that is neither permission nor a read-only volume");
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
    case FileResult::CouldNotQuarantine: return QObject::tr("the losing copy could not be moved to the quarantine");
    case FileResult::SourceIsNotUnderADestination:
        return QObject::tr("the folder is not inside a destination of the profile");
    case FileResult::SourceIsAReparsePoint: return QObject::tr("the entry is a link, not a real folder");
    case FileResult::CouldNotCheckFreeSpace:
        return QObject::tr("the free space of the destination volume could not be read");
    case FileResult::NotEnoughFreeSpace: return QObject::tr("there is not enough free space in the library");
    case FileResult::CouldNotCopy:
        return QObject::tr("the copy failed, and what was already copied stays where it is for the resume");
    case FileResult::VerificationFailed:
        return QObject::tr("the copy does not match the source, so nothing was removed");
    case FileResult::CouldNotMoveIntoPlace: return QObject::tr("the copy could not be put in its final place");
    case FileResult::CouldNotRemoveSource: return QObject::tr("the source folder could not be removed");
    case FileResult::CouldNotCreateLink:
        return QObject::tr("the files are already in the library, but the link could not be created");
    case FileResult::TheOriginIsUnknown:
        return QObject::tr("neither the record beside it nor the journal says where this came from");
    case FileResult::CouldNotRestore: return QObject::tr("the folder could not be moved back");
    case FileResult::TheOriginIsOccupied:
        return QObject::tr("something with that name is already in the place this came from");
    case FileResult::CouldNotDiscard: return QObject::tr("it could not be discarded");
    case FileResult::CouldNotRemoveTheLink:
        return QObject::tr("one of the links pointing at the library copy could not be removed");
    case FileResult::TheIdentityIsTaken: return QObject::tr("this library already has an addon with that folder name");
    case FileResult::TheTargetIsNotInALibrary:
        return QObject::tr("the target of the operation is not inside a library of the profile");
    case FileResult::CouldNotCreateTheCategory: return QObject::tr("the category could not be created");
    case FileResult::TheCategoryStillHoldsAddons:
        return QObject::tr("this category still holds addons, and only an empty category can be deleted");
    case FileResult::CouldNotRemoveTheCategory: return QObject::tr("the category could not be deleted");
    case FileResult::TheOutcomeIsUnknown:
        return QObject::tr("the journal records this operation, but does not say how it ended");
    case FileResult::CouldNotReadTheSource:
        return QObject::tr("the source folder could not be walked, so nothing was copied");
    case FileResult::TheRecycleBinIsTooSmall:
        return QObject::tr("the selection does not fit in the Recycle Bin of that volume");
    case FileResult::TheRecycleBinCannotReachIt:
        return QObject::tr("the Recycle Bin stops at 260 characters, and this addon holds a longer path");
    case FileResult::CouldNotDelete: return QObject::tr("the folder could not be deleted");
    case FileResult::CouldNotRecordTheOrigin:
        return QObject::tr("the record that says where this came from could not be written, so nothing was moved");
    case FileResult::CannotWriteInTheOtherProgramsFolder:
        return QObject::tr("the folder of the other program does not accept writes from you, so nothing was taken "
                           "away from it");
    case FileResult::TheDiskDisagreesWithTheScan:
        return QObject::tr("the entry no longer points where the last scan saw it point, so nothing was touched");
    case FileResult::CouldNotReadTheStartupFile:
        return QObject::tr("the startup file of the simulator could not be read");
    case FileResult::CouldNotWriteTheStartupFile:
        return QObject::tr("the startup file of the simulator could not be written, so nothing changed");
    case FileResult::TheStartupEntriesAreLeftLoose:
        return QObject::tr("the startup entries of the simulator are not managed, so the app does not read or write "
                           "that file");
    case FileResult::TheAddonWasNeverMeasured:
        return QObject::tr("nobody measured this addon, so there is no telling whether the Recycle Bin of that volume "
                           "takes it");
    case FileResult::ThePathIsTooLong:
        return QObject::tr("the name is longer than a folder name can be, so the disk would refuse it");
    case FileResult::CouldNotReadThePackageList:
        return QObject::tr("the package list of the simulator could not be read");
    case FileResult::CouldNotWriteThePackageList:
        return QObject::tr("the package list of the simulator could not be written, so nothing changed");
    case FileResult::ThePackageListIsLeftLoose:
        return QObject::tr("the package list of the simulator is not managed, so the app does not read or write that "
                           "file");
    case FileResult::AnotherProgramIsHoldingIt:
        return QObject::tr("another program is holding that folder open, and Windows refuses to move it until "
                           "that program lets go");
    case FileResult::TheQuarantineIsOccupied:
        return QObject::tr("something with that name is already in the quarantine");
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
        return QObject::tr("%1: it is back, and %2 is in the quarantine with its origin recorded.")
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
                                                  : QObject::tr("deleted for good");
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
    case OperationKind::ImportVerifyStaging: return QObject::tr("Checking whether the copy matches the source…");
    case OperationKind::ImportMoveIntoPlace: return QObject::tr("Putting the copy in its final place…");
    case OperationKind::ImportRemoveSource: return QObject::tr("Removing the source folder…");
    case OperationKind::EnableAddon: return QObject::tr("Creating the link in the destination…");
    default: return {};
    }
}
