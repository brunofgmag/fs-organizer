#include "view/documents/DocumentReader.h"

#include <QtCore/QEvent>
#include <QtGui/QAction>
#include <QtGui/QFont>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtPdf/QPdfBookmarkModel>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfLink>
#include <QtPdf/QPdfPageNavigator>
#include <QtPdf/QPdfSearchModel>
#include <QtPdfWidgets/QPdfView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <optional>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kOutlineWidth = 210;
    constexpr int kPageSpacing = 8;
    constexpr int kSearchWidth = 160;
    constexpr int kStepWidth = 28;
    constexpr qreal kOneNotchCloser = 1.1;
    constexpr int kNotch = 120;
    constexpr int kTheOnlyColumn = 0;
    constexpr int kPageRole = Qt::UserRole;
    constexpr int kLocationRole = Qt::UserRole + 1;
    constexpr int kZoomRole = Qt::UserRole + 2;
    constexpr int kBookmarkRole = Qt::UserRole + 3;

    const QString kDisc = QString::fromUtf8("●");
    const QString kBack = QString::fromUtf8("‹");
    const QString kForth = QString::fromUtf8("›");

    [[nodiscard]] bool ItIsABookmark(const QTreeWidgetItem& item)
    {
        return item.data(kTheOnlyColumn, kBookmarkRole).toBool();
    }

    [[nodiscard]] int PageOf(const QTreeWidgetItem& item)
    {
        return item.data(kTheOnlyColumn, kPageRole).toInt();
    }

    [[nodiscard]] int WhereThePageFitsUnder(const QTreeWidgetItem& under, const int page)
    {
        for (int within = 0; within < under.childCount(); ++within)
        {
            if (PageOf(*under.child(within)) > page)
            {
                return within;
            }
        }

        return under.childCount();
    }

    [[nodiscard]] int WhereThePageFitsAtTheRootOf(const QTreeWidget& pane, const int page)
    {
        for (int top = 0; top < pane.topLevelItemCount(); ++top)
        {
            if (PageOf(*pane.topLevelItem(top)) > page)
            {
                return top;
            }
        }

        return pane.topLevelItemCount();
    }

    void OpenTheBranchOf(const QTreeWidgetItem& item)
    {
        for (QTreeWidgetItem* above = item.parent(); above != nullptr; above = above->parent())
        {
            above->setExpanded(true);
        }
    }
}

DocumentReader::DocumentReader(QWidget* parent) : QWidget(parent)
{
    document_ = new QPdfDocument(this);

    view_ = new QPdfView(this);
    view_->setDocument(document_);
    view_->setPageMode(QPdfView::PageMode::MultiPage);
    view_->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    view_->setPageSpacing(kPageSpacing);
    view_->setDocumentMargins({0, 0, 0, 0});
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    search_ = new QPdfSearchModel(this);
    search_->setDocument(document_);
    view_->setSearchModel(search_);

    outline_ = new QPdfBookmarkModel(this);
    outline_->setDocument(document_);

    BuildTheOutlinePane();

    caption_ = new QLabel(this);
    caption_->setObjectName(QStringLiteral("ReadingCaption"));

    auto* pages = new QVBoxLayout;
    pages->setContentsMargins(0, 0, 0, 0);
    pages->setSpacing(8);
    pages->addLayout(TheBar());
    pages->addWidget(caption_);
    pages->addWidget(view_, 1);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kPageGutter, 0, 0, 0);
    row->setSpacing(12);
    row->addLayout(pages, 1);
    row->addWidget(outlinePane_);

    ConnectTheBar();
    ConnectThePane();
    ConnectTheDocument();

    view_->viewport()->installEventFilter(this);

    Retranslate();
    RebuildThePane();
}

DocumentReader::~DocumentReader()
{
    disconnect(document_, nullptr, this, nullptr);
    disconnect(search_, nullptr, this, nullptr);
    disconnect(view_->pageNavigator(), nullptr, this, nullptr);
}

void DocumentReader::Read(const std::filesystem::path& document,
                          const int page,
                          const DocumentKind kind,
                          const std::vector<DocumentBookmark>& bookmarks)
{
    kind_ = kind;
    bookmarks_ = bookmarks;
    wanted_->clear();
    view_->viewport()->setCursor(kind == DocumentKind::Chart ? Qt::OpenHandCursor : Qt::ArrowCursor);

    outlineView_->clear();
    sections_.clear();
    sectionItems_.clear();

    document_->load(AsText(document));

    view_->pageNavigator()->jump(page, {});

    RebuildThePane();
    SayWhereTheReadingIs();
}

