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

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    column->setSpacing(8);
    column->addWidget(refusal_);
    column->addWidget(registered_);
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
    refusal_->setText(
        tr("The simulator's report attributes no loading time to a package, so this screen shows none. "
           "What it does attribute is the module each package loaded, and the memory that module holds."));
    empty_->setText(tr("The simulator writes this report only when a load takes long, so there may be none yet. "
                       "Everything else on this screen works without it."));
    modules_->setHeaderLabels({tr("Module"), tr("Package"), tr("Addon"), tr("Memory")});

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
        load_.runAt.has_value() ? tr("%1, on the run of %2").arg(counted, AsMoment(*load_.runAt)) : counted;

    registered_->setText(
        tr("%1. It counts what the simulator registered on that run, which is not the number of addons in your "
           "library.")
            .arg(said));

    modules_->clear();

    for (const ModuleLine& line : load_.modules)
    {
        auto* row = new QTreeWidgetItem(modules_);
        row->setText(0, QString::fromStdString(line.moduleName));
        row->setText(1, QString::fromStdString(line.packageName));
        row->setText(2, line.addonUnderLibrary.empty() ? tr("not one of yours") : AsText(line.addonUnderLibrary));
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
