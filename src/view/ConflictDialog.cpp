#include "view/ConflictDialog.h"

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

namespace
{
    QString Moment(const std::optional<std::chrono::system_clock::time_point>& when)
    {
        if (!when.has_value())
        {
            return QObject::tr("(desconhecida)");
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

        return QObject::tr("A cópia da biblioteca está habilitada em %1. "
                    "Ficar com a do destino remove esse(s) link(s) antes de mandá-la para a quarentena.")
               .arg(destinations.join(QStringLiteral(", ")));
    }

    QString Version(const Manifest& manifest)
    {
        return manifest.packageVersion.empty()
                   ? QObject::tr("(sem versão no manifest)")
                   : QString::fromStdString(manifest.packageVersion);
    }
}

ConflictDialog::ConflictDialog(const ConflictDetails& details, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Duas cópias do mesmo addon"));

    auto* explanation = new QLabel(
        tr("Existe uma pasta de verdade no destino e um addon de mesmo nome na biblioteca. "
            "Escolha qual fica: a outra vai para a quarentena, e nada é apagado."),
        this);
    explanation->setWordWrap(true);

    auto* sides = new QHBoxLayout;
    sides->addWidget(CreateSide(tr("Cópia no destino"), details.destination));
    sides->addWidget(CreateSide(tr("Cópia na biblioteca"), details.library));

    auto* warning = new QLabel(WarningAbout(details.linksToTheLibraryCopy), this);
    warning->setWordWrap(true);
    warning->setVisible(!details.linksToTheLibraryCopy.empty());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);

    QPushButton* keepDestination = buttons->addButton(tr("Ficar com a do destino"),
                                                      QDialogButtonBox::AcceptRole);
    QPushButton* keepLibrary = buttons->addButton(tr("Ficar com a da biblioteca"),
                                                 QDialogButtonBox::AcceptRole);
    keepLibrary->setDefault(true);

    connect(keepLibrary, &QPushButton::clicked, this, [this]
    {
        choice_ = ConflictChoice::KeepTheLibraryCopy;
        accept();
    });
    connect(keepDestination, &QPushButton::clicked, this, [this]
    {
        choice_ = ConflictChoice::KeepTheDestinationCopy;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
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
    form->addRow(tr("Caminho:"), path);
    form->addRow(tr("Versão:"), new QLabel(Version(side.manifest), group));
    form->addRow(tr("Tamanho:"), new QLabel(AsSize(side.sizeBytes), group));
    form->addRow(tr("Modificada em:"), new QLabel(Moment(side.modified), group));

    if (!side.manifest.title.empty())
    {
        form->addRow(tr("Título:"), new QLabel(QString::fromStdString(side.manifest.title), group));
    }

    return group;
}

ConflictChoice ConflictDialog::Choice() const
{
    return choice_;
}
