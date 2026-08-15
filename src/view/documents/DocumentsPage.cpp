#include "view/documents/DocumentsPage.h"

#include <QtCore/QEvent>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MomentText.h"
#include "support/PathText.h"
#include "view/documents/DocumentReader.h"
#include "view/panels/EmptyState.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"

namespace
{
    constexpr int kIndexWidth = 340;
    constexpr int kNarrowestIndex = 300;
    constexpr int kPageWidth = 1120;
    constexpr int kGlyphColumn = 0;
    constexpr int kNameColumn = 1;
    constexpr int kDetailColumn = 2;
    constexpr int kProgressHeight = 4;

    const auto kLineRole = Qt::UserRole;
    const QString kOneLevelIn = QString::fromUtf8("   ");
    const QString kSeparator = QString::fromUtf8(" · ");

    class WithoutTheFocusFrame final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

    protected:
        void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& line) const override
        {
            QStyledItemDelegate::initStyleOption(option, line);

            option->state &= ~QStyle::State_HasFocus;
        }
    };

    [[nodiscard]] QString StarOf(const bool favourite)
    {
        return favourite ? QString::fromUtf8("★") : QString::fromUtf8("☆");
    }

    [[nodiscard]] QString ArrowOf(const bool open)
    {
        return open ? QString::fromUtf8("▾") : QString::fromUtf8("▸");
    }

    [[nodiscard]] std::size_t Which(const DocumentPanel panel)
    {
        return static_cast<std::size_t>(panel);
    }

    [[nodiscard]] QString TheHeadingOf(const DocumentGroup& group, const int depth)
    {
        QString said = depth == 0 ? group.name : kOneLevelIn + group.name;

        if (!group.aside.isEmpty())
        {
            said += kSeparator + group.aside;
        }

        return said;
    }

    void RememberWhatIsOpen(const QTreeWidgetItem& heading, const QString& trail, QSet<QString>& opened)
    {
        const QString mine = trail + heading.text(kNameColumn);

        if (heading.isExpanded())
        {
            opened.insert(mine);
        }

        for (int within = 0; within < heading.childCount(); ++within)
        {
            RememberWhatIsOpen(*heading.child(within), mine, opened);
        }
    }
}

DocumentsPage::DocumentsPage(DocumentsViewModel& viewModel, QWidget* parent) : QWidget(parent), viewModel_(viewModel)
{
    split_ = new QSplitter(Qt::Horizontal, this);
    split_->addWidget(TheIndexSide());
    split_->addWidget(TheReadingSide());
    split_->setStretchFactor(0, 0);
    split_->setStretchFactor(1, 1);
    split_->setSizes({kIndexWidth, kPageWidth - kIndexWidth});

    nothingIndexed_ = new EmptyState(this);
    readTheLibrary_ = nothingIndexed_->OfferTheOnlyAction();

    split_->installEventFilter(this);

    body_ = new QStackedWidget(this);
    body_->addWidget(split_);
    body_->addWidget(nothingIndexed_);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(TheBar());
    column->addWidget(body_, 1);

    ConnectTheBar();
    ConnectTheIndex();
    ConnectTheReader();

    Retranslate();
    Rebuild();
    ShowWhatIsHappening();
    Show(DocumentPanel::Documents);
}

QWidget* DocumentsPage::TheBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("PageToolbar"));

    for (PanelOfTheTab& panel : panels_)
    {
        panel.button = new QPushButton(bar);
        panel.button->setCheckable(true);
        panel.button->setAutoDefault(false);
    }

    readAt_ = new QLabel(bar);
    readAt_->setObjectName(QStringLiteral("PanelPromise"));

    readAgain_ = new QPushButton(bar);
    readAgain_->setAutoDefault(false);

    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    row->setSpacing(8);
    row->addWidget(panels_[Which(DocumentPanel::Documents)].button);
    row->addWidget(panels_[Which(DocumentPanel::Charts)].button);
    row->addStretch();
    row->addWidget(readAt_);
    row->addWidget(readAgain_);

    return bar;
}

