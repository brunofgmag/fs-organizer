#include "view/panels/ContextPanel.h"

#include <QtCore/QSettings>
#include <QtGui/QFont>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include "view/panels/PanelRail.h"

namespace
{
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

    badge_ = new QLabel(header);
    badge_->setObjectName(QStringLiteral("PanelBadge"));
    badge_->setVisible(false);

    toggle_ = new QToolButton(header);
    toggle_->setObjectName(QStringLiteral("PanelToggle"));
    toggle_->setAutoRaise(true);
    toggle_->setArrowType(Qt::RightArrow);
    toggle_->setCursor(Qt::PointingHandCursor);

    close_ = new QToolButton(header);
    close_->setObjectName(QStringLiteral("PanelClose"));
    close_->setAutoRaise(true);
    close_->setText(QStringLiteral("✕"));
    close_->setToolTip(tr("Fechar o painel"));
    close_->setCursor(Qt::PointingHandCursor);

    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(14, 8, 6, 8);
    headerRow->setSpacing(6);
    headerRow->addWidget(title_);
    headerRow->addWidget(badge_);
    headerRow->addStretch();
    headerRow->addWidget(toggle_);
    headerRow->addWidget(close_);

    body_ = new QWidget(this);
    body_->setObjectName(QStringLiteral("PanelBody"));
    content_ = new QVBoxLayout(body_);
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

void ContextPanel::ShowBadge(const QString& text) const
{
    badge_->setText(text);
    badge_->setVisible(!text.isEmpty());
}

void ContextPanel::ShowTitle(const QString& title, const bool alarming) const
{
    const QString shown = title.isEmpty() ? fallbackTitle_ : title;

    title_->setText(shown);
    rail_->ShowTitle(shown, alarming);
}

void ContextPanel::Summon(const bool summoned)
{
    setVisible(summoned);
}

void ContextPanel::SetCollapsed(const bool collapsed)
{
    collapsed_ = collapsed;

    header_->setVisible(!collapsed);
    body_->setVisible(!collapsed);
    badge_->setVisible(!collapsed && !badge_->text().isEmpty());
    rail_->setVisible(collapsed);

    setFixedWidth(collapsed ? PanelRail::Width() : expandedWidth_);
}
