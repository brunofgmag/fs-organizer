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

#include "support/PathText.h"
#include "support/SizeText.h"
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

    QString WarningAbout(const std::vector<std::filesystem::path>& links)
    {
        QStringList destinations;
        for (const std::filesystem::path& link : links)
        {
            destinations.append(AsText(link.parent_path().filename()));
        }

        return QObject::tr("The library copy is enabled in %1. Keeping the destination one removes those links before "
                           "sending it to the quarantine.")
            .arg(destinations.join(QStringLiteral(", ")));
    }

    QString Version(const Manifest& manifest)
    {
        return manifest.packageVersion.empty() ? QObject::tr("(no version in the manifest)")
                                               : QString::fromStdString(manifest.packageVersion);
    }
}

ConflictDialog::ConflictDialog(const ConflictDetails& details, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Two copies of the same addon"));

    auto* explanation =
        new QLabel(tr("There is a real folder in the destination and an addon with the same name in the library. "
                      "Choose which one stays: the other goes to the quarantine, and nothing is deleted."),
                   this);
    explanation->setWordWrap(true);

    auto* sides = new QHBoxLayout;
    sides->addWidget(CreateSide(tr("Copy in the destination"), details.destination));
    sides->addWidget(CreateSide(tr("Copy in the library"), details.library));

    auto* warning = new QLabel(WarningAbout(details.linksToTheLibraryCopy), this);
    warning->setWordWrap(true);
    warning->setVisible(!details.linksToTheLibraryCopy.empty());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);

    const QPushButton* keepDestination =
        buttons->addButton(tr("Keep the destination one"), QDialogButtonBox::AcceptRole);
    QPushButton* keepLibrary = buttons->addButton(tr("Keep the library one"), QDialogButtonBox::AcceptRole);
    keepLibrary->setDefault(true);

    connect(keepLibrary, &QPushButton::clicked, this,
            [this]
            {
                choice_ = ConflictChoice::KeepTheLibraryCopy;
                accept();
            });
    connect(keepDestination, &QPushButton::clicked, this,
            [this]
            {
                choice_ = ConflictChoice::KeepTheDestinationCopy;
                accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(explanation);
    layout->addLayout(sides, 1);
    layout->addWidget(warning);
    layout->addWidget(buttons);

    resize(760, 320);
}

QGroupBox* ConflictDialog::CreateSide(const QString& title, const ConflictSide& side)
{
    auto* group = new QGroupBox(title, this);

    auto* path = new QLabel(AsText(side.path), group);
    path->setWordWrap(true);
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);

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
