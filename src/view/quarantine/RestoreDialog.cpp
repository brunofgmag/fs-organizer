#include "view/quarantine/RestoreDialog.h"

#include <QtCore/QStringList>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/FailureText.h"

namespace
{
    constexpr int kNoPlaceChosen = 0;

    QString VersionsOf(const RestoreCheck& check)
    {
        QStringList sides;

        if (!check.version.empty())
        {
            sides.append(QObject::tr("in quarantine: %1").arg(QString::fromStdString(check.version)));
        }

        if (!check.occupantVersion.empty())
        {
            sides.append(QObject::tr("already there: %1").arg(QString::fromStdString(check.occupantVersion)));
        }

        return sides.join(QStringLiteral(" · "));
    }

    QString WhyItIsRefused(const RestoreCheck& check)
    {
        QStringList lines;
        lines.append(Explain(check.result));

        if (!check.occupant.empty())
        {
            lines.append(QObject::tr("it is in %1").arg(AsText(check.occupant)));
        }

        const QString versions = VersionsOf(check);
        if (!versions.isEmpty())
        {
            lines.append(versions);
        }

        return lines.join(QStringLiteral("\n"));
    }
}

RestoreDialog::RestoreDialog(const std::vector<RestoreOffer>& offers, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Restore from the quarantine"));

    auto* explanation =
        new QLabel(tr("Each folder goes back to where it came from. Nothing is overwritten: what would collide is "
                      "listed here with both versions, and the choice between them is yours."),
                   this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    int row = 0;
    for (const RestoreOffer& offer : offers)
    {
        if (offer.check.NeedsAPlace())
        {
            asked_.push_back(Choice{.offer = offer, .places = new QComboBox(listed)});
            AddTheQuestionRow(*grid, asked_.back(), row);
        }
        else
        {
            settled_.push_back(offer);
            AddTheSettledRow(*grid, offer, row);
        }

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);

    counted_ = new QLabel(this);
    counted_->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    restore_ = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
    restore_->setDefault(true);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for (const Choice& choice : asked_)
    {
        connect(choice.places, &QComboBox::currentIndexChanged, this,
                [this]
                {
                    ShowHowManyWillGoBack();
                });
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(counted_);
    layout->addWidget(buttons);

    ShowHowManyWillGoBack();

    resize(660, 420);
}

void RestoreDialog::AddTheSettledRow(QGridLayout& grid, const RestoreOffer& offer, const int row)
{
    auto* name = new QLabel(AsText(offer.check.item.path.filename()), grid.parentWidget());
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid.addWidget(name, row, 0, Qt::AlignTop);

    const bool proceeds = offer.check.CanProceed();

    auto* detail = new QLabel(proceeds ? tr("goes back to %1").arg(AsText(offer.check.target.parent_path()))
                                       : WhyItIsRefused(offer.check),
                              grid.parentWidget());
    detail->setObjectName(QStringLiteral("PanelPromise"));
    detail->setWordWrap(true);
    detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid.addWidget(detail, row, 1);
}

void RestoreDialog::AddTheQuestionRow(QGridLayout& grid, const Choice& choice, const int row)
{
    auto* name = new QLabel(AsText(choice.offer.check.item.path.filename()), grid.parentWidget());
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid.addWidget(name, row, 0, Qt::AlignTop);

    choice.places->addItem(tr("Choose where this goes back to"));

    for (const RestorePlace& place : choice.offer.places)
    {
        choice.places->addItem(AsText(place.label.empty() ? place.place : place.label));
    }

    grid.addWidget(choice.places, row, 1);
}

void RestoreDialog::ShowHowManyWillGoBack() const
{
    const auto going = static_cast<int>(Restorable().size());

    restore_->setEnabled(going > 0);
    counted_->setText(tr("%n folder will be restored.", nullptr, going));
}

std::vector<QuarantinedItem> RestoreDialog::Restorable() const
{
    std::vector<QuarantinedItem> items;

    for (const RestoreOffer& offer : settled_)
    {
        if (offer.check.CanProceed())
        {
            items.push_back(offer.check.item);
        }
    }

    for (const Choice& choice : asked_)
    {
        const int chosen = choice.places->currentIndex();
        if (chosen == kNoPlaceChosen)
        {
            continue;
        }

        QuarantinedItem item = choice.offer.check.item;
        item.origin = choice.offer.places[static_cast<std::size_t>(chosen - 1)].target;

        items.push_back(std::move(item));
    }

    return items;
}
