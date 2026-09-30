#include "view/panels/ContextPanel.h"

#include <QtCore/QEvent>
#include <QtCore/QSettings>
#include <QtGui/QFont>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "view/panels/PanelRail.h"

namespace
{
    constexpr int kRuleWidth = 1;

    QString SettingsKeyFor(const QWidget& panel)
    {
        return QStringLiteral("panels/%1/collapsed").arg(panel.objectName());
    }
}

ContextPanel::ContextPanel(const QString& title, const int expandedWidth, QWidget* parent)
    : QWidget(parent), fallbackTitle_(title.toUpper()), expandedWidth_(expandedWidth)
{
    header_ = new QWidget(this);
    QWidget* header = header_;
    header->setObjectName(QStringLiteral("PanelHeader"));
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    title_ = new QLabel(fallbackTitle_, header);
    title_->setObjectName(QStringLiteral("PanelTitle"));

    QFont titleFont = title_->font();
    titleFont.setWeight(QFont::Bold);
    titleFont.setLetterSpacing(QFont::PercentageSpacing, 108);
    title_->setFont(titleFont);

    toggle_ = new QToolButton(header);
    toggle_->setObjectName(QStringLiteral("PanelToggle"));
    toggle_->setAutoRaise(true);
    toggle_->setArrowType(Qt::RightArrow);
    toggle_->setCursor(Qt::PointingHandCursor);

    close_ = new QToolButton(header);
    close_->setObjectName(QStringLiteral("PanelClose"));
    close_->setAutoRaise(true);
    close_->setText(QStringLiteral("✕"));
    RetranslateUi();
    close_->setCursor(Qt::PointingHandCursor);

    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(14, 0, 6, 0);
    headerRow->setSpacing(6);
    headerRow->addWidget(title_, 0, Qt::AlignVCenter);
    headerRow->addStretch();
    headerRow->addWidget(toggle_, 0, Qt::AlignVCenter);
    headerRow->addWidget(close_, 0, Qt::AlignVCenter);

    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("PanelBody"));

    auto* scrolled = new QScrollArea(body_);
    scrolled->setFrameShape(QFrame::NoFrame);
    scrolled->setWidgetResizable(true);
    scrolled->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrolled->viewport()->setAutoFillBackground(false);

    auto* scrollable = new QWidget(scrolled);
    scrollable->setAutoFillBackground(false);
    scrolled->setWidget(scrollable);

    auto* bodyLayout = new QVBoxLayout(body_);
    bodyLayout->setContentsMargins(kRuleWidth, 0, 0, 0);
    bodyLayout->addWidget(scrolled);

    content_ = new QVBoxLayout(scrollable);
    content_->setContentsMargins(14, 12, 14, 12);
    content_->setSpacing(9);
    content_->addStretch();

    rail_ = new PanelRail(this);
    rail_->ShowTitle(fallbackTitle_, false);
    rail_->setVisible(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(body_, 1);
    layout->addWidget(rail_, 1);

    setFixedWidth(expandedWidth_);

    const auto foldOrUnfold = [this]
    {
        SetCollapsed(!collapsed_);
        QSettings().setValue(SettingsKeyFor(*this), collapsed_);
    };

    connect(toggle_, &QToolButton::clicked, this, foldOrUnfold);
    connect(rail_, &PanelRail::ExpandRequested, this, foldOrUnfold);

    connect(close_, &QToolButton::clicked, this, &ContextPanel::CloseRequested);
}

void ContextPanel::Add(QWidget* widget) const
{
    content_->insertWidget(content_->count() - 1, widget);
}

void ContextPanel::RestoreCollapsedState()
{
    if (QSettings().value(SettingsKeyFor(*this), false).toBool())
    {
        SetCollapsed(true);
    }
}

void ContextPanel::ShowTitle(const QString& title, const bool alarming)
{
    showingFallback_ = title.isEmpty();

    const QString shown = showingFallback_ ? fallbackTitle_ : title;

    title_->setText(shown);
    rail_->ShowTitle(shown, alarming);
}

void ContextPanel::RenameTheFallback(const QString& title)
{
    fallbackTitle_ = title.toUpper();

    if (showingFallback_)
    {
        ShowTitle({});
    }
}

void ContextPanel::Summon(const bool summoned)
{
    setVisible(summoned);
}

void ContextPanel::LevelWith(QHeaderView* header)
{
    if (levelWith_ != nullptr)
    {
        levelWith_->removeEventFilter(this);
    }

    levelWith_ = header;

    if (levelWith_ != nullptr)
    {
        levelWith_->installEventFilter(this);
    }

    MatchTheColumnHeader();
}

void ContextPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

void ContextPanel::RetranslateUi() const
{
    close_->setToolTip(tr("Close the panel"));
}

void ContextPanel::SetCollapsed(const bool collapsed)
{
    collapsed_ = collapsed;

    header_->setVisible(!collapsed);
    body_->setVisible(!collapsed);
    rail_->setVisible(collapsed);

    setFixedWidth(collapsed ? PanelRail::Width() : expandedWidth_);
}

bool ContextPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == levelWith_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    {
        MatchTheColumnHeader();
    }

    return QWidget::eventFilter(watched, event);
}

void ContextPanel::MatchTheColumnHeader()
{
    if (levelWith_ == nullptr || levelWith_->height() <= 0)
    {
        return;
    }

    const int height = levelWith_->height();

    header_->setFixedHeight(height);
    rail_->AlignTheArrowWithAStripOf(height);
}