QTreeWidget* DocumentsPage::AnIndex(const QString& named)
{
    auto* index = new QTreeWidget(this);
    index->setObjectName(named);
    index->setColumnCount(3);
    index->setHeaderHidden(true);
    index->setRootIsDecorated(true);
    index->setIndentation(0);
    index->setUniformRowHeights(true);
    index->setTextElideMode(Qt::ElideMiddle);
    index->setSelectionBehavior(QAbstractItemView::SelectRows);
    index->setAllColumnsShowFocus(false);
    index->setItemDelegate(new WithoutTheFocusFrame(index));
    index->setMinimumWidth(kNarrowestIndex);
    index->header()->setSectionResizeMode(kGlyphColumn, QHeaderView::ResizeToContents);
    index->header()->setSectionResizeMode(kNameColumn, QHeaderView::Stretch);
    index->header()->setSectionResizeMode(kDetailColumn, QHeaderView::ResizeToContents);
    index->header()->setStretchLastSection(false);
    index->viewport()->installEventFilter(this);

    return index;
}

QWidget* DocumentsPage::TheIndexSide()
{
    lists_ = new QStackedWidget(this);

    const std::array<QString, 2> named{QStringLiteral("DocumentsIndex"), QStringLiteral("ChartsIndex")};

    for (std::size_t panel = 0; panel < panels_.size(); ++panel)
    {
        panels_[panel].index = AnIndex(named[panel]);
        lists_->addWidget(panels_[panel].index);
    }

    howFar_ = new QLabel(this);
    howFar_->setObjectName(QStringLiteral("PanelPromise"));

    meter_ = new QProgressBar(this);
    meter_->setTextVisible(false);
    meter_->setFixedHeight(kProgressHeight);

    stop_ = new QPushButton(this);
    stop_->setAutoDefault(false);

    progress_ = new QWidget(this);

    auto* below = new QVBoxLayout(progress_);
    below->setContentsMargins(0, 8, 0, 0);
    below->setSpacing(6);
    below->addWidget(howFar_);
    below->addWidget(meter_);
    below->addWidget(stop_, 0, Qt::AlignLeft);

    auto* side = new QWidget(this);

    auto* column = new QVBoxLayout(side);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(lists_, 1);
    column->addWidget(progress_);

    return side;
}

QWidget* DocumentsPage::TheReadingSide()
{
    reader_ = new DocumentReader(this);
    reader_->SayTheGesturesOf(DocumentKind::Chart, viewModel_.TheGesturesOf(DocumentKind::Chart));
    reader_->SayTheGesturesOf(DocumentKind::Document, viewModel_.TheGesturesOf(DocumentKind::Document));

    nothingOpen_ = new EmptyState(this);

    elsewhere_ = new EmptyState(this);
    bringItBack_ = elsewhere_->OfferTheOnlyAction();

    readingSide_ = new QStackedWidget(this);
    readingSide_->addWidget(reader_);
    readingSide_->addWidget(nothingOpen_);
    readingSide_->addWidget(elsewhere_);
    readingSide_->setCurrentWidget(nothingOpen_);

    return readingSide_;
}

void DocumentsPage::ConnectTheBar()
{
    connect(panels_[Which(DocumentPanel::Documents)].button, &QPushButton::clicked, this,
            [this]
            {
                Show(DocumentPanel::Documents);
            });
    connect(panels_[Which(DocumentPanel::Charts)].button, &QPushButton::clicked, this,
            [this]
            {
                Show(DocumentPanel::Charts);
            });

    for (QPushButton* asking : {readAgain_, readTheLibrary_})
    {
        connect(asking, &QPushButton::clicked, &viewModel_, &DocumentsViewModel::ReadTheLibrary);
    }

    connect(stop_, &QPushButton::clicked, &viewModel_, &DocumentsViewModel::Stop);
    connect(bringItBack_, &QPushButton::clicked, this, &DocumentsPage::BringTheReadingBack);
}

void DocumentsPage::ConnectTheIndex()
{
    connect(&viewModel_, &DocumentsViewModel::Indexed, this, &DocumentsPage::ShowTheIndex);
    connect(&viewModel_, &DocumentsViewModel::Arrived, this, &DocumentsPage::ShowTheIndex);
    connect(&viewModel_, &DocumentsViewModel::ReadingChanged, this,
            [this]
            {
                if (viewModel_.Reading())
                {
                    indexed_ = 0;
                    outOf_ = 0;
                }

                ShowWhatIsHappening();
            });
    connect(&viewModel_, &DocumentsViewModel::Progressed, this,
            [this](const int indexed, const int outOf)
            {
                indexed_ = static_cast<std::size_t>(indexed);
                outOf_ = static_cast<std::size_t>(outOf);

                ShowWhatIsHappening();
            });

    for (const PanelOfTheTab& panel : panels_)
    {
        connect(panel.index, &QTreeWidget::itemExpanded, this,
                [](QTreeWidgetItem* heading)
                {
                    heading->setText(kGlyphColumn, ArrowOf(true));
                });
        connect(panel.index, &QTreeWidget::itemCollapsed, this,
                [](QTreeWidgetItem* heading)
                {
                    heading->setText(kGlyphColumn, ArrowOf(false));
                });
        connect(panel.index, &QTreeWidget::itemActivated, this,
                [this](const QTreeWidgetItem* item)
                {
                    if (const DocumentLine* line = LineOf(showing_, item); line != nullptr)
                    {
                        Open(*line);
                    }
                });
    }
}

