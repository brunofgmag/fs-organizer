#ifndef FS_ORGANIZER_TOOLS_TIMING_JOURNAL_SCROLL_H
#define FS_ORGANIZER_TOOLS_TIMING_JOURNAL_SCROLL_H

#include "domain/model/SimulatorProfile.h"
#include "domain/ports/OperationJournal.h"

int MeasureTheJournalScroll(const OperationJournal& journal, const SimulatorProfile& profile);

#endif // FS_ORGANIZER_TOOLS_TIMING_JOURNAL_SCROLL_H
