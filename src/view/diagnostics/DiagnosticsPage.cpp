#include "view/diagnostics/DiagnosticsPage.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "domain/model/RecycleLimits.h"
#include "support/MomentText.h"
#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/CommunityModel.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr int kRailWidth = 210;
    constexpr int kBytesRole = Qt::UserRole + 1;

    class MeasuredRow final : public QTreeWidgetItem
    {
    public:
        using QTreeWidgetItem::QTreeWidgetItem;

        [[nodiscard]] bool operator<(const QTreeWidgetItem& other) const override
        {
            const int column = treeWidget() == nullptr ? 0 : treeWidget()->sortColumn();
            if (column != 2)
            {
                return QTreeWidgetItem::operator<(other);
            }

            return data(column, kBytesRole).toULongLong() < other.data(column, kBytesRole).toULongLong();
        }
    };

    std::size_t CountAddons(const MeasuredNode& node)
    {
        if (node.kind == TreeNodeKind::Addon)
        {
            return 1;
        }

        std::size_t addons = 0;
        for (const MeasuredNode& child : node.children)
        {
            addons += CountAddons(child);
        }

        return addons;
    }

    QString NameOf(const MeasuredNode& node)
    {
        const std::filesystem::path shown = node.kind == TreeNodeKind::Library ? node.path : node.path.filename();

        return AsText(shown);
    }

    MeasuredRow* RowFor(const MeasuredNode& node)
    {
        auto* row = new MeasuredRow;
        row->setText(0, NameOf(node));
        row->setText(1, node.kind == TreeNodeKind::Addon ? QString() : QString::number(CountAddons(node)));
        row->setText(2, node.measured ? AsSize(node.bytes) : QObject::tr("not measured"));
        row->setData(2, kBytesRole, static_cast<qulonglong>(node.measured ? node.bytes : 0));
        row->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        row->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        row->setData(1, QuietRole, true);
        row->setData(2, QuietRole, true);

        for (const MeasuredNode& child : node.children)
        {
            row->addChild(RowFor(child));
        }

        return row;
    }

    QLabel* Quiet(const QString& text, QWidget* parent)
    {
        auto* label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("PanelPromise"));
        label->setWordWrap(true);

        return label;
    }

    QLabel* QuietOnOneLine(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(QStringLiteral("PanelPromise"));

        return label;
    }

    void DressTheRowsOf(QTreeWidget* tree)
    {
        tree->setItemDelegate(new RowDelegate(tree));
    }

    QVBoxLayout* InsetLikeAToolbar()
    {
        auto* inset = new QVBoxLayout;
        inset->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
        inset->setSpacing(8);

        return inset;
    }

    QVBoxLayout* AroundTheTableOf(QWidget* pane)
    {
        auto* layout = new QVBoxLayout(pane);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        return layout;
    }
}

DiagnosticsPage::DiagnosticsPage(DiagnosticsViewModel& viewModel, QWidget* parent)
    : QWidget(parent), viewModel_(viewModel)
{
    panes_ = new QStackedWidget(this);
    panes_->addWidget(CreateCountsPane());
    panes_->addWidget(CreateBrokenPane());
    panes_->addWidget(CreateQuarantinePane());
    panes_->addWidget(CreateSizePane());
    panes_->addWidget(CreateSceneryPane());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(CreateRail());
    body->addWidget(panes_, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateToolbar());
    layout->addLayout(body, 1);

    connect(rail_, &QListWidget::currentRowChanged, this, &DiagnosticsPage::OpenSection);
    connect(refresh_, &QPushButton::clicked, &viewModel_, &DiagnosticsViewModel::Show);
    connect(measureAgain_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.MeasureSizeAgain();
                DressTheSizeToolbar();
            });
    connect(cancel_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.CancelSize();
                emit StatusChanged(tr("Stopping the measurement after the addon being walked now."));
            });
    connect(openQuarantine_, &QPushButton::clicked, this, &DiagnosticsPage::QuarantineRequested);
    connect(repair_, &QPushButton::clicked, this, &DiagnosticsPage::RepairRequested);
    connect(&viewModel_, &DiagnosticsViewModel::Counted, this, &DiagnosticsPage::ShowWhatWasCounted);
    connect(&viewModel_, &DiagnosticsViewModel::SizeMeasured, this, &DiagnosticsPage::ShowWhatWasMeasured);
    connect(&viewModel_, &DiagnosticsViewModel::SizeProgressed, this, &DiagnosticsPage::ShowProgress);
    connect(readSceneryAgain_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.ReadTheSceneryAgain();
                DressTheSceneryToolbar();
            });
    connect(stopScenery_, &QPushButton::clicked, this,
            [this]
            {
                viewModel_.CancelScenery();
                emit StatusChanged(tr("Stopping the reading after the addon being read now."));
            });
    connect(&viewModel_, &DiagnosticsViewModel::SceneryRead, this, &DiagnosticsPage::ShowWhatTheSceneryCarries);
    connect(&viewModel_, &DiagnosticsViewModel::SceneryProgressed, this, &DiagnosticsPage::ShowSceneryProgress);

    RetranslateUi();
    rail_->setCurrentRow(DestinationEntries);
    ShowWhatWasCounted();
    ShowWhatWasMeasured();
}

void DiagnosticsPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
        ShowWhatWasCounted();
        ShowWhatWasMeasured();
    }

    QWidget::changeEvent(event);
}

QWidget* DiagnosticsPage::CreateToolbar()
{
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("PageToolbar"));

    refresh_ = new QPushButton(toolbar);
    refresh_->setObjectName(QStringLiteral("PrimaryButton"));
    refreshedAt_ = QuietOnOneLine(toolbar);

    auto* bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    bar->setSpacing(8);
    bar->addWidget(refresh_);
    bar->addWidget(refreshedAt_);
    bar->addStretch();

    return toolbar;
}

QWidget* DiagnosticsPage::CreateRail()
{
    rail_ = new QListWidget(this);
    rail_->setObjectName(QStringLiteral("SectionRail"));
    rail_->setFixedWidth(kRailWidth);
    rail_->setFrameShape(QFrame::NoFrame);
    rail_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    for (int section = DestinationEntries; section <= AirportsInTheScenery; ++section)
    {
        rail_->addItem(QString());
    }

    return rail_;
}

QWidget* DiagnosticsPage::CreateCountsPane()
{
    auto* pane = new QWidget(this);

    counts_ = new QTreeWidget(pane);
    counts_->setObjectName(QStringLiteral("DiagnosticsCounts"));
    counts_->setRootIsDecorated(false);
    counts_->setUniformRowHeights(true);
    counts_->setColumnCount(2);
    counts_->header()->setStretchLastSection(true);
    DressTheHeaderOf(counts_->header());
    DressTheRowsOf(counts_);

    AroundTheTableOf(pane)->addWidget(counts_, 1);

    return pane;
}

QWidget* DiagnosticsPage::CreateBrokenPane()
{
    auto* pane = new QWidget(this);

    troubled_ = new QTreeWidget(pane);
    troubled_->setObjectName(QStringLiteral("DiagnosticsTroubled"));
    troubled_->setUniformRowHeights(true);
    troubled_->setColumnCount(2);
    troubled_->header()->setStretchLastSection(true);
    DressTheHeaderOf(troubled_->header());
    DressTheRowsOf(troubled_);

    repair_ = new QPushButton(pane);
    troubledPromise_ = Quiet({}, pane);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addWidget(repair_);
    actions->addStretch();

    auto* below = InsetLikeAToolbar();
    below->addLayout(actions);
    below->addWidget(troubledPromise_);

    auto* layout = AroundTheTableOf(pane);
    layout->addWidget(troubled_, 1);
    layout->addLayout(below);

    return pane;
}

QWidget* DiagnosticsPage::CreateQuarantinePane()
{
    auto* pane = new QWidget(this);

    quarantineWeight_ = new QLabel(pane);
    quarantineWeight_->setObjectName(QStringLiteral("PanelHeadline"));
    quarantinePlaces_ = Quiet({}, pane);
    openQuarantine_ = new QPushButton(pane);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addWidget(openQuarantine_);
    actions->addStretch();

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(8);
    layout->addWidget(quarantineWeight_);
    layout->addWidget(quarantinePlaces_);
    layout->addLayout(actions);
    layout->addStretch();

    return pane;
}

