#include "viewmodel/DocumentsViewModel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <QtCore/QStringList>

#include "application/ManualCopy.h"
#include "domain/support/CaseFolding.h"
#include "domain/support/PathUtils.h"

namespace
{
    constexpr auto kInformation = "AOI";
    constexpr auto kTheProgramItself = "fs-organizer";
    constexpr auto kTheManual = "manual";
    const QString kSeparator = QString::fromUtf8(" · ");

    [[nodiscard]] QString WhatTheManualStateMeans(const ManualState state)
    {
        switch (state)
        {
        case ManualState::NotHere: return DocumentsViewModel::tr("not downloaded");
        case ManualState::Fetching: return DocumentsViewModel::tr("downloading…");
        case ManualState::Failed: return DocumentsViewModel::tr("download failed");
        case ManualState::Here: break;
        }

        return {};
    }

    [[nodiscard]] QString WhatTheTypeMeans(const std::string& type)
    {
        if (type == "AFC")
        {
            return DocumentsViewModel::tr("Terminal area, frequencies");
        }

        if (type == "AGC")
        {
            return DocumentsViewModel::tr("Ground movement");
        }

        if (type == "APC")
        {
            return DocumentsViewModel::tr("Aprons and stands");
        }

        if (type == "LVC")
        {
            return DocumentsViewModel::tr("Low visibility routes");
        }

        if (type == "MRC")
        {
            return DocumentsViewModel::tr("Minimum radar altitudes");
        }

        if (type == kInformation)
        {
            return DocumentsViewModel::tr("Operating information");
        }

        if (type == "IAC")
        {
            return DocumentsViewModel::tr("Approach charts");
        }

        if (type == "SID")
        {
            return DocumentsViewModel::tr("Departures");
        }

        if (type == "STAR")
        {
            return DocumentsViewModel::tr("Arrivals");
        }

        if (type == "VAC")
        {
            return DocumentsViewModel::tr("Visual charts");
        }

        if (type == "HELI")
        {
            return DocumentsViewModel::tr("Heliport charts");
        }

        return {};
    }

    [[nodiscard]] QString TitleOfTheType(const ChartsOfAType& type)
    {
        if (type.type.empty())
        {
            return DocumentsViewModel::tr("Charts with no type");
        }

        return QString::fromStdString(type.type);
    }

    [[nodiscard]] QString NameOf(const ChartEntry& chart, const ChartsOfAType& type)
    {
        if (chart.revision == ChartRevision::Previous)
        {
            return DocumentsViewModel::tr("Previous edition");
        }

        if (chart.name == type.type || chart.name.empty())
        {
            if (const QString meaning = WhatTheTypeMeans(type.type); !meaning.isEmpty())
            {
                return meaning;
            }
        }

        if (!chart.name.empty())
        {
            return QString::fromStdString(chart.name);
        }

        return QString::fromStdString(AsUtf8(chart.pages.front().stem()));
    }

    [[nodiscard]] QString DetailOf(const ChartEntry& chart)
    {
        if (chart.pages.size() < 2)
        {
            return {};
        }

        return DocumentsViewModel::tr("%n page(s)", nullptr, static_cast<int>(chart.pages.size()));
    }

    [[nodiscard]] QString CaptionOf(const QStringList& parts)
    {
        QStringList said;

        for (const QString& part : parts)
        {
            if (part.isEmpty() || said.contains(part))
            {
                continue;
            }

            said.append(part);
        }

        return said.join(kSeparator);
    }

    [[nodiscard]] QString TheAddonThatSaysMoreThanTheCode(const std::string& folderName, const std::string& code)
    {
        if (LoweredForComparison(folderName).find(LoweredForComparison(code)) != std::string::npos)
        {
            return {};
        }

        return QString::fromStdString(folderName);
    }

    [[nodiscard]] QString HowTheTypeIsSaid(const ChartsOfAType& type)
    {
        const QString meaning = WhatTheTypeMeans(type.type);

        if (meaning.isEmpty())
        {
            return TitleOfTheType(type);
        }

        return meaning;
    }

    [[nodiscard]] std::size_t LinesUnder(const DocumentGroup& group)
    {
        std::size_t lines = group.lines.size();

        for (const DocumentGroup& within : group.groups)
        {
            lines += LinesUnder(within);
        }

        return lines;
    }

