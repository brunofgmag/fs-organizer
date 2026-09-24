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
            .title = ConflictDialog::tr("The link was replaced"),
            .explanation = QObject::tr("This folder was a link to your library until something wrote a regular folder "
                                       "over it. The simulator now loads that folder and ignores your library copy."),
            .provenanceSide = QObject::tr("Folder in place of the link"),
            .keepTheProvenanceOne = QObject::tr("Move this folder into the library"),
            .warning = QObject::tr("The library copy is enabled in %1. It is moved to the quarantine first.")};
    }

    Wording WordingFor(const ConflictDetails& details)
    {
        if (details.ourLinkWasReplaced)
        {
            return WordingWhenOurLinkWasReplaced();
        }

        if (details.theProvenanceIsAnotherProgram)
        {
            return Wording{.title = ConflictDialog::tr("Two copies of the same addon"),
                           .explanation = QObject::tr(
                               "The program that installed this addon put its own copy back, and yours is still in the "
                               "library. Choose which one to keep; the other goes to the quarantine."),
                           .provenanceSide = QObject::tr("Copy in the other program's folder"),
                           .keepTheProvenanceOne = QObject::tr("Keep the other program's copy"),
                           .warning = QObject::tr("The library copy is enabled in %1. Keeping the other program's copy "
                                                  "removes those links and moves the library copy to the quarantine.")};
        }

        return Wording{.title = ConflictDialog::tr("Two copies of the same addon"),
                       .explanation =
                           QObject::tr("The destination has a regular folder with the same name as an addon in your "
                                       "library. Choose which one to keep; the other goes to the quarantine."),
                       .provenanceSide = QObject::tr("Copy in the destination"),
                       .keepTheProvenanceOne = QObject::tr("Keep the destination copy"),
                       .warning = QObject::tr("The library copy is enabled in %1. Keeping the destination copy removes "
                                              "those links and moves the library copy to the quarantine.")};
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
            return QObject::tr("Both copies declare the same version, so there is no telling which one is newer.");
        case VersionOrder::NoOneCanTell:
            return QObject::tr("At least one copy declares no version, so there is no telling which one is newer.");
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
        details.ourLinkWasReplaced ? tr("Keep the library copy and restore the link") : tr("Keep the library copy"),
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
