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
                                const FileResult result,
                                const OriginSource originSource) const
{
    journal_.Append(OperationRecord::OfImport(clock_.Now(), kind, addon, source, target, result, originSource));
}

std::chrono::system_clock::time_point OperationLog::Now() const
{
    return clock_.Now();
}

std::vector<OperationRecord> OperationLog::History() const
{
    return journal_.Read();
}
