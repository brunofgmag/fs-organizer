#include "view/community/ConflictDialog.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <QtCore/QStringList>

#include "domain/model/PackageVersion.h"
#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/TextThatIsNeverCut.h"
#include "view/theme/ModernistMetrics.h"

namespace
{
    QString Moment(const std::optional<std::chrono::system_clock::time_point>& when)
    {
        if (!when.has_value())
        {
            return QObject::tr("(unknown)");
        }

        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(when->time_since_epoch()).count();

        return QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::UTC)
            .toLocalTime()
            .toString(QStringLiteral("dd/MM/yyyy HH:mm"));
    }

    struct Wording
    {
        QString title{};
        QString explanation{};
        QString provenanceSide{};
        QString keepTheProvenanceOne{};
        QString warning{};
    };

    Wording WordingWhenOurLinkWasReplaced()
    {
        return Wording{
            .title = ConflictDialog::tr("Something replaced the link"),
            .explanation =
                QObject::tr("This folder used to be a link into your library, and something else wrote a real folder "
                            "over it. The simulator now loads that folder, and the copy in the library is adrift: it "
                            "answers no switch, enters no preset and joins no bisection."),
            .provenanceSide = QObject::tr("Folder that stands where the link was"),
            .keepTheProvenanceOne = QObject::tr("Take it into the library and link it back"),
            .warning = QObject::tr("The library copy is enabled in %1. Taking the destination one back moves the old "
                                   "copy to the quarantine first.")};
    }

    Wording WordingFor(const ConflictDetails& details)
    {
        if (details.ourLinkWasReplaced)
        {
            return WordingWhenOurLinkWasReplaced();
        }

        if (details.theProvenanceIsAnotherProgram)
        {
            return Wording{
                .title = ConflictDialog::tr("Two copies of the same addon"),
                .explanation =
                    QObject::tr("The other program put a real folder back where it installs this addon, and your copy "
                                "is still in the library. Choose which one stays: the other goes to the quarantine."),
                .provenanceSide = QObject::tr("Copy in the other program's folder"),
                .keepTheProvenanceOne = QObject::tr("Keep the other program's one"),
                .warning = QObject::tr("The library copy is enabled in %1. Keeping the other program's one removes "
                                       "those links before sending it to the quarantine.")};
        }

        return Wording{
            .title = ConflictDialog::tr("Two copies of the same addon"),
            .explanation =
                QObject::tr("There is a real folder in the destination and an addon with the same name in the library. "
                            "Choose which one stays: the other goes to the quarantine."),
            .provenanceSide = QObject::tr("Copy in the destination"),
            .keepTheProvenanceOne = QObject::tr("Keep the destination one"),
            .warning = QObject::tr("The library copy is enabled in %1. Keeping the destination one removes those links "
                                   "before sending it to the quarantine.")};
    }

    QString WarningAbout(const QString& sentence, const std::vector<std::filesystem::path>& links)
    {
        QStringList destinations;
        for (const std::filesystem::path& link : links)
        {
            destinations.append(AsText(link.parent_path().filename()));
        }

        return sentence.arg(destinations.join(QStringLiteral(", ")));
    }

    QString WhatTheVersionsSettle(const ConflictDetails& details)
    {
        if (!details.ourLinkWasReplaced)
        {
            return {};
        }

        switch (
            HowTheVersionCompares(details.provenance.manifest.packageVersion, details.library.manifest.packageVersion))
        {
        case VersionOrder::TheSame:
            return QObject::tr("Both copies declare the same version, so nothing here says which one is newer. If "
                               "anything changed, it changed inside the folder.");
        case VersionOrder::NoOneCanTell:
            return QObject::tr("The manifests do not both name a version, so nothing here says which one is newer. "
                               "If anything changed, it changed inside the folder.");
        case VersionOrder::Newer:
        case VersionOrder::Older: break;
        }

        return {};
    }

    QString Version(const Manifest& manifest)
    {
        return manifest.packageVersion.empty() ? QObject::tr("(no version in the manifest)")
                                               : QString::fromStdString(manifest.packageVersion);
    }

    QPushButton* TheNewerSideAmong(QPushButton* provenance, QPushButton* library, const ConflictDetails& details)
    {
        switch (
            HowTheVersionCompares(details.provenance.manifest.packageVersion, details.library.manifest.packageVersion))
        {
        case VersionOrder::Newer: return provenance;
        case VersionOrder::Older: return library;
        case VersionOrder::TheSame:
        case VersionOrder::NoOneCanTell: break;
        }

        return nullptr;
    }
}

