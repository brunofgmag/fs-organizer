#ifndef FS_ORGANIZER_TESTS_SUPPORT_ENUM_PRINTING_H
#define FS_ORGANIZER_TESTS_SUPPORT_ENUM_PRINTING_H

#include <QtTest/QtTest>

#include "domain/model/CheckState.h"
#include "domain/model/EntryClassification.h"
#include "domain/model/ImportResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"
#include "domain/model/OperationRecord.h"

namespace QTest
{
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
        }

        return qstrdup("LinkFailure(?)");
    }

    template<>
    inline char* toString(const ImportResult& t)
    {
        switch (t)
        {
        case ImportResult::Completed: return qstrdup("Completed");
        case ImportResult::Cancelled: return qstrdup("Cancelled");
        case ImportResult::TheSimulatorIsRunning: return qstrdup("TheSimulatorIsRunning");
        case ImportResult::CouldNotQuarantine: return qstrdup("CouldNotQuarantine");
        case ImportResult::SourceIsNotUnderADestination: return qstrdup("SourceIsNotUnderADestination");
        case ImportResult::SourceIsAReparsePoint: return qstrdup("SourceIsAReparsePoint");
        case ImportResult::CouldNotCheckFreeSpace: return qstrdup("CouldNotCheckFreeSpace");
        case ImportResult::NotEnoughFreeSpace: return qstrdup("NotEnoughFreeSpace");
        case ImportResult::CouldNotCopy: return qstrdup("CouldNotCopy");
        case ImportResult::VerificationFailed: return qstrdup("VerificationFailed");
        case ImportResult::CouldNotMoveIntoPlace: return qstrdup("CouldNotMoveIntoPlace");
        case ImportResult::CouldNotRemoveSource: return qstrdup("CouldNotRemoveSource");
        case ImportResult::CouldNotCreateLink: return qstrdup("CouldNotCreateLink");
        }

        return qstrdup("ImportResult(?)");
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
        }

        return qstrdup("OperationKind(?)");
    }

    template<>
    inline char* toString(const OperationOutcome& t)
    {
        if (const LinkFailure* failure = std::get_if<LinkFailure>(&t))
        {
            return toString(*failure);
        }

        return toString(std::get<ImportResult>(t));
    }
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_ENUM_PRINTING_H