void DocumentReader::ShowTheBookmarks(const std::vector<DocumentBookmark>& bookmarks)
{
    bookmarks_ = bookmarks;

    RebuildThePane();
}

void DocumentReader::SayItIsShowing(const QString& caption)
{
    caption_->setText(caption);
}

void DocumentReader::SayItIsDetached(const bool detached)
{
    detached_ = detached;

    Retranslate();
}

void DocumentReader::ZoomBy(const int notches)
{
    fitWidth_->setChecked(false);

    const qreal closer = notches > 0 ? kOneNotchCloser : 1 / kOneNotchCloser;

    view_->setZoomMode(QPdfView::ZoomMode::Custom);
    view_->setZoomFactor(view_->zoomFactor() * closer);
}

bool DocumentReader::TheChartAnswersThe(QEvent* event)
{
    if (event->type() == QEvent::Wheel)
    {
        ZoomBy(static_cast<QWheelEvent*>(event)->angleDelta().y());

        return true;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        grabbing_ = true;
        grabbedAt_ = static_cast<QMouseEvent*>(event)->pos();
        view_->viewport()->setCursor(Qt::ClosedHandCursor);

        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        grabbing_ = false;
        view_->viewport()->setCursor(Qt::OpenHandCursor);

        return true;
    }

    if (event->type() != QEvent::MouseMove || !grabbing_)
    {
        return false;
    }

    const QPoint now = static_cast<QMouseEvent*>(event)->pos();
    const QPoint dragged = now - grabbedAt_;
    grabbedAt_ = now;

    view_->horizontalScrollBar()->setValue(view_->horizontalScrollBar()->value() - dragged.x());
    view_->verticalScrollBar()->setValue(view_->verticalScrollBar()->value() - dragged.y());

    return true;
}

bool DocumentReader::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != view_->viewport() || kind_ != DocumentKind::Chart)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (TheChartAnswersThe(event))
    {
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void DocumentReader::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
        SayWhereTheReadingIs();
    }

    QWidget::changeEvent(event);
}

void DocumentReader::Retranslate() const
{
    previous_->setText(tr("Previous page"));
    next_->setText(tr("Next page"));
    fitWidth_->setText(tr("Fit width"));
    bookmark_->setText(tr("Bookmark"));
    detach_->setText(detached_ ? tr("Bring it back") : tr("Detach"));
    openFolder_->setText(tr("Open folder"));
    outlineHeading_->setText(TheHeadingOfThePane());
    rename_->setText(tr("Rename this bookmark…"));
    forget_->setText(tr("Remove this bookmark"));
    wanted_->setPlaceholderText(tr("Search in this document"));
    previousResult_->setToolTip(tr("Previous match"));
    nextResult_->setToolTip(tr("Next match"));

    const bool anythingToStepThrough = !wanted_->text().isEmpty() && search_->rowCount({}) > 0;

    previousResult_->setEnabled(anythingToStepThrough);
    nextResult_->setEnabled(anythingToStepThrough);

    if (wanted_->text().isEmpty())
    {
        found_->clear();

        return;
    }

    if (search_->rowCount({}) == 0)
    {
        found_->setText(tr("Not found"));

        return;
    }

    found_->setText(tr("%1 of %2").arg(result_ + 1).arg(search_->rowCount({})));
}

void DocumentReader::SayWhereTheReadingIs() const
{
    position_->setText(tr("Page %1 of %2").arg(view_->pageNavigator()->currentPage() + 1).arg(document_->pageCount()));
    previous_->setEnabled(view_->pageNavigator()->currentPage() > 0);
    next_->setEnabled(view_->pageNavigator()->currentPage() + 1 < document_->pageCount());
}

void DocumentReader::SearchFor(const QString& wanted)
{
    result_ = -1;
    search_->setSearchString(wanted);

    Retranslate();
}

void DocumentReader::StepThroughTheResults(const int by)
{
    if (search_->rowCount({}) == 0)
    {
        return;
    }

    JumpToTheResult((result_ + by + search_->rowCount({})) % search_->rowCount({}));
}

void DocumentReader::JumpToTheResult(const int result)
{
    result_ = result;
    view_->setCurrentSearchResultIndex(result);

    if (result >= 0)
    {
        const QPdfLink found = search_->resultAtIndex(result);
        view_->pageNavigator()->jump(found.page(), found.location());
    }

    Retranslate();
}

