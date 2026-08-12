#ifndef FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"

struct DocumentLine
{
    QString name{};
    QString detail{};
    std::filesystem::path document{};
    bool favourite = false;
};

struct DocumentGroup
{
    QString name{};
    std::vector<DocumentLine> lines{};
};

class DocumentsViewModel final : public QObject
{
    Q_OBJECT

public:
    DocumentsViewModel(const DocumentService& documents,
                       SceneryService& scenery,
                       Session& session,
                       BackgroundRunner& runner,
                       QObject* parent = nullptr);

    void Read(const std::filesystem::path& folder);

    [[nodiscard]] std::size_t Documents() const;

    [[nodiscard]] std::size_t Charts() const;

    [[nodiscard]] std::vector<DocumentGroup> Groups() const;

    [[nodiscard]] const std::string& Addon() const;

    [[nodiscard]] const std::filesystem::path& Folder() const;

    [[nodiscard]] std::filesystem::path FullPathOf(const std::filesystem::path& document) const;

    [[nodiscard]] bool ItIsAFavourite(const std::filesystem::path& document) const;

    void Favour(const std::filesystem::path& document, bool favourite);

    [[nodiscard]] int PageOf(const std::filesystem::path& document) const;

    void RememberThePage(const std::filesystem::path& document, int page);

signals:
    void Indexed();

private:
    [[nodiscard]] DocumentGroup TheManuals() const;

    [[nodiscard]] static DocumentGroup TheFavouritesAmong(const std::vector<DocumentGroup>& groups);

    [[nodiscard]] const ReadDocument* Remembered(const std::filesystem::path& document) const;

    void Remember(const std::filesystem::path& document, const std::function<void(ReadDocument&)>& change);

    const DocumentService& documents_;
    SceneryService& scenery_;
    Session& session_;
    BackgroundRunner& runner_;

    DocumentsOfAnAddon indexed_{};
};

#endif // FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H