void DocumentsPage::ConnectTheReader()
{
    connect(reader_, &DocumentReader::ThePageChanged, this,
            [this](const int page)
            {
                if (open_.has_value())
                {
                    viewModel_.RememberThePage(*open_, page);
                }
            });
    connect(reader_, &DocumentReader::TheMarkOfThePageWasTurned, this,
            [this](const int page, const bool marked)
            {
                if (!open_.has_value())
                {
                    return;
                }

                viewModel_.MarkThePage(*open_, page, marked);
                reader_->ShowTheBookmarks(viewModel_.BookmarksOf(*open_));
            });
    connect(reader_, &DocumentReader::TheBookmarkWasNamed, this,
            [this](const int page, const QString& name)
            {
                if (!open_.has_value())
                {
                    return;
                }

                viewModel_.NameTheBookmark(*open_, page, name.toStdString());
                reader_->ShowTheBookmarks(viewModel_.BookmarksOf(*open_));
            });
    connect(reader_, &DocumentReader::TheWheelWasSetToZoom, this,
            [this](const DocumentKind kind, const bool zooming)
            {
                viewModel_.MakeTheWheelZoom(kind, zooming);
            });
    connect(reader_, &DocumentReader::TheDragWasSetToMoveThePage, this,
            [this](const DocumentKind kind, const bool moving)
            {
                viewModel_.MakeTheDragMoveThePage(kind, moving);
            });
    connect(reader_, &DocumentReader::TheFolderWasAskedFor, this,
            [this]
            {
                if (open_.has_value())
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(open_->file.parent_path())));
                }
            });
    connect(reader_, &DocumentReader::TheDetachWasAskedFor, this,
            [this]
            {
                if (window_ == nullptr)
                {
                    DetachTheReading();

                    return;
                }

                BringTheReadingBack();
            });
}

void DocumentsPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
        Rebuild();
        ShowWhatIsHappening();
    }

    QWidget::changeEvent(event);
}