std::vector<bool> DocumentReader::WhichSectionsAreOpen() const
{
    std::vector<bool> opened;
    opened.reserve(sectionItems_.size());

    for (const QTreeWidgetItem* section : sectionItems_)
    {
        opened.push_back(section->isExpanded());
    }

    return opened;
}

void DocumentReader::OpenAgainWhatWasOpen(const std::vector<bool>& opened) const
{
    if (opened.size() != sectionItems_.size())
    {
        outlineView_->expandToDepth(0);

        return;
    }

    for (std::size_t which = 0; which < opened.size(); ++which)
    {
        sectionItems_[which]->setExpanded(opened[which]);
    }
}

void DocumentReader::RebuildThePane()
{
    const std::vector<bool> opened = WhichSectionsAreOpen();

    outlineView_->clear();
    sections_.clear();
    sectionItems_.clear();

    PutTheSectionsIn({}, nullptr);
    OpenAgainWhatWasOpen(opened);
    PutTheBookmarksIn();

    outlineView_->setRootIsDecorated(!sections_.empty());
    outlinePane_->setVisible(!sections_.empty() || !bookmarks_.empty());
    outlineHeading_->setText(TheHeadingOfThePane());

    MarkTheSectionOfThePage();
    SayWhetherThisPageIsMarked();
    SayWhatTheMenuCanDo();
}

void DocumentReader::PutTheSectionsIn(const QModelIndex& parent, QTreeWidgetItem* under)
{
    for (int row = 0; row < outline_->rowCount(parent); ++row)
    {
        const QModelIndex entry = outline_->index(row, 0, parent);
        const QString title = entry.data(Qt::DisplayRole).toString();
        const int page = entry.data(static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt();

        auto* item = under == nullptr ? new QTreeWidgetItem(outlineView_) : new QTreeWidgetItem(under);
        item->setText(kTheOnlyColumn, title);
        item->setData(kTheOnlyColumn, kPageRole, page);
        item->setData(kTheOnlyColumn, kLocationRole, entry.data(static_cast<int>(QPdfBookmarkModel::Role::Location)));
        item->setData(kTheOnlyColumn, kZoomRole, entry.data(static_cast<int>(QPdfBookmarkModel::Role::Zoom)));
        item->setData(kTheOnlyColumn, kBookmarkRole, false);

        sections_.push_back({.title = title.toStdString(), .page = page});
        sectionItems_.push_back(item);

        PutTheSectionsIn(entry, item);
    }
}

void DocumentReader::PutTheBookmarksIn()
{
    for (const DocumentBookmark& bookmark : bookmarks_)
    {
        const int page = bookmark.page;
        const std::optional<std::size_t> holder = TheSectionHolding(sections_, page);

        auto* item = new QTreeWidgetItem;
        item->setText(kTheOnlyColumn, kDisc + QStringLiteral(" ") + NameOf(bookmark));
        item->setData(kTheOnlyColumn, kPageRole, page);
        item->setData(kTheOnlyColumn, kBookmarkRole, true);

        if (!holder.has_value())
        {
            outlineView_->insertTopLevelItem(WhereThePageFitsAtTheRootOf(*outlineView_, page), item);
            continue;
        }

        QTreeWidgetItem* under = sectionItems_[*holder];
        under->insertChild(WhereThePageFitsUnder(*under, page), item);

        OpenTheBranchOf(*item);
    }
}

const DocumentBookmark* DocumentReader::TheBookmarkOn(const int page) const
{
    const auto known = std::ranges::find_if(bookmarks_,
                                            [page](const DocumentBookmark& mark)
                                            {
                                                return mark.page == page;
                                            });

    if (known == bookmarks_.end())
    {
        return nullptr;
    }

    return &*known;
}

QString DocumentReader::NameOf(const DocumentBookmark& bookmark) const
{
    if (!bookmark.name.empty())
    {
        return QString::fromStdString(bookmark.name);
    }

    return tr("Page %1").arg(bookmark.page + 1);
}

QString DocumentReader::TheHeadingOfThePane() const
{
    if (bookmarks_.empty())
    {
        return tr("Outline");
    }

    if (sections_.empty())
    {
        return tr("Bookmarks");
    }

    return tr("Outline and bookmarks");
}

void DocumentReader::MarkTheSectionOfThePage() const
{
    const std::optional<std::size_t> holding = TheSectionHolding(sections_, view_->pageNavigator()->currentPage());

    for (std::size_t which = 0; which < sectionItems_.size(); ++which)
    {
        QFont letters = outlineView_->font();
        letters.setBold(holding.has_value() && which == *holding);

        sectionItems_[which]->setFont(kTheOnlyColumn, letters);
    }

    if (holding.has_value())
    {
        outlineView_->scrollToItem(sectionItems_[*holding]);
    }
}

void DocumentReader::SayWhetherThisPageIsMarked() const
{
    const int page = view_->pageNavigator()->currentPage();

    bookmark_->setChecked(std::ranges::any_of(bookmarks_,
                                              [page](const DocumentBookmark& mark)
                                              {
                                                  return mark.page == page;
                                              }));
}

void DocumentReader::OfferTheMenuAt(const QPoint& where)
{
    QTreeWidgetItem* under = outlineView_->itemAt(where);

    if (under == nullptr || !ItIsABookmark(*under))
    {
        return;
    }

    outlineView_->setCurrentItem(under);
    SayWhatTheMenuCanDo();
    menu_->popup(outlineView_->viewport()->mapToGlobal(where));
}

void DocumentReader::SayWhatTheMenuCanDo() const
{
    const QTreeWidgetItem* chosen = outlineView_->currentItem();
    const bool aBookmark = chosen != nullptr && ItIsABookmark(*chosen);

    rename_->setEnabled(aBookmark);
    forget_->setEnabled(aBookmark);
}

void DocumentReader::RenameWhatIsChosen()
{
    const QTreeWidgetItem* chosen = outlineView_->currentItem();

    if (chosen == nullptr || !ItIsABookmark(*chosen))
    {
        return;
    }

    const int page = PageOf(*chosen);
    const DocumentBookmark* known = TheBookmarkOn(page);
    const QString already = known == nullptr ? QString() : QString::fromStdString(known->name);

    bool said = false;
    const QString given =
        QInputDialog::getText(this, tr("Rename the bookmark"), tr("Name:"), QLineEdit::Normal, already, &said);

    if (!said)
    {
        return;
    }

    emit TheBookmarkWasNamed(page, given);
}

void DocumentReader::ForgetWhatIsChosen()
{
    const QTreeWidgetItem* chosen = outlineView_->currentItem();

    if (chosen == nullptr || !ItIsABookmark(*chosen))
    {
        return;
    }

    const int page = PageOf(*chosen);
    const DocumentBookmark* known = TheBookmarkOn(page);

    if (known == nullptr)
    {
        return;
    }

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("Remove the bookmark"), tr("Remove the bookmark \"%1\"?").arg(NameOf(*known)));

    if (answer != QMessageBox::Yes)
    {
        return;
    }

    emit TheMarkOfThePageWasTurned(page, false);
}