QWidget* DiagnosticsPage::CreateSizePane()
{
    auto* pane = new QWidget(this);

    sizeMeasuredAt_ = QuietOnOneLine(pane);
    sizeCost_ = QuietOnOneLine(pane);
    measureAgain_ = new QPushButton(pane);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(measureAgain_);
    header->addWidget(sizeCost_);
    header->addStretch();
    header->addWidget(sizeMeasuredAt_);

    sizeProgress_ = QuietOnOneLine(pane);
    sizeMeter_ = new QProgressBar(pane);
    sizeMeter_->setTextVisible(false);
    sizeMeter_->setFixedWidth(160);
    cancel_ = new QPushButton(pane);

    auto* progress = new QHBoxLayout;
    progress->setContentsMargins(0, 0, 0, 0);
    progress->addWidget(cancel_);
    progress->addWidget(sizeMeter_);
    progress->addWidget(sizeProgress_, 1);

    sizes_ = new QTreeWidget(pane);
    sizes_->setObjectName(QStringLiteral("DiagnosticsSizes"));
    sizes_->setUniformRowHeights(true);
    sizes_->setColumnCount(3);
    sizes_->setSortingEnabled(true);
    sizes_->sortByColumn(2, Qt::DescendingOrder);
    sizes_->header()->setStretchLastSection(false);
    sizes_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    sizes_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    sizes_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    DressTheHeaderOf(sizes_->header());
    DressTheRowsOf(sizes_);

    longestPaths_ = Quiet({}, pane);

    auto* above = InsetLikeAToolbar();
    above->addLayout(header);
    above->addLayout(progress);

    auto* below = InsetLikeAToolbar();
    below->addWidget(longestPaths_);

    auto* layout = AroundTheTableOf(pane);
    layout->addLayout(above);
    layout->addWidget(sizes_, 1);
    layout->addLayout(below);

    return pane;
}

QWidget* DiagnosticsPage::CreateSceneryPane()
{
    auto* pane = new QWidget(this);

    sceneryReadAt_ = QuietOnOneLine(pane);
    sceneryCost_ = QuietOnOneLine(pane);
    readSceneryAgain_ = new QPushButton(pane);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(readSceneryAgain_);
    header->addWidget(sceneryCost_);
    header->addStretch();
    header->addWidget(sceneryReadAt_);

    sceneryProgress_ = QuietOnOneLine(pane);
    sceneryMeter_ = new QProgressBar(pane);
    sceneryMeter_->setTextVisible(false);
    sceneryMeter_->setFixedWidth(160);
    stopScenery_ = new QPushButton(pane);

    auto* progress = new QHBoxLayout;
    progress->setContentsMargins(0, 0, 0, 0);
    progress->addWidget(stopScenery_);
    progress->addWidget(sceneryMeter_);
    progress->addWidget(sceneryProgress_, 1);

    scenery_ = new QTreeWidget(pane);
    scenery_->setUniformRowHeights(true);
    scenery_->setColumnCount(1);
    scenery_->header()->setStretchLastSection(true);
    DressTheHeaderOf(scenery_->header());
    DressTheRowsOf(scenery_);

    auto* above = InsetLikeAToolbar();
    above->addLayout(header);
    above->addLayout(progress);

    auto* layout = AroundTheTableOf(pane);
    layout->addLayout(above);
    layout->addWidget(scenery_, 1);

    return pane;
}

void DiagnosticsPage::RetranslateUi() const
{
    refresh_->setText(tr("Measure again"));
    counts_->setHeaderLabels({tr("Entry"), tr("How many")});
    troubled_->setHeaderLabels({tr("Entry"), tr("Points at")});
    repair_->setText(tr("Repair the broken links…"));
    openQuarantine_->setText(tr("Open Quarantine"));
    measureAgain_->setText(tr("Measure again"));
    cancel_->setText(tr("Stop"));
    sizes_->setHeaderLabels({tr("Category"), tr("Addons"), tr("Size")});
    sizeCost_->setText(tr("walks the whole tree, and that takes seconds"));
    readSceneryAgain_->setText(tr("Read them again"));
    stopScenery_->setText(tr("Stop"));
    sceneryCost_->setText(tr("opens the scenery folder of every addon, and that takes a moment"));
    scenery_->setHeaderLabels({tr("Addon")});

    ShowTheLongestPaths();
    DressTheRail();
}

void DiagnosticsPage::OpenSection(const int section) const
{
    panes_->setCurrentIndex(section);

    if (section == SizeOnDisk)
    {
        viewModel_.ShowSize();
        DressTheSizeToolbar();
    }

    if (section == AirportsInTheScenery)
    {
        viewModel_.ShowScenery();
        DressTheSceneryToolbar();
    }
}

