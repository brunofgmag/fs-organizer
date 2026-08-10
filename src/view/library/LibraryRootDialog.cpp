#include "view/library/LibraryRootDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"

namespace
{
    const std::filesystem::path kAShortRoot{"D:/MSFS 2024"};

    void LetNoButtonAnswerTheEnterKey(const QDialogButtonBox& buttons)
    {
        for (QAbstractButton* button : buttons.buttons())
        {
            auto* pushed = qobject_cast<QPushButton*>(button);
            pushed->setAutoDefault(false);
            pushed->setDefault(false);
        }
    }

    QString HowMuchItLeaves(const RootDepth& depth)
    {
        return QObject::tr("%n character", nullptr, static_cast<int>(depth.characters)) + QStringLiteral("\n")
            + QObject::tr("leaves %n for the addon", nullptr, static_cast<int>(depth.leavesForTheAddon));
    }

    QString TheShortRootForComparison()
    {
        return AsText(kAShortRoot) + QStringLiteral(" · ") + HowMuchItLeaves(MeasureTheRoot(kAShortRoot));
    }
}

LibraryRootDialog::LibraryRootDialog(const std::filesystem::path& root, const RootDepth& depth, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add a library"));

    auto* name = new QLabel(AsText(root), this);
    name->setObjectName(QStringLiteral("PanelTitle"));
    name->setWordWrap(true);
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* explanation = new QLabel(tr("This folder is %1 deep. Addons routinely nest %2 characters below their own "
                                      "folder, and Windows stops at %3 for some operations, including the Recycle Bin.")
                                       .arg(tr("%n character", nullptr, static_cast<int>(depth.characters)))
                                       .arg(kAddonsRoutinelyNest)
                                       .arg(kTheRecycleBinStopsAt),
                                   this);
    explanation->setWordWrap(true);

    auto* compared = new QWidget(this);
    auto* grid = new QGridLayout(compared);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    AddTheSide(*grid, 0, tr("This folder"), HowMuchItLeaves(depth));
    AddTheSide(*grid, 1, tr("A short root, for comparison"), TheShortRootForComparison());

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* another = buttons->addButton(tr("Pick another folder"), QDialogButtonBox::RejectRole);
    another->setObjectName(QStringLiteral("PickAnotherFolder"));
    QPushButton* keep = buttons->addButton(tr("Use this one"), QDialogButtonBox::AcceptRole);
    keep->setObjectName(QStringLiteral("UseThisRoot"));
    GiveItTheRole(keep, QStringLiteral("primary"));

    LetNoButtonAnswerTheEnterKey(*buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(name);
    layout->addWidget(explanation);
    layout->addWidget(compared);
    layout->addStretch();
    layout->addWidget(buttons);

    SizeToTheContent(*this, 520);
}

void LibraryRootDialog::AddTheSide(QGridLayout& grid, const int column, const QString& title, const QString& said)
{
    auto* heading = new QLabel(title, grid.parentWidget());
    heading->setObjectName(QStringLiteral("DetailFieldName"));

    auto* value = new QLabel(said, grid.parentWidget());
    value->setObjectName(QStringLiteral("DetailFieldValue"));
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);

    grid.addWidget(heading, 0, column);
    grid.addWidget(value, 1, column, Qt::AlignTop);
}

void LibraryRootDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    LetNoButtonAnswerTheEnterKey(*findChild<QDialogButtonBox*>());
}