ConflictDialog::ConflictDialog(const ConflictDetails& details, QWidget* parent) : QDialog(parent)
{
    const Wording wording = WordingFor(details);

    setWindowTitle(wording.title);

    auto* explanation = new QLabel(wording.explanation, this);
    explanation->setWordWrap(true);

    const QString settled = WhatTheVersionsSettle(details);

    auto* versions = new QLabel(settled, this);
    versions->setWordWrap(true);
    versions->setVisible(!settled.isEmpty());

    auto* sides = new QHBoxLayout;
    sides->addWidget(CreateSide(wording.provenanceSide, details.provenance));
    sides->addWidget(CreateSide(tr("Copy in the library"), details.library));

    auto* warning = new QLabel(WarningAbout(wording.warning, details.linksToTheLibraryCopy), this);
    warning->setWordWrap(true);
    warning->setVisible(!details.linksToTheLibraryCopy.empty());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);

    const bool theTakeBackIsOffered = !details.ourLinkWasReplaced
        || TakingItBackIsWorthOffering(details.provenance.manifest.packageVersion,
                                       details.library.manifest.packageVersion);

    QPushButton* keepDestination = buttons->addButton(wording.keepTheProvenanceOne, QDialogButtonBox::AcceptRole);
    keepDestination->setVisible(theTakeBackIsOffered);

    QPushButton* keepLibrary = buttons->addButton(
        details.ourLinkWasReplaced ? tr("Put the link back over the library copy") : tr("Keep the library one"),
        QDialogButtonBox::AcceptRole);

    QPushButton* cancel = buttons->button(QDialogButtonBox::Cancel);
    for (QPushButton* button : {cancel, keepDestination, keepLibrary})
    {
        button->setAutoDefault(false);
    }

    QPushButton* answersTheEnterKey = TheNewerSideAmong(keepDestination, keepLibrary, details);
    if (answersTheEnterKey == nullptr)
    {
        answersTheEnterKey = cancel;
    }
    answersTheEnterKey->setDefault(true);

    connect(keepLibrary, &QPushButton::clicked, this,
            [this]
            {
                choice_ = ConflictChoice::KeepTheLibraryCopy;
                accept();
            });
    connect(keepDestination, &QPushButton::clicked, this,
            [this]
            {
                choice_ = ConflictChoice::KeepTheProvenanceCopy;
                accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addWidget(versions);
    layout->addLayout(sides, 1);
    layout->addWidget(warning);
    layout->addWidget(buttons);

    SizeToTheContent(*this, 760);
}

QGroupBox* ConflictDialog::CreateSide(const QString& title, const ConflictSide& side)
{
    auto* group = new QGroupBox(title, this);

    auto* path = new TextThatIsNeverCut(AsText(side.path), group);

    auto* form = new QFormLayout(group);
    form->addRow(tr("Path:"), path);
    form->addRow(tr("Version:"), new QLabel(Version(side.manifest), group));
    form->addRow(tr("Size:"), new QLabel(AsSize(side.sizeBytes), group));
    form->addRow(tr("Changed on:"), new QLabel(Moment(side.modified), group));

    if (!side.manifest.title.empty())
    {
        form->addRow(tr("Title:"), new QLabel(QString::fromStdString(side.manifest.title), group));
    }

    return group;
}

ConflictChoice ConflictDialog::Choice() const
{
    return choice_;
}
