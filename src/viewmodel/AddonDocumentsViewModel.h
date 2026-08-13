#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_DOCUMENTS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_DOCUMENTS_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>

#include <QtCore/QObject>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"

class AddonDocumentsViewModel final : public QObject
{
    Q_OBJECT

public:
    AddonDocumentsViewModel(const DocumentService& documents,
                            SceneryService& scenery,
                            Session& session,
                            BackgroundRunner& runner,
                            QObject* parent = nullptr);

    void Read(const std::filesystem::path& folder);

    [[nodiscard]] std::size_t Documents() const;

    [[nodiscard]] std::size_t Charts() const;

signals:
    void Indexed();

private:
    const DocumentService& documents_;
    SceneryService& scenery_;
    Session& session_;
    BackgroundRunner& runner_;

    DocumentsOfAnAddon indexed_{};
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_DOCUMENTS_VIEW_MODEL_H