bool DocumentsPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == split_ && event->type() == QEvent::Show && !settled_)
    {
        settled_ = true;

        split_->setSizes({kIndexWidth, split_->width() - kIndexWidth});
    }

    if (event->type() != QEvent::MouseButtonRelease)
    {
        return QWidget::eventFilter(watched, event);
    }

    for (const PanelOfTheTab& panel : panels_)
    {
        if (watched != panel.index->viewport())
        {
            continue;
        }

        const auto* click = static_cast<QMouseEvent*>(event);

        if (click->button() != Qt::LeftButton)
        {
            continue;
        }

        const DocumentPanel clicked =
            &panel == &panels_[Which(DocumentPanel::Documents)] ? DocumentPanel::Documents : DocumentPanel::Charts;

        if (AnswerTheClickOn(clicked, click->pos()))
        {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool DocumentsPage::AnswerTheClickOn(const DocumentPanel panel, const QPoint& where)
{
    QTreeWidget& index = *panels_[Which(panel)].index;
    QTreeWidgetItem* item = index.itemAt(where);

    if (item == nullptr)
    {
        return false;
    }

    const DocumentLine* line = LineOf(panel, item);

    if (line == nullptr)
    {
        if (!TheArrowOf(index, *item).contains(where))
        {
            return false;
        }

        item->setExpanded(!item->isExpanded());

        return true;
    }

    if (index.columnAt(where.x()) == kGlyphColumn)
    {
        TurnTheStarOf(panel, *line);

        return true;
    }

    Open(*line);

    return false;
}

const DocumentLine* DocumentsPage::LineOf(const DocumentPanel panel, const QTreeWidgetItem* item) const
{
    if (item == nullptr)
    {
        return nullptr;
    }

    const QVariant known = item->data(kNameColumn, kLineRole);

    if (!known.isValid())
    {
        return nullptr;
    }

    return &panels_[Which(panel)].lines[static_cast<std::size_t>(known.toInt())];
}

QRect DocumentsPage::TheArrowOf(const QTreeWidget& index, const QTreeWidgetItem& group)
{
    const QRect cell = index.visualItemRect(&group);

    QStyleOptionViewItem option;
    option.initFrom(&index);
    option.rect = cell;
    option.features = QStyleOptionViewItem::HasDisplay;
    option.text = group.text(kGlyphColumn);
    option.font = group.font(kGlyphColumn);
    option.fontMetrics = QFontMetrics(option.font);
    option.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;

    const QRect written = index.style()->subElementRect(QStyle::SE_ItemViewItemText, &option, &index);

    return {written.left(), cell.top(), option.fontMetrics.horizontalAdvance(option.text), cell.height()};
}

void DocumentsPage::TurnTheStarOf(const DocumentPanel panel, const DocumentLine& line)
{
    viewModel_.Favour(line, !viewModel_.ItIsAFavourite(line));

    RebuildTheIndexOf(panel);
    Retranslate();
}

void DocumentsPage::Open(const DocumentLine& line)
{
    if (open_.has_value() && open_->file == line.file)
    {
        return;
    }

    open_ = line;

    reader_->Read(line.file, viewModel_.PageOf(line), line.kind, viewModel_.BookmarksOf(line));
    reader_->SayItIsShowing(line.caption);

    if (window_ != nullptr)
    {
        window_->setWindowTitle(line.caption);
        elsewhere_->Retell(tr("Reading in a separate window"), line.caption);

        return;
    }

    readingSide_->setCurrentWidget(reader_);
}

void DocumentsPage::Show(const DocumentPanel panel)
{
    showing_ = panel;

    lists_->setCurrentWidget(panels_[Which(panel)].index);

    for (const PanelOfTheTab& shown : panels_)
    {
        const bool chosen = shown.index == panels_[Which(panel)].index;

        shown.button->setChecked(chosen);
        GiveItTheRole(shown.button, chosen ? QStringLiteral("primary") : QString());
    }
}

void DocumentsPage::Reveal(const std::string& addon)
{
    const std::optional<DocumentPlace> place = viewModel_.WhereToFind(addon);

    if (!place.has_value())
    {
        return;
    }

    Show(place->panel);

    QTreeWidget& index = *panels_[Which(place->panel)].index;

    for (int group = 0; group < index.topLevelItemCount(); ++group)
    {
        QTreeWidgetItem* heading = index.topLevelItem(group);

        if (heading->text(kNameColumn) != place->group)
        {
            continue;
        }

        heading->setExpanded(true);
        index.setCurrentItem(heading);
        index.scrollToItem(heading, QAbstractItemView::PositionAtTop);

        return;
    }
}

void DocumentsPage::DetachTheReading()
{
    if (window_ != nullptr)
    {
        return;
    }

    window_ = new QDialog(window());
    window_->setWindowTitle(open_.has_value() ? open_->caption : tr("Documents"));
    window_->setAttribute(Qt::WA_DeleteOnClose, false);

    auto* alone = new QVBoxLayout(window_);
    alone->setContentsMargins(0, 0, 0, 0);

    readingSide_->removeWidget(reader_);
    alone->addWidget(reader_);
    reader_->show();

    reader_->SayItIsDetached(true);

    connect(window_, &QDialog::finished, this, &DocumentsPage::BringTheReadingBack);

    SizeToTheContent(*window_, 900, 700);
    window_->show();

    elsewhere_->Retell(tr("Reading in a separate window"), open_.has_value() ? open_->caption : tr("Nothing open yet"));
    readingSide_->setCurrentWidget(elsewhere_);
}

void DocumentsPage::BringTheReadingBack()
{
    if (window_ == nullptr)
    {
        return;
    }

    QDialog* leaving = window_;
    window_ = nullptr;

    disconnect(leaving, &QDialog::finished, this, nullptr);

    reader_->setParent(nullptr);
    reader_->SayItIsDetached(false);
    readingSide_->insertWidget(0, reader_);
    readingSide_->setCurrentWidget(open_.has_value() ? static_cast<QWidget*>(reader_) : nothingOpen_);

    leaving->close();
    leaving->deleteLater();
}

void DocumentsPage::RebuildTheIndexOf(const DocumentPanel panel)
{
    PanelOfTheTab& built = panels_[Which(panel)];
    QTreeWidget& index = *built.index;

    QSet<QString> opened;

    for (int group = 0; group < index.topLevelItemCount(); ++group)
    {
        RememberWhatIsOpen(*index.topLevelItem(group), {}, opened);
    }

    built.lines.clear();
    index.clear();

    for (const DocumentGroup& group : viewModel_.GroupsOf(panel))
    {
        PutTheGroupIn(index, nullptr, group, built, opened, {}, 0);
    }
}

void DocumentsPage::PutTheGroupIn(QTreeWidget& index,
                                  QTreeWidgetItem* parent,
                                  const DocumentGroup& group,
                                  PanelOfTheTab& built,
                                  const QSet<QString>& opened,
                                  const QString& trail,
                                  const int depth)
{
    QTreeWidgetItem* heading = parent == nullptr ? new QTreeWidgetItem(&index) : new QTreeWidgetItem(parent);

    heading->setText(kNameColumn, TheHeadingOf(group, depth));
    heading->setText(kDetailColumn, group.count);
    heading->setFlags(Qt::ItemIsEnabled);

    if (depth == 0)
    {
        QFont carriesTheGroup = heading->font(kNameColumn);
        carriesTheGroup.setBold(true);
        heading->setFont(kNameColumn, carriesTheGroup);
        heading->setFont(kDetailColumn, carriesTheGroup);
    }

    const QString mine = trail + heading->text(kNameColumn);

    for (const DocumentGroup& within : group.groups)
    {
        PutTheGroupIn(index, heading, within, built, opened, mine, depth + 1);
    }

    for (const DocumentLine& line : group.lines)
    {
        auto* row = new QTreeWidgetItem(heading);
        row->setText(kGlyphColumn, StarOf(line.favourite));
        row->setText(kNameColumn, line.name);
        row->setText(kDetailColumn, line.detail);
        row->setData(kNameColumn, kLineRole, static_cast<int>(built.lines.size()));
        row->setToolTip(kNameColumn, line.caption);

        if (open_.has_value() && open_->file == line.file)
        {
            index.setCurrentItem(row);
        }

        built.lines.push_back(line);
    }

    const bool open = opened.contains(mine);

    heading->setExpanded(open);
    heading->setText(kGlyphColumn, ArrowOf(open));
}

void DocumentsPage::Rebuild()
{
    RebuildTheIndexOf(DocumentPanel::Documents);
    RebuildTheIndexOf(DocumentPanel::Charts);

    Retranslate();
}

void DocumentsPage::ShowTheIndex()
{
    Rebuild();
    ShowWhatIsHappening();
}

void DocumentsPage::ShowWhatIsHappening()
{
    const bool reading = viewModel_.Reading();
    const bool anything = viewModel_.CountOf(DocumentPanel::Documents) + viewModel_.CountOf(DocumentPanel::Charts) > 0;

    progress_->setVisible(reading);
    readAgain_->setEnabled(!reading);
    readAt_->setVisible(viewModel_.ReadAt().has_value());

    body_->setCurrentWidget(anything || reading ? static_cast<QWidget*>(split_) : nothingIndexed_);

    Retranslate();
}

void DocumentsPage::Retranslate()
{
    const std::array<QString, 2> named{tr("Documents"), tr("Charts")};

    for (std::size_t panel = 0; panel < panels_.size(); ++panel)
    {
        const std::size_t lines = viewModel_.CountOf(static_cast<DocumentPanel>(panel));

        panels_[panel].button->setText(
            viewModel_.ItWasRead() && lines > 0 ? named[panel] + kSeparator + QString::number(lines) : named[panel]);
    }

    readAgain_->setText(tr("Read again"));
    stop_->setText(tr("Stop"));
    bringItBack_->setText(tr("Bring it back"));
    readTheLibrary_->setText(tr("Read the library"));

    if (const std::optional<std::chrono::system_clock::time_point> read = viewModel_.ReadAt(); read.has_value())
    {
        readAt_->setText(tr("Read on %1").arg(AsMoment(*read)));
    }

    howFar_->setText(outOf_ == 0 ? tr("Reading the library…")
                                 : tr("Reading the library… %1 of %2").arg(indexed_).arg(outOf_));
    meter_->setRange(0, static_cast<int>(outOf_));
    meter_->setValue(static_cast<int>(indexed_));

    nothingOpen_->Retell(tr("Nothing open yet"),
                         tr("Pick a document on the left. The one you were last reading opens where you stopped."));

    if (viewModel_.ItWasRead())
    {
        nothingIndexed_->Retell(tr("No documentation in this library"),
                                tr("None of the addons carries a PDF. When one does, it shows up here without you "
                                   "asking."));

        return;
    }

    nothingIndexed_->Retell(tr("The library was never read for documentation"),
                            tr("Reading it walks every addon looking for PDFs, and what it finds is written down so "
                               "the next time is instant."));
}
