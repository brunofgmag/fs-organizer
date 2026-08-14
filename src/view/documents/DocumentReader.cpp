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
#include <QtWidgets/QApplication>
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
    constexpr int kSearchWidth = 172;
    constexpr int kStepWidth = 38;
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
    const QString kUp = QString::fromUtf8("↑");
    const QString kDown = QString::fromUtf8("↓");
    const QString kWheel = QString::fromUtf8("⊙");
    const QString kMove = QString::fromUtf8("✥");
    const QString kCloser = QString::fromUtf8("＋");
    const QString kFurther = QString::fromUtf8("－");

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

    view_ = new SelectablePages(this);
    view_->FollowTheDocument(document_);
    view_->setPageMode(QPdfView::PageMode::MultiPage);
    view_->setZoomMode(QPdfView::ZoomMode::Custom);
    view_->setPageSpacing(kPageSpacing);
    view_->setDocumentMargins({0, 0, 0, 0});
    view_->setFrameShape(QFrame::NoFrame);
    view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    search_ = new QPdfSearchModel(this);
    search_->setDocument(document_);
    view_->setSearchModel(search_);

    outline_ = new QPdfBookmarkModel(this);
    outline_->setDocument(document_);

    BuildTheOutlinePane();

    caption_ = new QLabel(this);
    caption_->setObjectName(QStringLiteral("ReadingCaption"));

    caption_->setContentsMargins(kPageGutter, 0, kPageGutter, 0);

    QLayout* bar = TheBar();
    bar->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, 0);

    auto* pages = new QVBoxLayout;
    pages->setContentsMargins(0, 0, 0, 0);
    pages->setSpacing(8);
    pages->addLayout(bar);
    pages->addWidget(caption_);
    pages->addWidget(view_, 1);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
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
    RestTheCursor();

    outlineView_->clear();
    sections_.clear();
    sectionItems_.clear();

    document_->load(AsText(document));

    view_->pageNavigator()->jump(page, {});

    RebuildThePane();
    SayWhereTheReadingIs();
    ShowTheGesturesInForce();
    Retranslate();
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

void DocumentReader::RestTheCursor() const
{
    if (kind_ == DocumentKind::Chart)
    {
        view_->viewport()->setCursor(Qt::OpenHandCursor);

        return;
    }

    view_->viewport()->unsetCursor();
}

int DocumentReader::HowManyClicksInARow(QEvent* event)
{
    const QPoint now = static_cast<QMouseEvent*>(event)->pos();

    const bool soonEnough =
        sinceTheLastClick_.isValid() && sinceTheLastClick_.elapsed() <= QApplication::doubleClickInterval();
    const bool closeEnough = (now - clickedAt_).manhattanLength() < QApplication::startDragDistance();
    const bool carriedOn = soonEnough && closeEnough;

    if (event->type() == QEvent::MouseButtonDblClick)
    {
        clicksInARow_ = 2;
    }
    else if (carriedOn && clicksInARow_ == 2)
    {
        clicksInARow_ = 3;
    }
    else
    {
        clicksInARow_ = 1;
    }

    clickedAt_ = now;
    sinceTheLastClick_.start();

    return clicksInARow_;
}

bool DocumentReader::TheClickChoseSomethingToSelect(QEvent* event)
{
    const int clicks = HowManyClicksInARow(event);

    if (clicks == 2)
    {
        view_->SelectTheWordAt(clickedAt_);

        return true;
    }

    if (clicks == 3)
    {
        view_->SelectTheLineAt(clickedAt_);

        return true;
    }

    return false;
}

bool DocumentReader::TheSelectionAnswersThe(QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (TheClickChoseSomethingToSelect(event))
        {
            return true;
        }

        grabbing_ = true;
        wandered_ = false;
        grabbedAt_ = static_cast<QMouseEvent*>(event)->pos();
        view_->StartSelectingAt(grabbedAt_);

        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        const bool marked = wandered_;

        grabbing_ = false;
        wandered_ = false;

        return marked;
    }

    if (event->type() != QEvent::MouseMove || !grabbing_)
    {
        return false;
    }

    const QPoint now = static_cast<QMouseEvent*>(event)->pos();

    if (!wandered_)
    {
        if ((now - grabbedAt_).manhattanLength() < QApplication::startDragDistance())
        {
            return false;
        }

        wandered_ = true;
    }

    view_->ExtendTheSelectionTo(now);

    return true;
}