    void GatherTheFavouritesOf(const DocumentGroup& group, std::vector<DocumentLine>& favourites)
    {
        for (const DocumentLine& line : group.lines)
        {
            if (!line.favourite)
            {
                continue;
            }

            DocumentLine gathered = line;
            gathered.detail = line.locator;

            favourites.push_back(std::move(gathered));
        }

        for (const DocumentGroup& within : group.groups)
        {
            GatherTheFavouritesOf(within, favourites);
        }
    }

    [[nodiscard]] DocumentGroup& TheTypeNamed(DocumentGroup& bucket, const QString& name)
    {
        for (DocumentGroup& within : bucket.groups)
        {
            if (within.name == name)
            {
                return within;
            }
        }

        bucket.groups.push_back({.name = name});

        return bucket.groups.back();
    }

    [[nodiscard]] bool ItCarriesSomething(const DocumentsOfAnAddon& addon)
    {
        return !addon.itWasWalked || !addon.documents.empty() || !addon.airports.empty();
    }

    [[nodiscard]] bool ItPutsALineOnTheScreen(const DocumentsOfAnAddon& addon)
    {
        return !addon.documents.empty() || !addon.airports.empty();
    }
}

DocumentsViewModel::DocumentsViewModel(const DocumentService& documents,
                                       SceneryService& scenery,
                                       Session& session,
                                       BackgroundRunner& runner,
                                       DocumentIndexCache& cache,
                                       ManualSource& manual,
                                       const Clock& clock,
                                       QObject* parent)
    : QObject(parent),
      documents_(documents),
      scenery_(scenery),
      session_(session),
      runner_(runner),
      cache_(cache),
      manual_(manual),
      clock_(clock)
{
    manual_.AddObserver(this);
}

DocumentsViewModel::~DocumentsViewModel()
{
    manual_.RemoveObserver(this);
}

void DocumentsViewModel::TheInterfaceSpeaks(const std::string& language)
{
    language_ = ManualLanguageFor(language);

    if (manualState_ != ManualState::Fetching)
    {
        manualState_ = manual_.TheManualIsHere(language_) ? ManualState::Here : ManualState::NotHere;
        manualFailure_.clear();
    }

    emit TheManualChanged();
}

ManualState DocumentsViewModel::TheManualIs() const
{
    return manualState_;
}

QString DocumentsViewModel::WhatHappenedToTheManual() const
{
    return manualFailure_;
}

void DocumentsViewModel::FetchTheManual()
{
    if (manualState_ == ManualState::Fetching)
    {
        return;
    }

    if (manual_.TheManualIsHere(language_))
    {
        manualState_ = ManualState::Here;
        manualFailure_.clear();

        emit TheManualChanged();

        return;
    }

    manualState_ = ManualState::Fetching;
    manualFailure_.clear();

    emit TheManualChanged();

    manual_.FetchTheManual(language_);
}

void DocumentsViewModel::OnManualFetched(const bool ok, const std::filesystem::path& file, const std::string& error)
{
    static_cast<void>(file);

    manualState_ = ok ? ManualState::Here : ManualState::Failed;
    manualFailure_ = ok ? QString() : QString::fromStdString(error);

    emit TheManualChanged();
}

bool DocumentsViewModel::ItIsTheManual(const DocumentLine& line) const
{
    return line.addon == kTheProgramItself && AsUtf8(line.document) == kTheManual;
}

DocumentLine DocumentsViewModel::TheManualLine() const
{
    DocumentLine line{.name = tr("Manual", "the user manual"),
                      .detail = WhatTheManualStateMeans(manualState_),
                      .caption = tr("FS Organizer manual"),
                      .addon = kTheProgramItself,
                      .document = PathFromUtf8(kTheManual),
                      .file = manual_.WhereTheManualWouldBe(language_),
                      .kind = DocumentKind::Document};

    line.favourite = ItIsAFavourite(line);

    return line;
}

DocumentGroup DocumentsViewModel::TheManualGroup() const
{
    DocumentGroup group{.name = QStringLiteral("FS Organizer")};
    group.lines.push_back(TheManualLine());

    return group;
}

void DocumentsViewModel::ShowWhatWasKept()
{
    const std::optional<RememberedDocuments> known = cache_.Remember();

    if (!known.has_value())
    {
        return;
    }

    indexed_ = known->addons;
    readAt_ = known->readAt;
    itWasRead_ = true;

    emit Indexed();
}

