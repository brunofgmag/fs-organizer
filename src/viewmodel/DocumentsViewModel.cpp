#include "viewmodel/DocumentsViewModel.h"

#include <memory>
#include <utility>

#include "domain/support/PathUtils.h"
#include "domain/tree/LibraryLookup.h"

namespace
{
    constexpr auto kInformation = "AOI";

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

        return {};
    }

    [[nodiscard]] QString TitleOf(const ChartsOfAnAirport& airport, const ChartsOfAType& type)
    {
        const QString named =
            type.type.empty() ? DocumentsViewModel::tr("Charts with no type") : QString::fromStdString(type.type);

        if (airport.code.empty())
        {
            return named;
        }

        return QString::fromStdString(airport.code) + QString::fromUtf8(" · ") + named;
    }

    [[nodiscard]] QString NameOf(const ChartEntry& chart, const ChartsOfAType& type)
    {
        if (chart.revision == ChartRevision::Previous)
        {
            return DocumentsViewModel::tr("The revision it superseded");
        }

        if (chart.name == type.type)
        {
            const QString meaning = WhatTheTypeMeans(type.type);

            if (!meaning.isEmpty())
            {
                return meaning;
            }
        }

        if (!chart.name.empty())
        {
            return QString::fromStdString(chart.name);
        }

        if (type.type == kInformation)
        {
            return DocumentsViewModel::tr("Operating information");
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
}

DocumentsViewModel::DocumentsViewModel(const DocumentService& documents,
                                       SceneryService& scenery,
                                       Session& session,
                                       BackgroundRunner& runner,
                                       QObject* parent)
    : QObject(parent), documents_(documents), scenery_(scenery), session_(session), runner_(runner)
{
}

void DocumentsViewModel::Read(const std::filesystem::path& folder)
{
    const AddonId addon = IdentityOf(session_.Profile(), folder);

    indexed_ = {.addon = addon, .folder = folder};

    auto found = std::make_shared<DocumentsOfAnAddon>();

    runner_.Run(
        [this, addon, folder, found]
        {
            const std::vector<AirportsOfAnAddon> airports =
                AirportsOfEachAddon({scenery_.SceneryOf({.addon = addon, .folder = folder})});

            *found = documents_.DocumentsOf(addon, folder,
                                            airports.empty() ? std::vector<std::string>{} : airports.front().codes);
        },
        [this, found]
        {
            indexed_ = std::move(*found);

            emit Indexed();
        });
}

std::size_t DocumentsViewModel::Documents() const
{
    return indexed_.documents.size();
}

std::size_t DocumentsViewModel::Charts() const
{
    std::size_t charts = 0;

    for (const ChartsOfAnAirport& airport : indexed_.airports)
    {
        for (const ChartsOfAType& type : airport.types)
        {
            for (const ChartEntry& chart : type.charts)
            {
                charts += chart.pages.size();
            }
        }
    }

    return charts;
}

const std::string& DocumentsViewModel::Addon() const
{
    return indexed_.addon.folderName;
}

const std::filesystem::path& DocumentsViewModel::Folder() const
{
    return indexed_.folder;
}

std::filesystem::path DocumentsViewModel::FullPathOf(const std::filesystem::path& document) const
{
    return PathUnder(indexed_.folder, document);
}

DocumentGroup DocumentsViewModel::TheManuals() const
{
    DocumentGroup manuals{.name = tr("Manuals"), .lines = {}};

    for (const std::filesystem::path& document : indexed_.documents)
    {
        manuals.lines.push_back({.name = QString::fromStdString(AsUtf8(document.stem())),
                                 .detail = {},
                                 .document = document,
                                 .favourite = ItIsAFavourite(document)});
    }

    return manuals;
}

DocumentGroup DocumentsViewModel::TheFavouritesAmong(const std::vector<DocumentGroup>& groups)
{
    DocumentGroup favourites{.name = tr("Favourites"), .lines = {}};

    for (const DocumentGroup& group : groups)
    {
        for (const DocumentLine& line : group.lines)
        {
            if (line.favourite)
            {
                favourites.lines.push_back(line);
            }
        }
    }

    return favourites;
}

std::vector<DocumentGroup> DocumentsViewModel::Groups() const
{
    std::vector<DocumentGroup> groups;

    if (!indexed_.documents.empty())
    {
        groups.push_back(TheManuals());
    }

    for (const ChartsOfAnAirport& airport : indexed_.airports)
    {
        for (const ChartsOfAType& type : airport.types)
        {
            DocumentGroup group{.name = TitleOf(airport, type), .lines = {}};

            for (const ChartEntry& chart : type.charts)
            {
                group.lines.push_back({.name = NameOf(chart, type),
                                       .detail = DetailOf(chart),
                                       .document = chart.pages.front(),
                                       .favourite = ItIsAFavourite(chart.pages.front())});
            }

            groups.push_back(std::move(group));
        }
    }

    DocumentGroup favourites = TheFavouritesAmong(groups);

    if (favourites.lines.empty())
    {
        return groups;
    }

    groups.insert(groups.begin(), std::move(favourites));

    return groups;
}

const ReadDocument* DocumentsViewModel::Remembered(const std::filesystem::path& document) const
{
    const std::string named = AsUtf8(document);

    for (const ReadDocument& known : session_.Settings().documents)
    {
        if (known.addon == indexed_.addon.folderName && known.document == named)
        {
            return &known;
        }
    }

    return nullptr;
}

void DocumentsViewModel::Remember(const std::filesystem::path& document,
                                  const std::function<void(ReadDocument&)>& change)
{
    const std::string addon = indexed_.addon.folderName;
    const std::string named = AsUtf8(document);

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

bool DocumentsViewModel::ItIsAFavourite(const std::filesystem::path& document) const
{
    const ReadDocument* known = Remembered(document);

    return known != nullptr && known->favourite;
}

void DocumentsViewModel::Favour(const std::filesystem::path& document, const bool favourite)
{
    Remember(document,
             [favourite](ReadDocument& known)
             {
                 known.favourite = favourite;
             });
}

int DocumentsViewModel::PageOf(const std::filesystem::path& document) const
{
    const ReadDocument* known = Remembered(document);

    return known == nullptr ? 0 : known->page;
}

void DocumentsViewModel::RememberThePage(const std::filesystem::path& document, const int page)
{
    if (PageOf(document) == page)
    {
        return;
    }

    Remember(document,
             [page](ReadDocument& known)
             {
                 known.page = page;
             });
}
