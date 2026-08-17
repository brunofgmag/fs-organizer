#include "view/library/SharedAirportsDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "view/ScrollThatReportsItsContent.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kDialogWidth = 620;
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
    grid->setColumnStretch(2, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    for (const auto& [column, heading] :
         {std::pair{0, tr("You turned on")}, std::pair{1, tr("Already on")}, std::pair{2, tr("Airports")}})
    {
        auto* label = new QLabel(heading, listed);
        label->setObjectName(QStringLiteral("PanelSubHeading"));
        grid->addWidget(label, 0, column);
    }

    int row = 1;
    for (const SharedAirportsLine& line : shared)
    {
        auto* yours = new QLabel(line.turningOn, listed);
        yours->setWordWrap(true);
        grid->addWidget(yours, row, 0, Qt::AlignTop);

        auto* theirs = new QLabel(line.alreadyOn, listed);
        theirs->setWordWrap(true);
        grid->addWidget(theirs, row, 1, Qt::AlignTop);

        auto* airports = new QLabel(AsAirports(line.codes), listed);
        airports->setObjectName(QStringLiteral("PanelPromise"));
        airports->setWordWrap(true);
        grid->addWidget(airports, row, 2, Qt::AlignTop);

        ++row;
    }

    grid->setRowStretch(row, 1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setWidget(listed);
    scroll->setWidgetResizable(true);
    scroll->MeasureTheContentAt(kDialogWidth - 2 * kPageGutter);

    auto* promise = new QLabel(tr("Nothing was undone and both stay on: which one the simulator loads is its own to "
                                  "decide, and turning one off is the switch you already use. Saying they can "
                                  "coexist keeps the app quiet about this pair from now on."),
                               this);
    promise->setObjectName(QStringLiteral("PanelPromise"));
    promise->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* leave = buttons->addButton(tr("Leave them both on"), QDialogButtonBox::RejectRole);
    leave->setProperty("role", "primary");
    leave->setDefault(true);
    buttons->addButton(shared.size() == 1 ? tr("They can coexist") : tr("They can all coexist"),
                       QDialogButtonBox::AcceptRole);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(scroll, 1);
    layout->addWidget(promise);
    layout->addWidget(buttons);

    SizeToTheContent(*this, kDialogWidth);
}
