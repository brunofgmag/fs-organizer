#include "view/panels/EmptyState.h"

#include <algorithm>

#include <QtGui/QFont>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "view/theme/ModernistTheme.h"

namespace
{
    constexpr int kReadableWidth = 420;
}

EmptyState::EmptyState(QWidget* parent) : QWidget(parent)
{
    auto* mark = new QLabel(this);
    mark->setPixmap(BrandIcon().pixmap(40));
    mark->setAlignment(Qt::AlignHCenter);

    auto* title = new QLabel(this);
    title_ = title;
    title->setObjectName(QStringLiteral("EmptyHeadline"));
    title->setAlignment(Qt::AlignHCenter);

    QFont bold = title->font();
    bold.setWeight(QFont::ExtraBold);
    bold.setPointSizeF(bold.pointSizeF() * 1.2);
    title->setFont(bold);

    auto* body = new QLabel(this);
    body_ = body;
    body->setObjectName(QStringLiteral("EmptyBody"));
    body->setAlignment(Qt::AlignHCenter);
    body->setWordWrap(true);
    body->setFixedWidth(kReadableWidth);

    column_ = new QVBoxLayout;
    column_->setSpacing(12);
    column_->setAlignment(Qt::AlignHCenter);
    column_->addWidget(mark, 0, Qt::AlignHCenter);
    column_->addWidget(title, 0, Qt::AlignHCenter);
    column_->addWidget(body, 0, Qt::AlignHCenter);

    auto* centred = new QVBoxLayout(this);
    centred->addStretch();
    centred->addLayout(column_);
    centred->addStretch();
}

void EmptyState::Retell(const QString& headline, const QString& explanation)
{
    title_->setText(headline);
    body_->setText(explanation);
    body_->setMinimumHeight(std::max(0, body_->heightForWidth(kReadableWidth)));
}

QPushButton* EmptyState::OfferTheOnlyAction()
{
    auto* action = new QPushButton(this);
    action->setProperty("role", "primary");
    action->setCursor(Qt::PointingHandCursor);

    column_->addSpacing(4);
    column_->addWidget(action, 0, Qt::AlignHCenter);

    return action;
}
