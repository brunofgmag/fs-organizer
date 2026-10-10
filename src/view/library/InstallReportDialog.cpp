#include "view/library/InstallReportDialog.h"

#include <algorithm>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "view/ScrollThatReportsItsContent.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kWide = 520;
    constexpr int kBetweenArchives = 12;
    constexpr int kBetweenLines = 4;
    constexpr int kUnderAnArchive = 20;

    [[nodiscard]] QLabel* ALine(const QString& said, const QString& name, QWidget* on)
    {
        auto* line = new QLabel(said, on);
        line->setObjectName(name);
        line->setWordWrap(true);
        line->setTextInteractionFlags(Qt::TextSelectableByMouse);

        return line;
    }

    [[nodiscard]] QString TitleOf(const InstallReportKind kind)
    {
        switch (kind)
        {
        case InstallReportKind::NothingToInstall: return InstallReportDialog::tr("Nothing to install");
        case InstallReportKind::SomeWereNotInstalled: return InstallReportDialog::tr("Some addons were not installed");
        case InstallReportKind::NothingWasInstalled: return InstallReportDialog::tr("Nothing was installed");
        case InstallReportKind::InstalledButNotLinked: return InstallReportDialog::tr("Installed, but not enabled");
        case InstallReportKind::InstalledButAFolderStayed:
            return InstallReportDialog::tr("Installed, but a temporary folder was left behind");
        }

        return {};
    }

    [[nodiscard]] int LinesOf(const InstallReport& report)
    {
        int lines = 0;

        for (const InstallReportArchive& archive : report.archives)
        {
            lines += static_cast<int>(archive.lines.size());
        }

        return lines;
    }

    [[nodiscard]] QString HeadlineOf(const InstallReport& report)
    {
        switch (report.kind)
        {
        case InstallReportKind::SomeWereNotInstalled:
            return QStringLiteral("%1 %2").arg(
                InstallReportDialog::tr("%n addon is now in the library.", nullptr, report.installed),
                InstallReportDialog::tr("%n addon was not installed:", nullptr, report.notInstalled));
        case InstallReportKind::InstalledButNotLinked:
            return InstallReportDialog::tr(
                "%n addon is in the library, but its links could not be made again:", nullptr, LinesOf(report));
        case InstallReportKind::InstalledButAFolderStayed:
            return InstallReportDialog::tr("%n addon is now in the library.", nullptr, report.installed);
        case InstallReportKind::NothingToInstall:
        case InstallReportKind::NothingWasInstalled: return {};
        }

        return {};
    }

    [[nodiscard]] bool AFolderStayedBehind(const InstallReport& report)
    {
        return std::ranges::any_of(report.archives,
                                   [](const InstallReportArchive& archive)
                                   {
                                       return !archive.leftBehind.isEmpty();
                                   });
    }

    [[nodiscard]] bool PromisesThatNothingChanged(const InstallReport& report)
    {
        if (AFolderStayedBehind(report))
        {
            return false;
        }

        return report.kind == InstallReportKind::NothingToInstall
            || report.kind == InstallReportKind::NothingWasInstalled;
    }
}

InstallReportDialog::InstallReportDialog(const InstallReport& report, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(TitleOf(report.kind));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);

    if (const QString headline = HeadlineOf(report); !headline.isEmpty())
    {
        layout->addWidget(ALine(headline, QStringLiteral("ReportHeadline"), this));
    }

    auto* listed = new QWidget(this);
    auto* blocks = new QVBoxLayout(listed);
    blocks->setContentsMargins(0, 0, 0, 0);
    blocks->setSpacing(kBetweenArchives);

    for (const InstallReportArchive& archive : report.archives)
    {
        auto* block = new QVBoxLayout;
        block->setContentsMargins(0, 0, 0, 0);
        block->setSpacing(kBetweenLines);
        block->addWidget(ALine(archive.fileName, QStringLiteral("SectionHeadline"), listed));

        for (const InstallReportLine& line : archive.lines)
        {
            auto* under = new QVBoxLayout;
            under->setContentsMargins(line.addon.isEmpty() ? 0 : kUnderAnArchive, 0, 0, 0);
            under->setSpacing(0);

            if (!line.addon.isEmpty())
            {
                under->addWidget(ALine(line.addon, QStringLiteral("ReportAddon"), listed));
            }

            under->addWidget(ALine(line.reason, QStringLiteral("ReportReason"), listed));

            block->addLayout(under);
        }

        if (!archive.leftBehind.isEmpty())
        {
            block->addWidget(ALine(archive.leftBehind, QStringLiteral("ReportLeftBehind"), listed));
        }

        blocks->addLayout(block);
    }

    blocks->addStretch(1);

    auto* scroll = new ScrollThatReportsItsContent(this);
    scroll->setObjectName(QStringLiteral("ReportList"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(listed);
    scroll->setMaximumHeight(listed->heightForWidth(kWide - 2 * kPageGutter));
    scroll->MeasureTheContentAt(kWide - 2 * kPageGutter);

    layout->addWidget(scroll, 1);

    if (PromisesThatNothingChanged(report))
    {
        layout->addWidget(ALine(tr("Nothing changed in the library, and what you dropped is where it was."),
                                QStringLiteral("ReportPromise"), this));
    }

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* close = buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    close->setObjectName(QStringLiteral("CloseTheReport"));
    close->setDefault(true);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttons);

    SizeToTheContent(*this, kWide);
}
