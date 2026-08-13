#include "viewmodel/AddonDocumentsViewModel.h"

#include <memory>
#include <utility>
#include <vector>

#include "domain/tree/LibraryLookup.h"

AddonDocumentsViewModel::AddonDocumentsViewModel(const DocumentService& documents,
                                                 SceneryService& scenery,
                                                 Session& session,
                                                 BackgroundRunner& runner,
                                                 QObject* parent)
    : QObject(parent), documents_(documents), scenery_(scenery), session_(session), runner_(runner)
{
}

void AddonDocumentsViewModel::Read(const std::filesystem::path& folder)
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

std::size_t AddonDocumentsViewModel::Documents() const
{
    return indexed_.documents.size();
}

std::size_t AddonDocumentsViewModel::Charts() const
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
