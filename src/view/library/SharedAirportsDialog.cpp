#include "view/library/SharedAirportsDialog.h"

#include <algorithm>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

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
}

SharedAirportsDialog::SharedAirportsDialog(const std::vector<SharedAirportsLine>& shared, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Two addons for the same place"));

    auto* explanation = new QLabel(
        tr("%n addon you just turned on covers a place another addon of yours already covers. The codes are read "
           "from inside the scenery files, and the app compares against the addons whose scenery it has read so "
           "far.",
           nullptr, static_cast<int>(shared.size())),
        this);
    explanation->setWordWrap(true);

    auto* listed = new QWidget(this);
    auto* grid = new QGridLayout(listed);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(3, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    for (const auto& [column, heading] : {std::pair{0, tr("Can coexist")}, std::pair{1, tr("You turned on")},
                                          std::pair{2, tr("Already on")}, std::pair{3, tr("Airports")}})
    {
        auto* label = new QLabel(heading, listed);
        label->setObjectName(QStringLiteral("PanelSubHeading"));
        grid->addWidget(label, 0, column);
    }

    rows_.reserve(shared.size());

    int row = 1;
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

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->MeasureTheContentAt(kDialogWidth - 2 * kPageGutter);

    auto* promise = new QLabel(tr("Nothing was undone and both stay on: which one the simulator loads is its own to "
                                  "decide, and turning one off is the switch you already use. Checking a pair keeps "
                                  "the app quiet about that pair from now on, and leaves the others alone."),
                               this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* leave = buttons->addButton(tr("Leave them both on"), QDialogButtonBox::RejectRole);
    leave->setProperty("role", "primary");
    leave->setDefault(true);

    QPushButton* remember = buttons->addButton(tr("Remember the checked ones"), QDialogButtonBox::AcceptRole);
    remember->setEnabled(false);

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

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(all);
    layout->addWidget(scroll, 1);
    layout->addWidget(promise);
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