void DocumentReader::BuildTheOutlinePane()
{
    outlineHeading_ = new QLabel(this);
    outlineHeading_->setObjectName(QStringLiteral("PanelSubHeading"));

    outlineView_ = new QTreeWidget(this);
    outlineView_->setObjectName(QStringLiteral("ReadingOutline"));
    outlineView_->setColumnCount(1);
    outlineView_->setHeaderHidden(true);
    outlineView_->setContextMenuPolicy(Qt::CustomContextMenu);

    rename_ = new QAction(this);
    forget_ = new QAction(this);
    outlineView_->addAction(rename_);
    outlineView_->addAction(forget_);

    menu_ = new QMenu(this);
    menu_->addAction(rename_);
    menu_->addAction(forget_);

    outlinePane_ = new QWidget(this);
    outlinePane_->setFixedWidth(kOutlineWidth);

    auto* outlineColumn = new QVBoxLayout(outlinePane_);
    outlineColumn->setContentsMargins(0, 0, 0, 0);
    outlineColumn->setSpacing(6);
    outlineColumn->addWidget(outlineHeading_);
    outlineColumn->addWidget(outlineView_);
}

QLayout* DocumentReader::TheBar()
{
    previous_ = new QPushButton(this);
    next_ = new QPushButton(this);
    next_->setObjectName(QStringLiteral("NextPage"));
    position_ = new QLabel(this);
    wanted_ = new QLineEdit(this);
    wanted_->setClearButtonEnabled(true);
    wanted_->setMinimumWidth(kSearchWidth);
    found_ = new QLabel(this);
    found_->setObjectName(QStringLiteral("PanelPromise"));
    previousResult_ = new QPushButton(kBack, this);
    previousResult_->setObjectName(QStringLiteral("PreviousMatch"));
    nextResult_ = new QPushButton(kForth, this);
    nextResult_->setObjectName(QStringLiteral("NextMatch"));

    for (QPushButton* step : {previousResult_, nextResult_})
    {
        step->setFixedWidth(kStepWidth);
    }
    fitWidth_ = new QPushButton(this);
    bookmark_ = new QPushButton(this);
    bookmark_->setObjectName(QStringLiteral("BookmarkThePage"));

    for (QPushButton* toggle : {fitWidth_, bookmark_})
    {
        toggle->setCheckable(true);
        toggle->setProperty("toggle", "true");
    }

    fitWidth_->setChecked(true);
    detach_ = new QPushButton(this);
    openFolder_ = new QPushButton(this);

    for (QPushButton* button :
         {previous_, next_, previousResult_, nextResult_, fitWidth_, bookmark_, detach_, openFolder_})
    {
        button->setAutoDefault(false);
        button->setDefault(false);
    }

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(8);
    bar->addWidget(previous_);
    bar->addWidget(next_);
    bar->addWidget(position_);
    bar->addSpacing(12);
    bar->addWidget(wanted_, 1);
    bar->addWidget(found_);
    bar->addWidget(previousResult_);
    bar->addWidget(nextResult_);
    bar->addWidget(fitWidth_);
    bar->addWidget(bookmark_);
    bar->addWidget(detach_);
    bar->addWidget(openFolder_);

    return bar;
}

