#ifndef FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include "application/DocumentService.h"
#include "application/SceneryService.h"
#include "application/Session.h"
#include "application/ports/BackgroundRunner.h"
#include "viewmodel/GuardedRunner.h"
#include "application/ports/DocumentIndexCache.h"
#include "application/model/ManualDelivery.h"
#include "application/ports/ManualSource.h"
#include "application/model/ReadingGestures.h"
#include "domain/documents/DocumentClassification.h"
#include "domain/ports/Clock.h"

enum class DocumentPanel : int
{
    Documents = 0,
    Charts = 1,
};

enum class ManualState : int
{
    NotHere = 0,
    Fetching = 1,
    Here = 2,
    Failed = 3,
    NotShipped = 4,
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

class DocumentsViewModel final : public QObject, public ManualSourceObserver
{
    Q_OBJECT

public:
    DocumentsViewModel(const DocumentService& documents,
                       SceneryService& scenery,
                       Session& session,
                       BackgroundRunner& runner,
                       DocumentIndexCache& cache,
                       ManualSource& manual,
                       ManualDelivery manualDelivery,
                       const Clock& clock,
                       QObject* parent = nullptr);

    ~DocumentsViewModel() override;

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

    void FlushThePage();

    [[nodiscard]] std::vector<DocumentBookmark> BookmarksOf(const DocumentLine& line) const;

    void MarkThePage(const DocumentLine& line, int page, bool marked);

    void NameTheBookmark(const DocumentLine& line, int page, const std::string& name);

    [[nodiscard]] ReadingGestures TheGesturesOf(DocumentKind kind) const;

    void MakeTheWheelZoom(DocumentKind kind, bool zooming);

    void MakeTheDragMoveThePage(DocumentKind kind, bool moving);

    void TheInterfaceSpeaks(const std::string& language);

    [[nodiscard]] ManualState TheManualIs() const;

    [[nodiscard]] DocumentLine TheManualLine() const;

    [[nodiscard]] bool ItIsTheManual(const DocumentLine& line) const;

    [[nodiscard]] QString WhatHappenedToTheManual() const;

    void FetchTheManual();

    void OnManualFetched(bool ok, const std::filesystem::path& file, const std::string& error) override;

signals:
    void Indexed();

    void Arrived();

    void ReadingChanged();

    void Progressed(int indexed, int outOf);

    void TheManualChanged();

private:
    struct PendingPage
    {
        std::string addon{};
        std::string document{};
        int page = 0;

        [[nodiscard]] bool IsFor(const std::string& ofAddon, const std::string& ofDocument) const
        {
            return addon == ofAddon && document == ofDocument;
        }
    };

    [[nodiscard]] std::vector<DocumentsOfAnAddon> WhatEachAddonCarries(const std::vector<AddonToRead>& addons,
                                                                       const std::vector<DocumentsOfAnAddon>& before,
                                                                       bool& stopped);

    void CountWhatIsShown();

    void CountTheLinesOf(const DocumentsOfAnAddon& addon);

    void TakeWhatWasRead(std::vector<DocumentsOfAnAddon>& found, bool stopped);

    void TakeTheAddonThatArrived(const DocumentsOfAnAddon& addon);

    [[nodiscard]] const std::vector<DocumentsOfAnAddon>& WhatToShow() const;

    [[nodiscard]] std::vector<DocumentGroup> TheDocuments() const;

    [[nodiscard]] DocumentGroup TheManualGroup() const;

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

    [[nodiscard]] std::optional<PendingPage> TakeThePendingPage();

    static void WriteThePage(AppSettings& settings, const PendingPage& turned);

    const DocumentService& documents_;
    SceneryService& scenery_;
    Session& session_;
    DocumentIndexCache& cache_;
    ManualSource& manual_;
    ManualDelivery manualDelivery_;
    const Clock& clock_;

    std::vector<DocumentsOfAnAddon> indexed_{};
    std::vector<DocumentsOfAnAddon> arriving_{};
    std::optional<std::chrono::system_clock::time_point> readAt_{};
    bool itWasRead_ = false;
    GuardedRunner reading_;
    std::atomic<bool> stop_ = false;
    std::size_t documentLines_ = 0;
    std::size_t chartLines_ = 0;
    std::optional<PendingPage> pendingPage_{};
    QTimer quietAfterTheLastTurn_;
    std::string language_{};
    ManualState manualState_ = ManualState::NotHere;
    QString manualFailure_{};
};

#endif // FS_ORGANIZER_VIEWMODEL_DOCUMENTS_VIEW_MODEL_H
