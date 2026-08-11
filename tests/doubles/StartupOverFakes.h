#ifndef FS_ORGANIZER_TESTS_DOUBLES_STARTUP_OVER_FAKES_H
#define FS_ORGANIZER_TESTS_DOUBLES_STARTUP_OVER_FAKES_H

#include "application/StartupService.h"
#include "domain/ports/FilesystemProbe.h"
#include "tests/doubles/FakeProcessProbe.h"
#include "tests/doubles/FakeStartupEntries.h"

struct StartupOverFakes
{
    explicit StartupOverFakes(const FilesystemProbe& filesystemProbe)
        : service(entries, processProbe, filesystemProbe, true)
    {
    }

    FakeStartupEntries entries;
    FakeProcessProbe processProbe;
    StartupService service;
};

#endif // FS_ORGANIZER_TESTS_DOUBLES_STARTUP_OVER_FAKES_H
