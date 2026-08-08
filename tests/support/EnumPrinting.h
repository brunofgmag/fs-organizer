#ifndef FS_ORGANIZER_TESTS_SUPPORT_ENUM_PRINTING_H
#define FS_ORGANIZER_TESTS_SUPPORT_ENUM_PRINTING_H

#include <QtTest/QtTest>

#include "application/DependencyReport.h"
#include "application/model/RestorePlan.h"
#include "domain/legacy/ProposedState.h"
#include "domain/model/CheckState.h"
#include "domain/model/DestinationEntry.h"
#include "domain/model/FileResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/LinkType.h"
#include "domain/model/OperationKind.h"
#include "domain/model/OperationRecord.h"
#include "domain/model/PackagePresence.h"
#include "domain/model/Preset.h"
#include "domain/model/QuarantineOrigin.h"

namespace QTest
{
    template<>
    inline char* toString(const ProposedState& t)
    {
        switch (t)
        {
        case ProposedState::New: return qstrdup("New");
        case ProposedState::AlreadyPresent: return qstrdup("AlreadyPresent");
        }

        return qstrdup("ProposedState(?)");
    }

    template<>
    inline char* toString(const PresetAction& t)
    {
        switch (t)
        {
        case PresetAction::Enable: return qstrdup("Enable");
        case PresetAction::Disable: return qstrdup("Disable");
        }

        return qstrdup("PresetAction(?)");
    }

    template<>
    inline char* toString(const CheckState& t)
    {
        switch (t)
        {
        case CheckState::Unchecked: return qstrdup("Unchecked");
        case CheckState::Checked: return qstrdup("Checked");
        case CheckState::Partial: return qstrdup("Partial");
        }

        return qstrdup("CheckState(?)");
    }

    template<>
    inline char* toString(const EntryClassification& t)
    {
        switch (t)
        {
        case EntryClassification::Managed: return qstrdup("Managed");
        case EntryClassification::External: return qstrdup("External");
        case EntryClassification::Divergent: return qstrdup("Divergent");
        case EntryClassification::Vanished: return qstrdup("Vanished");
        case EntryClassification::Broken: return qstrdup("Broken");
        case EntryClassification::Unavailable: return qstrdup("Unavailable");
        case EntryClassification::Unmanaged: return qstrdup("Unmanaged");
        case EntryClassification::Duplicated: return qstrdup("Duplicated");
        }

        return qstrdup("EntryClassification(?)");
    }

    template<>
    inline char* toString(const LinkFailure& t)
    {
        switch (t)
        {
        case LinkFailure::None: return qstrdup("None");
        case LinkFailure::DestinationHoldsRealFolder: return qstrdup("DestinationHoldsRealFolder");
        case LinkFailure::DestinationHoldsLiveLink: return qstrdup("DestinationHoldsLiveLink");
        case LinkFailure::UnreadableLinkTarget: return qstrdup("UnreadableLinkTarget");
        case LinkFailure::CouldNotReplaceStaleLink: return qstrdup("CouldNotReplaceStaleLink");
        case LinkFailure::CouldNotCreateLink: return qstrdup("CouldNotCreateLink");
        case LinkFailure::PathIsNotAReparsePoint: return qstrdup("PathIsNotAReparsePoint");
        case LinkFailure::CouldNotRemoveLink: return qstrdup("CouldNotRemoveLink");
        case LinkFailure::PrivilegeNotHeld: return qstrdup("PrivilegeNotHeld");
        case LinkFailure::TheOutcomeIsUnknown: return qstrdup("TheOutcomeIsUnknown");
        }

        return qstrdup("LinkFailure(?)");
    }

    template<>
    inline char* toString(const LinkType& t)
    {
        switch (t)
        {
        case LinkType::Junction: return qstrdup("Junction");
        case LinkType::Symbolic: return qstrdup("Symbolic");
        }

        return qstrdup("LinkType(?)");
    }

