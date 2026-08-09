#include "view/quarantine/CollisionDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    void LetNoButtonAnswerTheEnterKey(const QDialogButtonBox& buttons)
    {
        for (QAbstractButton* button : buttons.buttons())
        {
            auto* pushed = qobject_cast<QPushButton*>(button);
            pushed->setAutoDefault(false);
            pushed->setDefault(false);
        }
    }

    QString VersionOrSilence(const std::string& version)
    {
        return version.empty() ? QObject::tr("the manifest does not say") : QString::fromStdString(version);
    }

    QString SizeOrSilence(const MeasuredFolder& measured)
    {
        return measured.measured ? AsSize(measured.bytes) : QObject::tr("it could not be measured");
    }

    QString SideText(const QString& version, const QString& size)
    {
        return QObject::tr("Version %1\nSize %2").arg(version, size);
    }
}

CollisionDialog::CollisionDialog(const RestoreCheck& check, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Restoring would collide"));

    auto* name = new QLabel(AsText(check.item.path.filename()), this);
    name->setObjectName(QStringLiteral("PanelTitle"));
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* explanation =
        new QLabel(tr("Something with this name is already in %1. Restoring would put two addons in one place.")
                       .arg(AsText(check.occupant.parent_path())),
                   this);
    explanation->setWordWrap(true);

    auto* compared = new QWidget(this);
    auto* grid = new QGridLayout(compared);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    held_ = AddTheSide(*grid, 0, tr("In quarantine"), VersionOrSilence(check.version));
    occupant_ = AddTheSide(*grid, 1, tr("Already there"), VersionOrSilence(check.occupantVersion));

    auto* promise =
        new QLabel(tr("Replacing puts what is there in the quarantine, with its own origin recorded."), this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* replace = buttons->addButton(tr("Replace what's there"), QDialogButtonBox::AcceptRole);
    replace->setObjectName(QStringLiteral("ReplaceWhatIsThere"));

    LetNoButtonAnswerTheEnterKey(*buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(name);
    layout->addWidget(explanation);
    layout->addWidget(compared);
    layout->addWidget(promise);
    layout->addStretch();
    layout->addWidget(buttons);

    ShowTheSizes(TwoSides{});

    SizeToTheContent(*this, 520);
}

QLabel* CollisionDialog::AddTheSide(QGridLayout& grid, const int column, const QString& title, const QString& version)
{
    auto* heading = new QLabel(title, grid.parentWidget());
    heading->setObjectName(QStringLiteral("DetailFieldName"));

    auto* said = new QLabel(grid.parentWidget());
    said->setObjectName(QStringLiteral("DetailFieldValue"));
    said->setTextInteractionFlags(Qt::TextSelectableByMouse);
    said->setProperty("version", version);

    grid.addWidget(heading, 0, column);
    grid.addWidget(said, 1, column, Qt::AlignTop);

    return said;
}

void CollisionDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    LetNoButtonAnswerTheEnterKey(*findChild<QDialogButtonBox*>());
}

void CollisionDialog::ShowTheSizes(const TwoSides& sides)
{
    held_->setText(SideText(held_->property("version").toString(), SizeOrSilence(sides.held)));
    occupant_->setText(SideText(occupant_->property("version").toString(), SizeOrSilence(sides.occupant)));
}
