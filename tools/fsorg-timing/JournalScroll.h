#ifndef FS_ORGANIZER_TOOLS_TIMING_JOURNAL_SCROLL_H
#define FS_ORGANIZER_TOOLS_TIMING_JOURNAL_SCROLL_H

#include "application/Session.h"
#include "domain/ports/OperationJournal.h"

int MeasureTheJournalScroll(const OperationJournal& journal, const Session& session);

#endif // FS_ORGANIZER_TOOLS_TIMING_JOURNAL_SCROLL_H
