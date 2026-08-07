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
    case FileResult::TheOriginIsUnknown: return QObject::tr("the journal does not know where this came from");
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
    QString line =
        QStringLiteral("%1: %2").arg(AsText(result.addonFolder.filename()), Explain(result.outcome.Failure()));

    if (const CopyConflict* conflict = result.outcome.Conflict(); conflict != nullptr)
    {
        line += QObject::tr("\n    folder in the destination: %1\n    addon in the library: %2")
                    .arg(AsText(conflict->destinationPath), AsText(conflict->libraryPath));
    }

    if (const OccupiedDestination* occupation = result.outcome.Occupation(); occupation != nullptr)
    {
        line += QObject::tr("\n    the current link points at: %1").arg(AsText(occupation->existingTarget));
    }

    return line;
}

QString Describe(const ImportOperationResult& result)
{
    return QStringLiteral("%1: %2%3")
        .arg(AsText(result.request.source.filename()), Explain(result.result), WhereTheOccupantIs(result.occupant));
}

QString Describe(const FileOperationResult& result)
{
    return QStringLiteral("%1: %2%3")
        .arg(AsText(result.path.filename()), Explain(result.result), WhereTheOccupantIs(result.occupant));
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
