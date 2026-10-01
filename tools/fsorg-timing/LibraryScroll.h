#ifndef FS_ORGANIZER_TOOLS_TIMING_LIBRARY_SCROLL_H
#define FS_ORGANIZER_TOOLS_TIMING_LIBRARY_SCROLL_H

class MainWindow;
class AddonTreePage;
class AddonTreeModel;
class CommunityViewModel;
class CoverageViewModel;
class PageTab;
class SceneryService;
class Session;
class TimedRunner;

int MeasureTheAppLibrary(MainWindow& window,
                         AddonTreePage& page,
                         AddonTreeModel& model,
                         CoverageViewModel& coverage,
                         CommunityViewModel& community,
                         PageTab& libraryTab,
                         PageTab& destinationsTab,
                         SceneryService& scenery,
                         Session& session,
                         const TimedRunner& timing);

#endif // FS_ORGANIZER_TOOLS_TIMING_LIBRARY_SCROLL_H
