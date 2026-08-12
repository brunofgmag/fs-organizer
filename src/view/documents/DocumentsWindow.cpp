#include "view/documents/DocumentsWindow.h"

#include <QtCore/QEvent>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#include <QtGui/QFont>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeWidget>

#include "domain/support/PathUtils.h"
#include "support/PathText.h"
#include "view/documents/DocumentReader.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kIndexWidth = 340;
    constexpr int kNarrowestIndex = 300;
    constexpr int kWindowWidth = 1180;
    constexpr int kWindowHeight = 780;
    constexpr int kStarColumn = 0;
    constexpr int kNameColumn = 1;
    constexpr int kDetailColumn = 2;

    const auto kDocumentRole = Qt::UserRole;

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
}

DocumentsWindow::DocumentsWindow(DocumentsViewModel& viewModel, QWidget* parent)
    : QDialog(parent), viewModel_(viewModel)
{
    index_ = new QTreeWidget(this);
    index_->setColumnCount(3);
    index_->setHeaderHidden(true);
    index_->setRootIsDecorated(true);
    index_->setIndentation(0);
    index_->setUniformRowHeights(true);
    index_->setSelectionBehavior(QAbstractItemView::SelectRows);
    index_->setAllColumnsShowFocus(true);
    index_->setItemDelegate(new WithoutTheFocusFrame(index_));
    index_->setMinimumWidth(kNarrowestIndex);
    index_->header()->setSectionResizeMode(kStarColumn, QHeaderView::ResizeToContents);
    index_->header()->setSectionResizeMode(kNameColumn, QHeaderView::Stretch);
    index_->header()->setSectionResizeMode(kDetailColumn, QHeaderView::ResizeToContents);

    reader_ = new DocumentReader(this);

    auto* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(index_);
    split->addWidget(reader_);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({kIndexWidth, kWindowWidth - kIndexWidth});

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    row->addWidget(split);

    connect(index_, &QTreeWidget::itemActivated, this,
            [this](const QTreeWidgetItem* line)
            {
                OpenWhatWasChosen(line);
            });
    connect(index_, &QTreeWidget::itemClicked, this, &DocumentsWindow::TurnTheStarOf);
    connect(index_, &QTreeWidget::itemExpanded, this,
            [](QTreeWidgetItem* heading)
            {
                heading->setText(kStarColumn, ArrowOf(true));
            });
    connect(index_, &QTreeWidget::itemCollapsed, this,
            [](QTreeWidgetItem* heading)
            {
                heading->setText(kStarColumn, ArrowOf(false));
            });
    connect(reader_, &DocumentReader::ThePageChanged, this,
            [this](const int page)
            {
                if (!reading_.empty())
                {
                    viewModel_.RememberThePage(reading_, page);
                }
            });
    connect(reader_, &DocumentReader::TheFolderWasAskedFor, this,
            [this]
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(viewModel_.Folder())));
            });
    connect(&viewModel_, &DocumentsViewModel::Indexed, this, &DocumentsWindow::Rebuild);

    resize(kWindowWidth, kWindowHeight);

    Retranslate();
    Rebuild();
}

void DocumentsWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        Retranslate();
        Rebuild();
    }

    QDialog::changeEvent(event);
}

void DocumentsWindow::Retranslate()
{
    setWindowTitle(tr("Documents · %1").arg(QString::fromStdString(viewModel_.Addon())));
}

void DocumentsWindow::Rebuild()
{
    QSet<QString> opened;

    for (int group = 0; group < index_->topLevelItemCount(); ++group)
    {
        if (index_->topLevelItem(group)->isExpanded())
        {
            opened.insert(index_->topLevelItem(group)->text(kNameColumn));
        }
    }

    const std::filesystem::path chosen = DocumentOf(index_->currentItem());

    index_->clear();

    for (const DocumentGroup& group : viewModel_.Groups())
    {
        auto* heading = new QTreeWidgetItem(index_);
        heading->setText(kNameColumn, group.name);

        heading->setText(kStarColumn, ArrowOf(opened.contains(group.name)));

        QFont carriesTheGroup = heading->font(kNameColumn);
        carriesTheGroup.setBold(true);
        heading->setFont(kNameColumn, carriesTheGroup);
        heading->setFont(kDetailColumn, carriesTheGroup);

        heading->setText(kDetailColumn, QString::number(group.lines.size()));
        heading->setFlags(Qt::ItemIsEnabled);

        for (const DocumentLine& line : group.lines)
        {
            auto* row = new QTreeWidgetItem(heading);
            row->setText(kStarColumn, StarOf(line.favourite));
            row->setText(kNameColumn, line.name);
            row->setText(kDetailColumn, line.detail);
            row->setData(kNameColumn, kDocumentRole, AsText(line.document));
            row->setToolTip(kNameColumn, line.name);

            if (line.document == chosen)
            {
                index_->setCurrentItem(row);
            }
        }

        heading->setExpanded(opened.contains(group.name));
    }
}

std::filesystem::path DocumentsWindow::DocumentOf(const QTreeWidgetItem* line)
{
    if (line == nullptr)
    {
        return {};
    }

    return AsPath(line->data(kNameColumn, kDocumentRole).toString());
}

void DocumentsWindow::OpenWhatWasChosen(const QTreeWidgetItem* line)
{
    const std::filesystem::path document = DocumentOf(line);

    if (document.empty())
    {
        return;
    }

    reading_ = document;
    reader_->Read(viewModel_.FullPathOf(document), viewModel_.PageOf(document));
}

void DocumentsWindow::TurnTheStarOf(QTreeWidgetItem* line, const int column)
{
    const std::filesystem::path document = DocumentOf(line);

    if (column != kStarColumn)
    {
        return;
    }

    if (document.empty())
    {
        line->setExpanded(!line->isExpanded());

        return;
    }

    viewModel_.Favour(document, !viewModel_.ItIsAFavourite(document));
    Rebuild();
}