std::vector<DocumentsOfAnAddon> DocumentsViewModel::WhatEachAddonCarries(const std::vector<Library>& libraries,
                                                                         const std::vector<AddonToRead>& addons,
                                                                         bool& stopped)
{
    const std::vector<SceneryOfAnAddon> scenery = scenery_.SceneryOfEach(addons,
                                                                         [this](std::size_t, std::size_t)
                                                                         {
                                                                             return !stop_;
                                                                         });

    return documents_.IndexWhile(
        libraries, AirportsOfEachAddon(scenery),
        [this, &stopped](const DocumentsOfAnAddon& addon, const std::size_t indexed, const std::size_t outOf)
        {
            QMetaObject::invokeMethod(this,
                                      [this, addon, indexed, outOf]
                                      {
                                          TakeTheAddonThatArrived(addon);

                                          emit Progressed(static_cast<int>(indexed), static_cast<int>(outOf));
                                      });

            stopped = stop_;

            return !stop_;
        });
}

void DocumentsViewModel::TakeTheAddonThatArrived(const DocumentsOfAnAddon& addon)
{
    arriving_.push_back(addon);

    if (!indexed_.empty() || !ItPutsALineOnTheScreen(addon))
    {
        return;
    }

    emit Arrived();
}

const std::vector<DocumentsOfAnAddon>& DocumentsViewModel::WhatToShow() const
{
    return indexed_.empty() ? arriving_ : indexed_;
}

void DocumentsViewModel::TakeWhatWasRead(std::vector<DocumentsOfAnAddon>& found, const bool stopped)
{
    reading_ = false;

    emit ReadingChanged();

    if (stopped)
    {
        return;
    }

    indexed_.clear();
    arriving_.clear();

    for (DocumentsOfAnAddon& addon : found)
    {
        if (ItCarriesSomething(addon))
        {
            indexed_.push_back(std::move(addon));
        }
    }

    readAt_ = clock_.Now();
    itWasRead_ = true;

    cache_.Keep({.readAt = *readAt_, .addons = indexed_});

    emit Indexed();
}

void DocumentsViewModel::ReadTheLibrary()
{
    if (reading_)
    {
        return;
    }

    reading_ = true;
    stop_ = false;
    arriving_.clear();

    emit ReadingChanged();

    const std::vector<Library> libraries = session_.Profile().libraries;
    const std::vector<AddonToRead> addons = SceneryService::AddonsOf(session_.Profile(), session_.Snapshot());

    auto found = std::make_shared<std::vector<DocumentsOfAnAddon>>();
    auto stopped = std::make_shared<bool>(false);

    runner_.Run(
        [this, libraries, addons, found, stopped]
        {
            *found = WhatEachAddonCarries(libraries, addons, *stopped);
        },
        [this, found, stopped]
        {
            TakeWhatWasRead(*found, *stopped);
        });
}

void DocumentsViewModel::Stop()
{
    stop_ = true;
}

bool DocumentsViewModel::Reading() const
{
    return reading_;
}

bool DocumentsViewModel::ItWasRead() const
{
    return itWasRead_;
}

std::optional<std::chrono::system_clock::time_point> DocumentsViewModel::ReadAt() const
{
    return readAt_;
}

DocumentLine DocumentsViewModel::LineOfADocument(const DocumentsOfAnAddon& addon,
                                                 const std::filesystem::path& document) const
{
    const QString name = QString::fromStdString(AsUtf8(document.stem()));
    const QString locator = QString::fromStdString(addon.addon.folderName);

    DocumentLine line{.name = name,
                      .detail = {},
                      .caption = CaptionOf({locator, name}),
                      .locator = locator,
                      .addon = addon.addon.folderName,
                      .document = document,
                      .file = PathUnder(addon.folder, document),
                      .kind = DocumentKind::Document,
                      .favourite = false};

    line.favourite = ItIsAFavourite(line);

    if (const int page = PageOf(line); page > 0)
    {
        line.detail = tr("p. %1").arg(page + 1);
    }

    return line;
}

DocumentLine DocumentsViewModel::LineOfAChart(const DocumentsOfAnAddon& addon,
                                              const QString& locator,
                                              const ChartsOfAType& type,
                                              const ChartEntry& chart) const
{
    const QString name = NameOf(chart, type);

    DocumentLine line{.name = name,
                      .detail = DetailOf(chart),
                      .caption = CaptionOf({locator, HowTheTypeIsSaid(type), name}),
                      .locator = locator,
                      .addon = addon.addon.folderName,
                      .document = chart.pages.front(),
                      .file = PathUnder(addon.folder, chart.pages.front()),
                      .kind = DocumentKind::Chart,
                      .favourite = false};

    line.favourite = ItIsAFavourite(line);

    return line;
}