void DiagnosticsPage::ShowWhatTheSceneryCarries() const
{
    const SceneryCensus& census = viewModel_.Scenery();

    scenery_->clear();

    const auto group = [this](const QString& title, const std::vector<QString>& addons)
    {
        auto* parent = new QTreeWidgetItem(scenery_);
        parent->setText(0, title);
        parent->setFirstColumnSpanned(true);

        for (const QString& addon : addons)
        {
            auto* row = new QTreeWidgetItem(parent);
            row->setText(0, addon);
        }

        parent->setExpanded(!addons.empty());
    };

    group(tr("Carrying an airport code · %1").arg(census.carryingACode.size()), census.carryingACode);
    group(tr("Carrying a record that did not decode · %1").arg(census.whoseRecordWasNotRead.size()),
          census.whoseRecordWasNotRead);
    group(tr("Carrying no airport record · %1").arg(census.carryingNoAirportRecord), {});

    DressTheSceneryToolbar();
    DressTheRail();
}

void DiagnosticsPage::ShowSceneryProgress(const int read, const int total) const
{
    sceneryMeter_->setRange(0, total);
    sceneryMeter_->setValue(read);
    sceneryProgress_->setText(tr("reading %1 of %2").arg(read).arg(total));
}

void DiagnosticsPage::DressTheSceneryToolbar() const
{
    const bool reading = viewModel_.ReadingTheScenery();

    sceneryMeter_->setVisible(reading);
    sceneryProgress_->setVisible(reading);
    stopScenery_->setVisible(reading);
    readSceneryAgain_->setEnabled(!reading);

    const std::optional<std::chrono::system_clock::time_point> read = viewModel_.SceneryReadAt();

    if (read.has_value())
    {
        sceneryReadAt_->setText(tr("read %1").arg(AsMoment(*read)));
        return;
    }

    sceneryReadAt_->setText(reading ? tr("reading now") : tr("not read yet"));
}

void DiagnosticsPage::ShowWhatWasCounted()
{
    ShowTheCounts();
    ShowWhatIsTroubled();
    ShowWhatTheQuarantineHolds();

    const std::optional<std::chrono::system_clock::time_point> counted = viewModel_.CountedAt();
    refreshedAt_->setText(counted.has_value() ? tr("everything under a second · %1").arg(AsMoment(*counted))
                                              : tr("not counted yet"));

    DressTheRail();

    std::size_t entries = 0;
    for (const ClassificationCount& row : viewModel_.Counts())
    {
        entries += row.count;
    }

    emit SummaryChanged(tr("%n entry in the destinations of this profile.", nullptr, static_cast<int>(entries)));
}

void DiagnosticsPage::ShowTheCounts() const
{
    counts_->clear();
    for (const ClassificationCount& row : viewModel_.Counts())
    {
        auto* item = new QTreeWidgetItem(counts_);
        item->setText(0, CommunityModel::ClassificationName(row.classification));
        item->setText(1, QString::number(row.count));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(1, QuietRole, true);
    }
}

void DiagnosticsPage::ShowWhatIsTroubled() const
{
    troubled_->clear();
    const std::vector<DestinationEntry>& broken = viewModel_.Broken();
    const std::vector<DestinationEntry>& unavailable = viewModel_.Unavailable();

    const auto group = [this](const QString& title, const std::vector<DestinationEntry>& entries)
    {
        auto* parent = new QTreeWidgetItem(troubled_);
        parent->setText(0, title);
        parent->setFirstColumnSpanned(true);

        for (const DestinationEntry& entry : entries)
        {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, AsText(entry.path));
            item->setText(1, AsText(entry.target));
            item->setData(1, QuietRole, true);
        }

        parent->setExpanded(true);
    };

    group(tr("Broken (%n)", nullptr, static_cast<int>(broken.size())), broken);
    group(tr("Unavailable (%n)", nullptr, static_cast<int>(unavailable.size())), unavailable);

    repair_->setEnabled(!broken.empty());
    troubledPromise_->setText(
        broken.empty() && unavailable.empty()
            ? tr("No entry in any destination is broken or parked on a volume that is not here.")
            : tr("An unavailable entry is not offered for cleanup: the volume can come back. Repairing points a broken "
                 "link at the addon again, and it is the same repair the Destinations screen runs."));
}