    template<>
    inline char* toString(const FileResult& t)
    {
        switch (t)
        {
        case FileResult::Completed: return qstrdup("Completed");
        case FileResult::Cancelled: return qstrdup("Cancelled");
        case FileResult::TheSimulatorIsRunning: return qstrdup("TheSimulatorIsRunning");
        case FileResult::CouldNotQuarantine: return qstrdup("CouldNotQuarantine");
        case FileResult::SourceIsNotUnderADestination: return qstrdup("SourceIsNotUnderADestination");
        case FileResult::SourceIsAReparsePoint: return qstrdup("SourceIsAReparsePoint");
        case FileResult::CouldNotCheckFreeSpace: return qstrdup("CouldNotCheckFreeSpace");
        case FileResult::NotEnoughFreeSpace: return qstrdup("NotEnoughFreeSpace");
        case FileResult::CouldNotCopy: return qstrdup("CouldNotCopy");
        case FileResult::VerificationFailed: return qstrdup("VerificationFailed");
        case FileResult::CouldNotMoveIntoPlace: return qstrdup("CouldNotMoveIntoPlace");
        case FileResult::CouldNotRemoveSource: return qstrdup("CouldNotRemoveSource");
        case FileResult::CouldNotCreateLink: return qstrdup("CouldNotCreateLink");
        case FileResult::TheOriginIsUnknown: return qstrdup("TheOriginIsUnknown");
        case FileResult::CouldNotRestore: return qstrdup("CouldNotRestore");
        case FileResult::CouldNotDiscard: return qstrdup("CouldNotDiscard");
        case FileResult::CouldNotRemoveTheLink: return qstrdup("CouldNotRemoveTheLink");
        case FileResult::TheIdentityIsTaken: return qstrdup("TheIdentityIsTaken");
        case FileResult::TheTargetIsNotInALibrary: return qstrdup("TheTargetIsNotInALibrary");
        case FileResult::CouldNotCreateTheCategory: return qstrdup("CouldNotCreateTheCategory");
        case FileResult::TheCategoryStillHoldsAddons: return qstrdup("TheCategoryStillHoldsAddons");
        case FileResult::CouldNotRemoveTheCategory: return qstrdup("CouldNotRemoveTheCategory");
        case FileResult::TheOutcomeIsUnknown: return qstrdup("TheOutcomeIsUnknown");
        case FileResult::CouldNotReadTheSource: return qstrdup("CouldNotReadTheSource");
        case FileResult::TheOriginIsOccupied: return qstrdup("TheOriginIsOccupied");
        case FileResult::TheRecycleBinIsTooSmall: return qstrdup("TheRecycleBinIsTooSmall");
        case FileResult::TheRecycleBinCannotReachIt: return qstrdup("TheRecycleBinCannotReachIt");
        case FileResult::CouldNotDelete: return qstrdup("CouldNotDelete");
        case FileResult::CouldNotRecordTheOrigin: return qstrdup("CouldNotRecordTheOrigin");
        case FileResult::CannotWriteInTheOtherProgramsFolder: return qstrdup("CannotWriteInTheOtherProgramsFolder");
        case FileResult::TheDiskDisagreesWithTheScan: return qstrdup("TheDiskDisagreesWithTheScan");
        case FileResult::CouldNotReadTheStartupFile: return qstrdup("CouldNotReadTheStartupFile");
        case FileResult::CouldNotWriteTheStartupFile: return qstrdup("CouldNotWriteTheStartupFile");
        }

        return qstrdup("FileResult(?)");
    }

    template<>
    inline char* toString(const SwapStep& t)
    {
        switch (t)
        {
        case SwapStep::QuarantineTheOccupant: return qstrdup("QuarantineTheOccupant");
        case SwapStep::RestoreTheItem: return qstrdup("RestoreTheItem");
        }

        return qstrdup("SwapStep(?)");
    }

    template<>
    inline char* toString(const OriginSource& t)
    {
        switch (t)
        {
        case OriginSource::Unknown: return qstrdup("Unknown");
        case OriginSource::Sidecar: return qstrdup("Sidecar");
        case OriginSource::Journal: return qstrdup("Journal");
        }

        return qstrdup("OriginSource(?)");
    }

    template<>
    inline char* toString(const OperationKind& t)
    {
        switch (t)
        {
        case OperationKind::EnableAddon: return qstrdup("EnableAddon");
        case OperationKind::DisableAddon: return qstrdup("DisableAddon");
        case OperationKind::RemoveBrokenLink: return qstrdup("RemoveBrokenLink");
        case OperationKind::RepointLink: return qstrdup("RepointLink");
        case OperationKind::ImportCopyToStaging: return qstrdup("ImportCopyToStaging");
        case OperationKind::ImportVerifyStaging: return qstrdup("ImportVerifyStaging");
        case OperationKind::ImportMoveIntoPlace: return qstrdup("ImportMoveIntoPlace");
        case OperationKind::ImportRemoveSource: return qstrdup("ImportRemoveSource");
        case OperationKind::QuarantineFromDestination: return qstrdup("QuarantineFromDestination");
        case OperationKind::QuarantineFromLibrary: return qstrdup("QuarantineFromLibrary");
        case OperationKind::RestoreFromQuarantine: return qstrdup("RestoreFromQuarantine");
        case OperationKind::DiscardFromQuarantine: return qstrdup("DiscardFromQuarantine");
        case OperationKind::DiscardStaging: return qstrdup("DiscardStaging");
        case OperationKind::MoveAddon: return qstrdup("MoveAddon");
        case OperationKind::CreateCategory: return qstrdup("CreateCategory");
        case OperationKind::RenameCategory: return qstrdup("RenameCategory");
        case OperationKind::RemoveCategory: return qstrdup("RemoveCategory");
        case OperationKind::RecycleFromLibrary: return qstrdup("RecycleFromLibrary");
        case OperationKind::DeleteFromLibrary: return qstrdup("DeleteFromLibrary");
        case OperationKind::LinkTheOtherProgramsFolder: return qstrdup("LinkTheOtherProgramsFolder");
        }

        return qstrdup("OperationKind(?)");
    }

    template<>
    inline char* toString(const DependencyResolution& t)
    {
        switch (t)
        {
        case DependencyResolution::InThisLibrary: return qstrdup("InThisLibrary");
        case DependencyResolution::InTheSimulator: return qstrdup("InTheSimulator");
        case DependencyResolution::Unverifiable: return qstrdup("Unverifiable");
        }

        return qstrdup("DependencyResolution(?)");
    }

    template<>
    inline char* toString(const PackagePresence& t)
    {
        switch (t)
        {
        case PackagePresence::Present: return qstrdup("Present");
        case PackagePresence::Absent: return qstrdup("Absent");
        case PackagePresence::Unverifiable: return qstrdup("Unverifiable");
        }

        return qstrdup("PackagePresence(?)");
    }

    template<>
    inline char* toString(const OperationOutcome& t)
    {
        if (const LinkFailure* failure = std::get_if<LinkFailure>(&t))
        {
            return toString(*failure);
        }

        return toString(std::get<FileResult>(t));
    }
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_ENUM_PRINTING_H
