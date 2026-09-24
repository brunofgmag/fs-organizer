#include "view/diagnostics/LoadPanel.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/MomentText.h"
#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/delegates/RowDelegate.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr int kThereIsNoReport = 0;
    constexpr int kTheModulesAreListed = 1;

    [[nodiscard]] QLabel* Quiet(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(QStringLiteral("PanelPromise"));
        label->setWordWrap(true);

        return label;
    }
}

LoadPanel::LoadPanel(QWidget* parent) : QWidget(parent)
{
    refusal_ = Quiet(this);
    registered_ = Quiet(this);
    empty_ = Quiet(this);
    empty_->setAlignment(Qt::AlignTop);
    empty_->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);

    modules_ = new QTreeWidget(this);
    modules_->setObjectName(QStringLiteral("DiagnosticsModules"));
    modules_->setRootIsDecorated(false);
    modules_->setUniformRowHeights(true);
    modules_->setColumnCount(4);
    modules_->header()->setStretchLastSection(false);
    modules_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    modules_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    modules_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    modules_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    DressTheHeaderOf(modules_->header());
    modules_->setItemDelegate(new RowDelegate(modules_));

    body_ = new QStackedWidget(this);
    body_->addWidget(empty_);
    body_->addWidget(modules_);

    auto* prose = new QVBoxLayout;
    prose->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    prose->setSpacing(8);
    prose->addWidget(refusal_);
    prose->addWidget(registered_);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(prose);
    column->addWidget(body_, 1);

    RetranslateUi();
}

void LoadPanel::Show(const LoadDiagnostics& load)
{
    load_ = load;

    ShowWhatTheReportAttributes();
}

void LoadPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

void LoadPanel::RetranslateUi()
{
    refusal_->setText(tr("The simulator does not report loading time per package. This shows which module each package "
                         "loaded and how much memory it holds."));
    empty_->setText(tr("The simulator only writes this report after a slow load, so there may not be one yet."));
    modules_->setHeaderLabels({tr("Module"), tr("Package"), tr("Addon"), tr("Memory")});
    modules_->headerItem()->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);

    ShowWhatTheReportAttributes();
}

void LoadPanel::ShowWhatTheReportAttributes() const
{
    body_->setCurrentIndex(load_.reportWasRead ? kTheModulesAreListed : kThereIsNoReport);
    registered_->setVisible(load_.reportWasRead);

    if (!load_.reportWasRead)
    {
        return;
    }

    const QString counted =
        tr("%n package registered by the simulator", nullptr, static_cast<int>(load_.packagesRegistered));
    const QString said =
        load_.runAt.has_value() ? tr("%1, in the session of %2").arg(counted, AsMoment(*load_.runAt)) : counted;

    registered_->setText(
        tr("%1. This is what the simulator registered in that session, not the number of addons in your library.")
            .arg(said));

    modules_->clear();

    for (const ModuleLine& line : load_.modules)
    {
        auto* row = new QTreeWidgetItem(modules_);
        row->setText(0, QString::fromStdString(line.moduleName));
        row->setText(1, QString::fromStdString(line.packageName));
        row->setText(2, line.addonUnderLibrary.empty() ? tr("not in your library") : AsText(line.addonUnderLibrary));
        row->setText(3, line.memoryBytes.has_value() ? AsSize(*line.memoryBytes) : QString());
        row->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        row->setData(1, QuietRole, true);
        row->setData(3, QuietRole, true);

        if (line.addonUnderLibrary.empty())
        {
            row->setData(2, QuietRole, true);
        }
    }
}
