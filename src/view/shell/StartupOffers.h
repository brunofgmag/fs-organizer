#ifndef FS_ORGANIZER_VIEW_SHELL_STARTUP_OFFERS_H
#define FS_ORGANIZER_VIEW_SHELL_STARTUP_OFFERS_H

class BisectionViewModel;
class ImportViewModel;
class LegacyImportViewModel;
class QWidget;
class Session;

void OfferToCarryOnTheSearchThatWasLeftHalfway(BisectionViewModel& bisectionViewModel, QWidget* parent);

void OfferToDropTheOverridesThatPointNowhere(Session& session, QWidget* parent);

void OfferWhatTheOldProgramKept(LegacyImportViewModel& legacyViewModel, QWidget* parent);

void OfferWhatALostImportLeftBehind(ImportViewModel& importViewModel, QWidget* parent);

void OfferToPutBackWhatALostSwapRenamed(ImportViewModel& importViewModel, QWidget* parent);

#endif // FS_ORGANIZER_VIEW_SHELL_STARTUP_OFFERS_H
