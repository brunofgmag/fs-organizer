#include "view/library/SharedAirportsDialog.h"

#include <algorithm>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include "view/HeadingsOverTheColumns.h"
#include "view/ListUnderItsHeadings.h"
#include "view/ScrollThatReportsItsContent.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kDialogWidth = 700;
    constexpr int kAtMostSpelledOut = 4;

    [[nodiscard]] QString AsAirports(const QStringList& codes)
    {
        if (codes.size() <= kAtMostSpelledOut)
        {
            return codes.join(QStringLiteral(", "));
        }

        return QObject::tr("%1 and %n more", nullptr, static_cast<int>(codes.size()) - kAtMostSpelledOut)
            .arg(codes.mid(0, kAtMostSpelledOut).join(QStringLiteral(", ")));
    }

    [[nodiscard]] std::vector<AddonId> WhatWasJustEnabled(const std::vector<SharedAirportsLine>& shared)
    {
        std::vector<AddonId> enabled;

        for (const SharedAirportsLine& line : shared)
        {
            if (std::ranges::find(enabled, line.one) == enabled.end())
            {
                enabled.push_back(line.one);
            }
        }

        return enabled;
    }
}

SharedAirportsDialog::SharedAirportsDialog(const std::vector<SharedAirportsLine>& shared, QWidget* parent)
    : QDialog(parent), enabled_(WhatWasJustEnabled(shared))
{
    setWindowTitle(tr("Two addons for the same airport"));

    const int enabled = static_cast<int>(enabled_.size());

    auto* explanation = new QLabel(tr("%n addon you just enabled covers the same airport as another enabled addon. "
                                      "Only addons whose scenery has already been scanned are compared.",
                                      nullptr, enabled),
                                   this);
    explanation->setWordWrap(true);

    const QStringList named = {tr("Can coexist"), tr("You enabled"), tr("Already enabled"), tr("Airports")};
    auto* headings = new HeadingsOverTheColumns(named, this);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    headings->ShapeTheGridUnder(*grid);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    rows_.reserve(shared.size());

    int row = 0;
    for (const SharedAirportsLine& line : shared)
    {
        auto* box = new QCheckBox(listed);
        box->setAccessibleName(tr("%1 and %2 can coexist").arg(line.turningOn, line.alreadyOn));
        grid->addWidget(box, row, 0, Qt::AlignTop);

        rows_.push_back({.box = box, .pair = {.one = line.one, .other = line.other}});

        auto* yours = new QLabel(line.turningOn, listed);
        yours->setWordWrap(true);
        grid->addWidget(yours, row, 1, Qt::AlignTop);

        auto* theirs = new QLabel(line.alreadyOn, listed);
        theirs->setWordWrap(true);
        grid->addWidget(theirs, row, 2, Qt::AlignTop);

        auto* airports = new QLabel(AsAirports(line.codes), listed);
        airports->setObjectName(QStringLiteral("PanelPromise"));
        airports->setWordWrap(true);
        grid->addWidget(airports, row, 3, Qt::AlignTop);

        if (row == 0)
        {
            headings->StandOver({box, yours, theirs, airports});
        }

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->MeasureTheContentAt(kDialogWidth - 2 * kPageGutter - scroll->verticalScrollBar()->sizeHint().width());

    auto* promise = new QLabel(
        tr("Both stay enabled, and the simulator decides which one loads. Check a pair to stop the warnings about it."),
        this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* whatDisablingDoes =
        new QLabel(tr("Disabling what you enabled changes nothing that was already enabled."), this);
    whatDisablingDoes->setObjectName(QStringLiteral("PanelPromise"));
    whatDisablingDoes->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* leave = buttons->addButton(tr("Keep both enabled"), QDialogButtonBox::RejectRole);
    leave->setProperty("role", "primary");
    leave->setDefault(true);

    QPushButton* remember = buttons->addButton(tr("Stop warning about the checked ones"), QDialogButtonBox::AcceptRole);
    remember->setEnabled(false);

    QPushButton* turnBackOff =
        buttons->addButton(tr("Disable the %n you enabled", nullptr, enabled), QDialogButtonBox::ResetRole);
    turnBackOff->setObjectName(QStringLiteral("DisableWhatWasEnabled"));

    const auto SayHowManyAreChecked = [this, remember]
    {
        remember->setEnabled(!Chosen().empty());
    };

    for (const Row& each : rows_)
    {
        connect(each.box, &QCheckBox::toggled, this, SayHowManyAreChecked);
    }

    auto* all = new QCheckBox(tr("Check all"), this);
    all->setVisible(shared.size() > 1);

    connect(all, &QCheckBox::clicked, this,
            [this](const bool checked)
            {
                for (const Row& each : rows_)
                {
                    each.box->setChecked(checked);
                }
            });

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(turnBackOff, &QPushButton::clicked, this,
            [this]
            {
                done(static_cast<int>(SharedAirportsAnswer::DisableWhatWasEnabled));
            });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(all);
    layout->addLayout(ListUnderItsHeadings(*headings, *scroll, *this), 1);
    layout->addWidget(promise);
    layout->addWidget(whatDisablingDoes);
    layout->addWidget(buttons);

    SizeToTheContent(*this, kDialogWidth);
}

std::vector<CoexistingPair> SharedAirportsDialog::Chosen() const
{
    std::vector<CoexistingPair> chosen;

    for (const Row& each : rows_)
    {
        if (each.box->isChecked())
        {
            chosen.push_back(each.pair);
        }
    }

    return chosen;
}

const std::vector<AddonId>& SharedAirportsDialog::WhatWasEnabled() const
{
    return enabled_;
}
