#ifndef FS_ORGANIZER_DOMAIN_PORTS_OPERATION_JOURNAL_H
#define FS_ORGANIZER_DOMAIN_PORTS_OPERATION_JOURNAL_H

#include "domain/model/OperationRecord.h"

class OperationJournal
{
public:
    virtual ~OperationJournal() = default;

    virtual void Append(const OperationRecord& record) = 0;
};

#endif // FS_ORGANIZER_DOMAIN_PORTS_OPERATION_JOURNAL_H
