#ifndef FS_ORGANIZER_VIEWMODEL_ARCHIVE_REFUSAL_TEXT_H
#define FS_ORGANIZER_VIEWMODEL_ARCHIVE_REFUSAL_TEXT_H

#include <QtCore/QString>

#include "domain/installing/PackagesInAnArchive.h"
#include "domain/model/ArchiveEntry.h"
#include "domain/model/FileResult.h"
#include "viewmodel/InstallOffer.h"

[[nodiscard]] QString WhyTheArchiveIsRefused(ArchiveFailure failure, const QString& fileName, SourceKind source);

[[nodiscard]] QString WhyTheArchiveIsRefused(NothingToInstall refusal, SourceKind source);

[[nodiscard]] QString WhyItWasNotInstalled(FileResult result, SourceKind source);

[[nodiscard]] QString TheExtractionFolderStayed(SourceKind source);

#endif // FS_ORGANIZER_VIEWMODEL_ARCHIVE_REFUSAL_TEXT_H