bool DocumentReader::ThePanAnswersThe(QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (TheClickChoseSomethingToSelect(event))
        {
            return true;
        }

        view_->ForgetTheSelection();

        grabbing_ = true;
        wandered_ = false;
        grabbedAt_ = static_cast<QMouseEvent*>(event)->pos();

        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        const bool moved = wandered_;

        grabbing_ = false;
        wandered_ = false;
        RestTheCursor();

        return moved;
    }

    if (event->type() != QEvent::MouseMove || !grabbing_)
    {
        return false;
    }

    const QPoint now = static_cast<QMouseEvent*>(event)->pos();

    if (!wandered_)
    {
        if ((now - grabbedAt_).manhattanLength() < QApplication::startDragDistance())
        {
            return false;
        }

        wandered_ = true;
        view_->viewport()->setCursor(Qt::ClosedHandCursor);
    }

    const QPoint dragged = now - grabbedAt_;
    grabbedAt_ = now;

    view_->horizontalScrollBar()->setValue(view_->horizontalScrollBar()->value() - dragged.x());
    view_->verticalScrollBar()->setValue(view_->verticalScrollBar()->value() - dragged.y());

    return true;
}

bool DocumentReader::TheGestureAnswersThe(QEvent* event)
{
    if (event->type() == QEvent::Wheel)
    {
        if (!TheGesturesInForce().wheelZooms)
        {
            return false;
        }

        ZoomBy(static_cast<QWheelEvent*>(event)->angleDelta().y());

        return true;
    }

    if (event->type() == QEvent::MouseButtonDblClick)
    {
        return TheClickChoseSomethingToSelect(event);
    }

    return TheGesturesInForce().dragMovesThePage ? ThePanAnswersThe(event) : TheSelectionAnswersThe(event);
}

const ReadingGestures& DocumentReader::TheGesturesInForce() const
{
    return kind_ == DocumentKind::Chart ? onCharts_ : onDocuments_;
}

void DocumentReader::SayTheGesturesOf(const DocumentKind kind, const ReadingGestures gestures)
{
    (kind == DocumentKind::Chart ? onCharts_ : onDocuments_) = gestures;

    if (kind == kind_)
    {
        ShowTheGesturesInForce();
        Retranslate();
    }
}

void DocumentReader::ShowTheGesturesInForce()
{
    const ReadingGestures& gestures = TheGesturesInForce();

    wheelZoom_->setChecked(gestures.wheelZooms);
    dragMoves_->setChecked(gestures.dragMovesThePage);

    if (!gestures.dragMovesThePage)
    {
        StopAnyGrabbing();
    }
}

void DocumentReader::StopAnyGrabbing()
{
    grabbing_ = false;
    wandered_ = false;

    RestTheCursor();
}

bool DocumentReader::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != view_->viewport())
    {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ContextMenu)
    {
        OfferTheCopyMenuAt(static_cast<QContextMenuEvent*>(event)->pos());

        return true;
    }

    if (TheGestureAnswersThe(event))
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
    previous_->setToolTip(tr("Previous page"));
    next_->setToolTip(tr("Next page"));
    closer_->setToolTip(tr("Zoom in"));
    further_->setToolTip(tr("Zoom out"));
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
    wheelZoom_->setToolTip(kind_ == DocumentKind::Chart ? tr("The wheel zooms the chart")
                                                        : tr("The wheel zooms the document"));
    dragMoves_->setToolTip(dragMoves_->isChecked() ? tr("Dragging moves the page. Turn it off to select text")
                                                   : tr("Dragging selects text. Turn it on to move the page"));
    copy_->setText(tr("Copy"));

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

        BringTheResultIntoView(found);
    }

    Retranslate();
}

int DocumentReader::WhereTheResultSitsInTheScrollbar(const QPdfLink& found) const
{
    const WhereAPageSits sits = view_->WhereThePageSits(found.page());

    if (sits.box.isEmpty())
    {
        return -1;
    }

    return qRound(sits.box.y() + found.location().y() * sits.scale);
}

