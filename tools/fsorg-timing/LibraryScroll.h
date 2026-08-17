#ifndef FS_ORGANIZER_TOOLS_TIMING_LIBRARY_SCROLL_H
#define FS_ORGANIZER_TOOLS_TIMING_LIBRARY_SCROLL_H

class MainWindow;
class AddonTreePage;
class AddonTreeModel;
class CoverageViewModel;
class SceneryService;
class Session;

int MeasureTheAppLibrary(MainWindow& window,
                         AddonTreePage& page,
                         AddonTreeModel& model,
                         CoverageViewModel& coverage,
                         SceneryService& scenery,
                         Session& session);

#endif // FS_ORGANIZER_TOOLS_TIMING_LIBRARY_SCROLL_H
