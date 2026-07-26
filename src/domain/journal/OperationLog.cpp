#include "domain/journal/OperationLog.h"

OperationLog::OperationLog(OperationJournal& journal, const Clock& clock) : journal_(journal), clock_(clock)
{
}

void OperationLog::RecordLink(const OperationKind kind,
                              const AddonId& addon,
                              const std::filesystem::path& source,
                              const std::filesystem::path& target,
                              const LinkFailure failure) const
{
    journal_.Append(OperationRecord::OfLink(clock_.Now(), kind, addon, source, target, failure));
}

void OperationLog::RecordImport(const OperationKind kind,
                                const AddonId& addon,
                                const std::filesystem::path& source,
                                const std::filesystem::path& target,
                                const ImportResult result) const
{
    journal_.Append(OperationRecord::OfImport(clock_.Now(), kind, addon, source, target, result));
}

std::vector<OperationRecord> OperationLog::History() const
{
    return journal_.Read();
}