DocumentGroup DocumentsViewModel::TheChartsOf(const DocumentsOfAnAddon& addon, const ChartsOfAnAirport& airport) const
{
    const QString code = QString::fromStdString(airport.code);

    DocumentGroup group{.name = code, .aside = TheAddonThatSaysMoreThanTheCode(addon.addon.folderName, airport.code)};

    for (const ChartsOfAType& type : airport.types)
    {
        DocumentGroup within{.name = TitleOfTheType(type), .aside = WhatTheTypeMeans(type.type)};

        for (const ChartEntry& chart : type.charts)
        {
            within.lines.push_back(LineOfAChart(addon, code, type, chart));
        }

        within.count = QString::number(within.lines.size());

        group.groups.push_back(std::move(within));
    }

    group.count = QString::number(LinesUnder(group));

    return group;
}

std::vector<DocumentGroup> DocumentsViewModel::TheDocuments() const
{
    std::vector<DocumentGroup> groups;

    for (const DocumentsOfAnAddon& addon : WhatToShow())
    {
        if (addon.documents.empty())
        {
            continue;
        }

        DocumentGroup group{.name = QString::fromStdString(addon.addon.folderName)};

        for (const std::filesystem::path& document : addon.documents)
        {
            group.lines.push_back(LineOfADocument(addon, document));
        }

        if (group.lines.size() > 1)
        {
            group.count = QString::number(group.lines.size());
        }

        groups.push_back(std::move(group));
    }

    std::ranges::sort(groups, {}, &DocumentGroup::name);

    return groups;
}

std::vector<DocumentGroup> DocumentsViewModel::TheCharts() const
{
    std::vector<DocumentGroup> groups;
    DocumentGroup loose{.name = tr("Charts with no airport")};

    for (const DocumentsOfAnAddon& addon : WhatToShow())
    {
        for (const ChartsOfAnAirport& airport : addon.airports)
        {
            if (!airport.code.empty())
            {
                groups.push_back(TheChartsOf(addon, airport));

                continue;
            }

            const QString locator = QString::fromStdString(addon.addon.folderName);

            for (const ChartsOfAType& type : airport.types)
            {
                DocumentGroup& within = TheTypeNamed(loose, TitleOfTheType(type));
                within.aside = WhatTheTypeMeans(type.type);

                for (const ChartEntry& chart : type.charts)
                {
                    DocumentLine line = LineOfAChart(addon, locator, type, chart);
                    line.detail = locator;

                    within.lines.push_back(std::move(line));
                }

                within.count = QString::number(within.lines.size());
            }
        }
    }

    std::ranges::sort(groups, {}, &DocumentGroup::name);

    if (!loose.groups.empty())
    {
        loose.count = QString::number(LinesUnder(loose));
        groups.push_back(std::move(loose));
    }

    return groups;
}

DocumentGroup DocumentsViewModel::TheFavouritesAmong(const std::vector<DocumentGroup>& groups)
{
    DocumentGroup favourites{.name = tr("Favourites")};

    for (const DocumentGroup& group : groups)
    {
        GatherTheFavouritesOf(group, favourites.lines);
    }

    favourites.count = QString::number(favourites.lines.size());

    return favourites;
}

std::vector<DocumentGroup> DocumentsViewModel::GroupsOf(const DocumentPanel panel) const
{
    std::vector<DocumentGroup> groups = panel == DocumentPanel::Documents ? TheDocuments() : TheCharts();

    if (panel == DocumentPanel::Documents)
    {
        groups.insert(groups.begin(), TheManualGroup());
    }

    DocumentGroup favourites = TheFavouritesAmong(groups);

    if (favourites.lines.empty())
    {
        return groups;
    }

    groups.insert(groups.begin(), std::move(favourites));

    return groups;
}

std::size_t DocumentsViewModel::CountOf(const DocumentPanel panel) const
{
    std::size_t lines = 0;

    for (const DocumentGroup& group : panel == DocumentPanel::Documents ? TheDocuments() : TheCharts())
    {
        lines += LinesUnder(group);
    }

    return lines;
}

std::optional<DocumentPlace> DocumentsViewModel::WhereToFind(const std::string& addon) const
{
    for (const DocumentsOfAnAddon& indexed : WhatToShow())
    {
        if (indexed.addon.folderName != addon)
        {
            continue;
        }

        if (!indexed.documents.empty())
        {
            return DocumentPlace{.panel = DocumentPanel::Documents, .group = QString::fromStdString(addon)};
        }

        if (!indexed.airports.empty())
        {
            return DocumentPlace{.panel = DocumentPanel::Charts,
                                 .group = TheChartsOf(indexed, indexed.airports.front()).name};
        }
    }

    return std::nullopt;
}

