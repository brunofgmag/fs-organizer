#ifndef FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"
#include "application/ports/DocumentIndexCache.h"
#include "domain/documents/DocumentClassification.h"
#include "domain/ports/Clock.h"

enum class DocumentPanel : int
{
    Documents = 0,
    Charts = 1,
};

struct DocumentLine
{
    QString name{};
    QString detail{};
    QString caption{};
    QString locator{};
    std::string addon{};
    std::filesystem::path document{};
    std::filesystem::path file{};
    DocumentKind kind = DocumentKind::Document;
    bool favourite = false;
};

struct DocumentGroup
{
    QString name{};
    QString aside{};
    QString count{};
    std::vector<DocumentGroup> groups{};
    std::vector<DocumentLine> lines{};
};

struct DocumentPlace
{
    DocumentPanel panel = DocumentPanel::Documents;
    QString group{};
};

class DocumentsViewModel final : public QObject
{
    Q_OBJECT

public:
    DocumentsViewModel(const DocumentService& documents,
                       SceneryService& scenery,
                       Session& session,
                       BackgroundRunner& runner,
                       DocumentIndexCache& cache,
                       const Clock& clock,
                       QObject* parent = nullptr);

    void ShowWhatWasKept();

    void ReadTheLibrary();

    void Stop();

    [[nodiscard]] bool Reading() const;

    [[nodiscard]] bool ItWasRead() const;

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> ReadAt() const;

    [[nodiscard]] std::vector<DocumentGroup> GroupsOf(DocumentPanel panel) const;

    [[nodiscard]] std::size_t CountOf(DocumentPanel panel) const;

    [[nodiscard]] std::optional<DocumentPlace> WhereToFind(const std::string& addon) const;

    [[nodiscard]] bool ItIsAFavourite(const DocumentLine& line) const;

    void Favour(const DocumentLine& line, bool favourite);

    [[nodiscard]] int PageOf(const DocumentLine& line) const;

    void RememberThePage(const DocumentLine& line, int page);

    [[nodiscard]] std::vector<DocumentBookmark> BookmarksOf(const DocumentLine& line) const;

    void MarkThePage(const DocumentLine& line, int page, bool marked);

    void NameTheBookmark(const DocumentLine& line, int page, const std::string& name);

    [[nodiscard]] bool TheWheelZooms() const;

    void MakeTheWheelZoom(bool zooming);

    [[nodiscard]] bool TheDragMovesThePage() const;

    void MakeTheDragMoveThePage(bool moving);

signals:
    void Indexed();

    void Arrived();

    void ReadingChanged();

    void Progressed(int indexed, int outOf);

private:
    [[nodiscard]] std::vector<DocumentsOfAnAddon>
    WhatEachAddonCarries(const std::vector<Library>& libraries, const std::vector<AddonToRead>& addons, bool& stopped);

    void TakeWhatWasRead(std::vector<DocumentsOfAnAddon>& found, bool stopped);

    void TakeTheAddonThatArrived(const DocumentsOfAnAddon& addon);

    [[nodiscard]] const std::vector<DocumentsOfAnAddon>& WhatToShow() const;

    [[nodiscard]] std::vector<DocumentGroup> TheDocuments() const;

    [[nodiscard]] std::vector<DocumentGroup> TheCharts() const;

    [[nodiscard]] DocumentGroup TheChartsOf(const DocumentsOfAnAddon& addon, const ChartsOfAnAirport& airport) const;

    [[nodiscard]] DocumentLine LineOfADocument(const DocumentsOfAnAddon& addon,
                                               const std::filesystem::path& document) const;

    [[nodiscard]] DocumentLine LineOfAChart(const DocumentsOfAnAddon& addon,
                                            const QString& locator,
                                            const ChartsOfAType& type,
                                            const ChartEntry& chart) const;

    [[nodiscard]] static DocumentGroup TheFavouritesAmong(const std::vector<DocumentGroup>& groups);

    [[nodiscard]] const ReadDocument* Remembered(const DocumentLine& line) const;

    void Remember(const DocumentLine& line, const std::function<void(ReadDocument&)>& change);

    const DocumentService& documents_;
    SceneryService& scenery_;
    Session& session_;
    BackgroundRunner& runner_;
    DocumentIndexCache& cache_;
    const Clock& clock_;

    std::vector<DocumentsOfAnAddon> indexed_{};
    std::vector<DocumentsOfAnAddon> arriving_{};
    std::optional<std::chrono::system_clock::time_point> readAt_{};
    bool itWasRead_ = false;
    bool reading_ = false;
    bool stop_ = false;
};

#endif // FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H