void DocumentReader::BringTheResultIntoView(const QPdfLink& found) const
{
    QScrollBar* bar = view_->verticalScrollBar();

    if (bar->maximum() <= 0 || document_->pageCount() <= 0)
    {
        return;
    }

    const int where = WhereTheResultSitsInTheScrollbar(found);
    const int lead = view_->viewport()->height() / 4;

    if (where < 0 || (where >= bar->value() + lead && where <= bar->value() + view_->viewport()->height() - lead))
    {
        return;
    }

    bar->setValue(std::clamp(where - lead, bar->minimum(), bar->maximum()));
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

void DocumentReader::OfferTheCopyMenuAt(const QPoint& where)
{
    if (!view_->CarriesASelection())
    {
        return;
    }

    copyMenu_->popup(view_->viewport()->mapToGlobal(where));
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

    copy_ = new QAction(this);
    copyMenu_ = new QMenu(this);
    copyMenu_->addAction(copy_);

    connect(copy_, &QAction::triggered, this,
            [this]
            {
                view_->CopyWhatIsSelected();
            });

    outlinePane_ = new QWidget(this);
    outlinePane_->setFixedWidth(kOutlineWidth);

    auto* outlineColumn = new QVBoxLayout(outlinePane_);
    outlineColumn->setContentsMargins(0, kPageGutter, kPageGutter, 0);
    outlineColumn->setSpacing(6);
    outlineColumn->addWidget(outlineHeading_);
    outlineColumn->addWidget(outlineView_);
}

QLayout* DocumentReader::TheBar()
{
    previous_ = new QPushButton(kBack, this);
    next_ = new QPushButton(kForth, this);
    next_->setObjectName(QStringLiteral("NextPage"));
    position_ = new QLabel(this);
    wanted_ = new QLineEdit(this);
    wanted_->setClearButtonEnabled(true);
    wanted_->setMinimumWidth(kSearchWidth);
    found_ = new QLabel(this);
    found_->setObjectName(QStringLiteral("PanelPromise"));
    previousResult_ = new QPushButton(kUp, this);
    previousResult_->setObjectName(QStringLiteral("PreviousMatch"));
    nextResult_ = new QPushButton(kDown, this);
    nextResult_->setObjectName(QStringLiteral("NextMatch"));
    closer_ = new QPushButton(kCloser, this);
    closer_->setObjectName(QStringLiteral("ZoomIn"));
    further_ = new QPushButton(kFurther, this);
    further_->setObjectName(QStringLiteral("ZoomOut"));
    wheelZoom_ = new QPushButton(kWheel, this);
    wheelZoom_->setObjectName(QStringLiteral("WheelZooms"));
    dragMoves_ = new QPushButton(kMove, this);
    dragMoves_->setObjectName(QStringLiteral("DragMovesThePage"));

    for (QPushButton* step :
         {previous_, next_, previousResult_, nextResult_, closer_, further_, wheelZoom_, dragMoves_})
    {
        step->setFixedWidth(kStepWidth);
    }
    fitWidth_ = new QPushButton(this);
    fitWidth_->setObjectName(QStringLiteral("FitTheWidth"));
    bookmark_ = new QPushButton(this);
    bookmark_->setObjectName(QStringLiteral("BookmarkThePage"));

    for (QPushButton* toggle : {fitWidth_, bookmark_, wheelZoom_, dragMoves_})
    {
        toggle->setCheckable(true);
        toggle->setProperty("toggle", "true");
    }

    ShowTheGesturesInForce();

    detach_ = new QPushButton(this);
    detach_->setObjectName(QStringLiteral("DetachTheReading"));
    openFolder_ = new QPushButton(this);

    for (QPushButton* button : {previous_, next_, previousResult_, nextResult_, closer_, further_, wheelZoom_,
                                dragMoves_, fitWidth_, bookmark_, detach_, openFolder_})
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
    bar->addSpacing(12);
    bar->addWidget(further_);
    bar->addWidget(closer_);
    bar->addWidget(wheelZoom_);
    bar->addWidget(dragMoves_);
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
    connect(wheelZoom_, &QPushButton::clicked, this,
            [this](const bool zooming)
            {
                (kind_ == DocumentKind::Chart ? onCharts_ : onDocuments_).wheelZooms = zooming;

                emit TheWheelWasSetToZoom(kind_, zooming);
            });
    connect(dragMoves_, &QPushButton::clicked, this,
            [this](const bool moving)
            {
                (kind_ == DocumentKind::Chart ? onCharts_ : onDocuments_).dragMovesThePage = moving;

                if (!moving)
                {
                    StopAnyGrabbing();
                }

                Retranslate();

                emit TheDragWasSetToMoveThePage(kind_, moving);
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
    connect(closer_, &QPushButton::clicked, this,
            [this]
            {
                ZoomBy(1);
            });
    connect(further_, &QPushButton::clicked, this,
            [this]
            {
                ZoomBy(-1);
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
