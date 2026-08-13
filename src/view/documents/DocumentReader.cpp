#include "view/documents/DocumentReader.h"

#include <QtCore/QEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtPdf/QPdfBookmarkModel>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfLink>
#include <QtPdf/QPdfPageNavigator>
#include <QtPdf/QPdfSearchModel>
#include <QtPdfWidgets/QPdfView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kOutlineWidth = 210;
    constexpr int kPageSpacing = 8;
    constexpr int kSearchWidth = 240;
    constexpr qreal kOneNotchCloser = 1.1;
    constexpr int kNotch = 120;
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

    ConnectTheParts();

    view_->viewport()->installEventFilter(this);

    Retranslate();
    ShowTheOutlineOnlyWhenThereIsOne();
}

void DocumentReader::Read(const std::filesystem::path& document, const int page, const DocumentKind kind)
{
    kind_ = kind;
    wanted_->clear();
    view_->viewport()->setCursor(kind == DocumentKind::Chart ? Qt::OpenHandCursor : Qt::ArrowCursor);
    document_->load(AsText(document));

    view_->pageNavigator()->jump(page, {});

    ShowTheOutlineOnlyWhenThereIsOne();
    SayWhereTheReadingIs();
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
    detach_->setText(detached_ ? tr("Bring it back") : tr("Detach"));
    openFolder_->setText(tr("Open folder"));
    outlineHeading_->setText(tr("Outline"));
    wanted_->setPlaceholderText(tr("Search in this document"));

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

void DocumentReader::ShowTheOutlineOnlyWhenThereIsOne() const
{
    outlinePane_->setVisible(outline_->rowCount() > 0);
    outlineView_->expandToDepth(0);
}

void DocumentReader::BuildTheOutlinePane()
{
    outlineHeading_ = new QLabel(this);
    outlineHeading_->setObjectName(QStringLiteral("PanelSubHeading"));

    outlineView_ = new QTreeView(this);
    outlineView_->setModel(outline_);
    outlineView_->setHeaderHidden(true);
    outlineView_->setRootIsDecorated(true);
    outlineView_->expandToDepth(0);

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
    position_ = new QLabel(this);
    wanted_ = new QLineEdit(this);
    wanted_->setClearButtonEnabled(true);
    wanted_->setMinimumWidth(kSearchWidth);
    found_ = new QLabel(this);
    found_->setObjectName(QStringLiteral("PanelPromise"));
    fitWidth_ = new QPushButton(this);
    fitWidth_->setCheckable(true);
    fitWidth_->setChecked(true);
    detach_ = new QPushButton(this);
    openFolder_ = new QPushButton(this);

    for (QPushButton* button : {previous_, next_, fitWidth_, detach_, openFolder_})
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
    bar->addWidget(fitWidth_);
    bar->addWidget(detach_);
    bar->addWidget(openFolder_);

    return bar;
}

void DocumentReader::ConnectTheParts()
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
    connect(wanted_, &QLineEdit::textChanged, this, &DocumentReader::SearchFor);
    connect(wanted_, &QLineEdit::returnPressed, this,
            [this]
            {
                StepThroughTheResults(1);
            });
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

                emit ThePageChanged(page);
            });
    connect(outlineView_, &QTreeView::clicked, this,
            [this](const QModelIndex& entry)
            {
                view_->pageNavigator()->jump(entry.data(static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt(),
                                             entry.data(static_cast<int>(QPdfBookmarkModel::Role::Location)).toPointF(),
                                             entry.data(static_cast<int>(QPdfBookmarkModel::Role::Zoom)).toReal());
            });
    connect(document_, &QPdfDocument::statusChanged, this,
            [this]
            {
                ShowTheOutlineOnlyWhenThereIsOne();
                SayWhereTheReadingIs();
            });
}