void DocumentReader::ConnectTheBar()
{
    connect(previous_, &QPushButton::clicked, this,
            [this]
            {
                view_->pageNavigator()->jump(view_->pageNavigator()->currentPage() - 1, {});
            });
    connect(next_, &QPushButton::clicked, this,
            [this]
            {
                view_->pageNavigator()->jump(view_->pageNavigator()->currentPage() + 1, {});
            });
    connect(openFolder_, &QPushButton::clicked, this, &DocumentReader::TheFolderWasAskedFor);
    connect(detach_, &QPushButton::clicked, this, &DocumentReader::TheDetachWasAskedFor);
    connect(fitWidth_, &QPushButton::toggled, this,
            [this](const bool fit)
            {
                view_->setZoomMode(fit ? QPdfView::ZoomMode::FitToWidth : QPdfView::ZoomMode::Custom);
                view_->setHorizontalScrollBarPolicy(fit ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
            });
    connect(bookmark_, &QPushButton::clicked, this,
            [this](const bool marked)
            {
                emit TheMarkOfThePageWasTurned(view_->pageNavigator()->currentPage(), marked);
            });
    connect(wanted_, &QLineEdit::textChanged, this, &DocumentReader::SearchFor);
    connect(wanted_, &QLineEdit::returnPressed, this,
            [this]
            {
                StepThroughTheResults(1);
            });
    connect(previousResult_, &QPushButton::clicked, this,
            [this]
            {
                StepThroughTheResults(-1);
            });
    connect(nextResult_, &QPushButton::clicked, this,
            [this]
            {
                StepThroughTheResults(1);
            });
}

void DocumentReader::ConnectThePane()
{
    connect(outlineView_, &QTreeWidget::itemClicked, this,
            [this](const QTreeWidgetItem* entry)
            {
                view_->pageNavigator()->jump(PageOf(*entry), entry->data(kTheOnlyColumn, kLocationRole).toPointF(),
                                             entry->data(kTheOnlyColumn, kZoomRole).toReal());
            });
    connect(outlineView_, &QTreeWidget::currentItemChanged, this,
            [this]
            {
                SayWhatTheMenuCanDo();
            });
    connect(outlineView_, &QTreeWidget::customContextMenuRequested, this, &DocumentReader::OfferTheMenuAt);
    connect(rename_, &QAction::triggered, this, &DocumentReader::RenameWhatIsChosen);
    connect(forget_, &QAction::triggered, this, &DocumentReader::ForgetWhatIsChosen);
}

void DocumentReader::ConnectTheDocument()
{
    connect(search_, &QPdfSearchModel::countChanged, this,
            [this]
            {
                Retranslate();
                JumpToTheResult(search_->rowCount({}) > 0 ? 0 : -1);
            });
    connect(view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this,
            [this](const int page)
            {
                SayWhereTheReadingIs();
                MarkTheSectionOfThePage();
                SayWhetherThisPageIsMarked();

                emit ThePageChanged(page);
            });
    connect(document_, &QPdfDocument::statusChanged, this,
            [this]
            {
                RebuildThePane();
                SayWhereTheReadingIs();
            });
}