const ReadDocument* DocumentsViewModel::Remembered(const DocumentLine& line) const
{
    const std::string named = AsUtf8(line.document);

    for (const ReadDocument& known : session_.Settings().documents)
    {
        if (known.addon == line.addon && known.document == named)
        {
            return &known;
        }
    }

    return nullptr;
}

void DocumentsViewModel::Remember(const DocumentLine& line, const std::function<void(ReadDocument&)>& change)
{
    const std::string addon = line.addon;
    const std::string named = AsUtf8(line.document);

    session_.Rewrite(
        [&addon, &named, &change](AppSettings& settings)
        {
            for (ReadDocument& known : settings.documents)
            {
                if (known.addon == addon && known.document == named)
                {
                    change(known);

                    return true;
                }
            }

            ReadDocument fresh{.addon = addon, .document = named};
            change(fresh);
            settings.documents.push_back(std::move(fresh));

            return true;
        });
}

bool DocumentsViewModel::ItIsAFavourite(const DocumentLine& line) const
{
    const ReadDocument* known = Remembered(line);

    return known != nullptr && known->favourite;
}

void DocumentsViewModel::Favour(const DocumentLine& line, const bool favourite)
{
    Remember(line,
             [favourite](ReadDocument& known)
             {
                 known.favourite = favourite;
             });
}

int DocumentsViewModel::PageOf(const DocumentLine& line) const
{
    const ReadDocument* known = Remembered(line);

    return known == nullptr ? 0 : known->page;
}

void DocumentsViewModel::RememberThePage(const DocumentLine& line, const int page)
{
    if (PageOf(line) == page)
    {
        return;
    }

    Remember(line,
             [page](ReadDocument& known)
             {
                 known.page = page;
             });
}

std::vector<DocumentBookmark> DocumentsViewModel::BookmarksOf(const DocumentLine& line) const
{
    const ReadDocument* known = Remembered(line);

    if (known == nullptr)
    {
        return {};
    }

    return known->bookmarks;
}

void DocumentsViewModel::MarkThePage(const DocumentLine& line, const int page, const bool marked)
{
    Remember(line,
             [page, marked](ReadDocument& known)
             {
                 const auto where = std::ranges::find_if(known.bookmarks,
                                                         [page](const DocumentBookmark& mark)
                                                         {
                                                             return mark.page == page;
                                                         });

                 if (!marked)
                 {
                     if (where != known.bookmarks.end())
                     {
                         known.bookmarks.erase(where);
                     }

                     return;
                 }

                 if (where != known.bookmarks.end())
                 {
                     return;
                 }

                 known.bookmarks.insert(std::ranges::upper_bound(known.bookmarks, page, {}, &DocumentBookmark::page),
                                        DocumentBookmark{.page = page, .name = {}});
             });
}

void DocumentsViewModel::NameTheBookmark(const DocumentLine& line, const int page, const std::string& name)
{
    Remember(line,
             [page, &name](ReadDocument& known)
             {
                 for (DocumentBookmark& mark : known.bookmarks)
                 {
                     if (mark.page == page)
                     {
                         mark.name = name;

                         return;
                     }
                 }
             });
}

ReadingGestures DocumentsViewModel::TheGesturesOf(const DocumentKind kind) const
{
    return kind == DocumentKind::Chart ? session_.Settings().onCharts : session_.Settings().onDocuments;
}

void DocumentsViewModel::MakeTheWheelZoom(const DocumentKind kind, const bool zooming)
{
    session_.Rewrite(
        [kind, zooming](AppSettings& settings)
        {
            ReadingGestures& gestures = kind == DocumentKind::Chart ? settings.onCharts : settings.onDocuments;

            if (gestures.wheelZooms == zooming)
            {
                return false;
            }

            gestures.wheelZooms = zooming;

            return true;
        });
}

void DocumentsViewModel::MakeTheDragMoveThePage(const DocumentKind kind, const bool moving)
{
    session_.Rewrite(
        [kind, moving](AppSettings& settings)
        {
            ReadingGestures& gestures = kind == DocumentKind::Chart ? settings.onCharts : settings.onDocuments;

            if (gestures.dragMovesThePage == moving)
            {
                return false;
            }

            gestures.dragMovesThePage = moving;

            return true;
        });
}
