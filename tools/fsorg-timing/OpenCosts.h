#ifndef FS_ORGANIZER_TOOLS_TIMING_OPEN_COSTS_H
#define FS_ORGANIZER_TOOLS_TIMING_OPEN_COSTS_H

class AddonDocumentsViewModel;
class AddonTreeModel;
class AddonTreePage;
class AddonTreeViewModel;
class ColdableSceneryCache;
class DocumentService;
class FilesystemProbe;
class MainWindow;
class ProfileService;
class SceneryService;
class Session;

int MeasureTheOpenCosts(MainWindow& window,
                        AddonTreePage& page,
                        AddonTreeModel& model,
                        AddonTreeViewModel& treeViewModel,
                        AddonDocumentsViewModel& documentsViewModel,
                        const ProfileService& profileService,
                        const DocumentService& documentService,
                        const FilesystemProbe& filesystemProbe,
                        SceneryService& scenery,
                        ColdableSceneryCache& sceneryCache,
                        Session& session);

#endif // FS_ORGANIZER_TOOLS_TIMING_OPEN_COSTS_H