void DiagnosticsPage::ShowWhatTheQuarantineHolds() const
{
    const QuarantineWeight weight = viewModel_.Quarantine();

    quarantineWeight_->setText(tr("%1 held in quarantine").arg(AsSize(weight.bytes)));
    quarantinePlaces_->setText(tr("%1 beside a destination, %2 inside a library")
                                   .arg(static_cast<qulonglong>(weight.besideDestinations))
                                   .arg(static_cast<qulonglong>(weight.insideLibraries)));
}

void DiagnosticsPage::ShowWhatWasMeasured() const
{
    sizes_->setSortingEnabled(false);
    sizes_->clear();

    for (const MeasuredNode& library : viewModel_.Size().libraries)
    {
        auto* row = RowFor(library);
        sizes_->addTopLevelItem(row);
        row->setExpanded(true);
    }

    sizes_->setSortingEnabled(true);

    ShowTheLongestPaths();

    DressTheSizeToolbar();
    DressTheRail();
}

void DiagnosticsPage::ShowTheLongestPaths() const
{
    QStringList said;

    for (const MeasuredNode& library : viewModel_.Size().libraries)
    {
        if (!library.measured)
        {
            continue;
        }

        const QString measured =
            tr("%1 · longest path %2")
                .arg(AsText(library.path), tr("%n character", nullptr, static_cast<int>(library.longestEntry)));

        said << (TheRecycleBinReaches(library.longestEntry)
                     ? measured
                     : tr("%1, past the %2 the Recycle Bin stops at").arg(measured).arg(kTheRecycleBinStopsAt));
    }

    longestPaths_->setText(said.join(QStringLiteral("\n")));
    longestPaths_->setVisible(!said.isEmpty());
}

void DiagnosticsPage::ShowProgress(const QString& folder, const int measured, const int total) const
{
    sizeMeter_->setRange(0, total);
    sizeMeter_->setValue(measured);
    sizeProgress_->setText(tr("measuring %1").arg(folder));

    DressTheSizeToolbar();
}

void DiagnosticsPage::DressTheRail() const
{
    const QuarantineWeight weight = viewModel_.Quarantine();

    std::size_t entries = 0;
    for (const ClassificationCount& row : viewModel_.Counts())
    {
        entries += row.count;
    }

    const std::size_t troubled = viewModel_.Broken().size() + viewModel_.Unavailable().size();
    const std::optional<std::chrono::system_clock::time_point> measured = viewModel_.MeasuredAt();

    std::uintmax_t bytes = 0;
    for (const MeasuredNode& library : viewModel_.Size().libraries)
    {
        bytes += library.bytes;
    }

    rail_->item(DestinationEntries)->setText(tr("Destination entries · %1").arg(entries));
    rail_->item(BrokenAndUnavailable)->setText(tr("Broken, unavailable · %1").arg(troubled));
    rail_->item(Quarantine)->setText(tr("Quarantine · %1").arg(AsSize(weight.bytes)));
    rail_->item(SizeOnDisk)
        ->setText(measured.has_value() ? tr("Size on disk · %1").arg(AsSize(bytes)) : tr("Size on disk"));

    const SceneryCensus& census = viewModel_.Scenery();

    rail_->item(AirportsInTheScenery)
        ->setText(viewModel_.SceneryReadAt().has_value()
                      ? tr("Airports in the scenery · %1").arg(census.carryingACode.size())
                      : tr("Airports in the scenery"));
}

void DiagnosticsPage::DressTheSizeToolbar() const
{
    const bool measuring = viewModel_.Measuring();

    sizeMeter_->setVisible(measuring);
    sizeProgress_->setVisible(measuring);
    cancel_->setVisible(measuring);
    measureAgain_->setEnabled(!measuring);

    const std::optional<std::chrono::system_clock::time_point> measured = viewModel_.MeasuredAt();
    if (!measured.has_value())
    {
        sizeMeasuredAt_->setText(measuring ? tr("measuring now") : tr("not measured yet"));
        return;
    }

    sizeMeasuredAt_->setText(viewModel_.Size().complete
                                 ? tr("measured %1").arg(AsMoment(*measured))
                                 : tr("stopped %1, and these numbers are incomplete").arg(AsMoment(*measured)));
}
