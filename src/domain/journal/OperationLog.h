#ifndef FS_ORGANIZER_DOMAIN_JOURNAL_OPERATION_LOG_H
#define FS_ORGANIZER_DOMAIN_JOURNAL_OPERATION_LOG_H

#include <filesystem>
#include <string>
#include <vector>

#include "domain/model/AddonId.h"
#include "domain/model/FileResult.h"
#include "domain/model/LinkFailure.h"
#include "domain/model/OperationKind.h"
#include "domain/model/OperationRecord.h"
#include "domain/ports/Clock.h"
#include "domain/ports/OperationJournal.h"

class OperationLog
{
public:
    OperationLog(OperationJournal& journal, const Clock& clock);

    void RecordLink(OperationKind kind,
                    const AddonId& addon,
                    const std::filesystem::path& source,
                    const std::filesystem::path& target,
                    LinkFailure failure) const;

    void RecordImport(OperationKind kind,
                      const AddonId& addon,
                      const std::filesystem::path& source,
                      const std::filesystem::path& target,
                      FileResult result,
                      OriginSource originSource = OriginSource::Unknown,
                      const std::string& label = {}) const;

    [[nodiscard]] std::chrono::system_clock::time_point Now() const;

    [[nodiscard]] std::vector<OperationRecord> History() const;

private:
    OperationJournal& journal_;
    const Clock& clock_;
};

#endif // FS_ORGANIZER_DOMAIN_JOURNAL_OPERATION_LOG_H
