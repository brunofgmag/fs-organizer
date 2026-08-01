#include "view/options/OptionsPage.h"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/SimulatorText.h"

namespace
{
    constexpr int kNavigationWidth = 190;
    constexpr int kBetweenGroups = 22;
    constexpr int kInsideGroup = 10;
    constexpr int kBodyMarginX = 22;
    constexpr int kBodyMarginY = 18;
    constexpr int kRowPaddingX = 12;
    constexpr int kRowPaddingY = 7;
    constexpr int kRowSpacing = 9;

    void SeparateFromWhatCameBefore(QWidget* row, const bool follows)
    {
        row->setProperty("follows", follows);
    }

    QLabel* Heading(const QString& text, QWidget* parent)
    {
        auto* heading = new QLabel(text.toUpper(), parent);
        heading->setObjectName(QStringLiteral("OptionsGroupName"));

        return heading;
    }

    QLabel* Quiet(const QString& text, QWidget* parent)
    {
        auto* label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("PanelPromise"));
        label->setWordWrap(true);

        return label;
    }

    QLabel* Detail(const QString& text, QWidget* parent)
    {
        auto* label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("PanelPromise"));

        return label;
    }

    QLabel* Tag(const QString& text, const char* tone, QWidget* parent)
    {
        auto* tag = new QLabel(text, parent);
        tag->setProperty("tag", tone);

        return tag;
    }

    QFrame* Note(const QString& text, QWidget* parent)
    {
        auto* note = new QFrame(parent);
        note->setObjectName(QStringLiteral("OptionsNote"));

        auto* layout = new QVBoxLayout(note);
        layout->setContentsMargins(14, 12, 14, 12);
        layout->addWidget(Quiet(text, note));

        return note;
    }

    QFrame* Box(QWidget* parent)
    {
        auto* box = new QFrame(parent);
        box->setObjectName(QStringLiteral("OptionsBox"));

        return box;
    }

    QWidget* Row(QWidget* parent)
    {
        auto* row = new QWidget(parent);
        row->setObjectName(QStringLiteral("OptionsRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);

        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(kRowPaddingX, kRowPaddingY, kRowPaddingX, kRowPaddingY);
        layout->setSpacing(kRowSpacing);

        return row;
    }

    void ClearInto(QVBoxLayout* layout)
    {
        while (const QLayoutItem* item = layout->takeAt(0))
        {
            delete item->widget();
            delete item;
        }
    }

    QWidget* GroupWith(const QString& heading,
                       QVBoxLayout*& rows,
                       QVBoxLayout*& below,
                       QWidget* parent,
                       QLabel** headingLabel = nullptr)
    {
        auto* group = new QWidget(parent);

        auto* layout = new QVBoxLayout(group);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(kInsideGroup);

        QLabel* name = Heading(heading, group);
        if (headingLabel != nullptr)
        {
            *headingLabel = name;
        }

        layout->addWidget(name);

        QFrame* box = Box(group);
        rows = new QVBoxLayout(box);
        rows->setContentsMargins(0, 0, 0, 0);
        rows->setSpacing(0);

        layout->addWidget(box);
        below = layout;

        return group;
    }

    QWidget* ButtonRow(QPushButton* button, QWidget* parent)
    {
        auto* holder = new QWidget(parent);

        auto* layout = new QHBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(button);
        layout->addStretch();

        return holder;
    }
}

OptionsPage::OptionsPage(OptionsViewModel& viewModel,
                         UpdateViewModel& updates,
                         std::filesystem::path settingsFile,
                         QWidget* parent)
    : QWidget(parent), viewModel_(viewModel), updates_(updates), settingsFile_(std::move(settingsFile))
{
    panes_ = new QStackedWidget(this);
    panes_->addWidget(CreateProfilesAndLibraries());
    panes_->addWidget(CreateLinks());
    panes_->addWidget(CreateUpdates());
    panes_->addWidget(
        CreateWaitingOn(tr("Idioma"),
                        tr("A chave language já existe no settings.json, e o seletor aparece quando a interface tiver "
                           "os dois idiomas. Hoje ela nasce em português do Brasil: as 609 frases ainda não foram "
                           "invertidas para inglês, então não há o que escolher.")));
    panes_->addWidget(CreateAbout());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateNavigation());
    layout->addWidget(panes_, 1);

    connect(navigation_, &QListWidget::currentRowChanged, panes_, &QStackedWidget::setCurrentIndex);

    connect(&viewModel_, &OptionsViewModel::Changed, this, &OptionsPage::Reload);

    Reload();
}

QWidget* OptionsPage::CreateNavigation()
{
    navigation_ = new QListWidget(this);
    navigation_->setObjectName(QStringLiteral("OptionsNav"));
    navigation_->setFixedWidth(kNavigationWidth);
    navigation_->setFrameShape(QFrame::NoFrame);
    navigation_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    navigation_->addItem(tr("Perfis e bibliotecas"));
    navigation_->addItem(tr("Links"));
    navigation_->addItem(tr("Atualizações"));
    navigation_->addItem(tr("Idioma"));
    navigation_->addItem(tr("Sobre"));

    navigation_->item(3)->setToolTip(tr("Espera a interface ganhar os dois idiomas."));

    navigation_->setCurrentRow(0);

    return navigation_;
}

QWidget* OptionsPage::CreateProfilesAndLibraries()
{
    auto* pane = new QWidget(this);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kBodyMarginX, kBodyMarginY, kBodyMarginX, kBodyMarginY);
    layout->setSpacing(kBetweenGroups);

    QVBoxLayout* underTheProfiles = nullptr;
    QWidget* profileGroup = GroupWith(tr("Perfis"), profiles_, underTheProfiles, pane);

    auto* addProfile = new QPushButton(tr("Adicionar perfil…"), pane);
    connect(addProfile, &QPushButton::clicked, this, &OptionsPage::AddProfileRequested);
    underTheProfiles->addWidget(ButtonRow(addProfile, profileGroup));
    layout->addWidget(profileGroup);

    QVBoxLayout* underTheDestinations = nullptr;
    layout->addWidget(GroupWith(tr("Destinos"), destinations_, underTheDestinations, pane, &destinationsHeading_));

    QVBoxLayout* underTheLibraries = nullptr;
    QWidget* libraryGroup = GroupWith(tr("Bibliotecas"), libraries_, underTheLibraries, pane, &librariesHeading_);

    addLibrary_ = new QPushButton(tr("Adicionar biblioteca…"), pane);
    connect(addLibrary_, &QPushButton::clicked, this, &OptionsPage::AddLibrary);

    importLegacy_ = new QPushButton(tr("Importar do MSFS Addons Linker…"), pane);
    connect(importLegacy_, &QPushButton::clicked, this, &OptionsPage::LegacyImportRequested);

    auto* underneath = new QWidget(libraryGroup);
    auto* buttons = new QHBoxLayout(underneath);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(addLibrary_);
    buttons->addWidget(importLegacy_);
    buttons->addStretch();
    underTheLibraries->addWidget(underneath);

    onlyForTheProfileInUse_ = Quiet(tr("Este é outro perfil, e o FS Organizer só mexe no que está em uso. Marque-o "
                                       "como ativo para trocar destino ou biblioteca."),
                                    libraryGroup);
    underTheLibraries->addWidget(onlyForTheProfileInUse_);
    underTheLibraries->addWidget(
        Quiet(tr("Descadastrar remove a biblioteca da configuração e não apaga arquivo nenhum. Os links que apontavam "
                 "para ela continuam funcionando no simulador, mas passam a aparecer como links de terceiros, que o FS "
                 "Organizer não toca."),
              libraryGroup));
    layout->addWidget(libraryGroup);
    layout->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(pane);

    return scroll;
}

QWidget* OptionsPage::Choice(const QString& name, const QString& explanation, QWidget* control, const bool follows)
{
    auto* choice = new QWidget(this);
    choice->setObjectName(QStringLiteral("OptionsChoice"));
    choice->setAttribute(Qt::WA_StyledBackground, true);
    SeparateFromWhatCameBefore(choice, follows);

    auto* said = new QVBoxLayout;
    said->setContentsMargins(0, 0, 0, 0);
    said->setSpacing(4);

    auto* title = new QLabel(name, choice);
    title->setObjectName(QStringLiteral("OptionsChoiceName"));
    said->addWidget(title);
    said->addWidget(Quiet(explanation, choice));

    auto* layout = new QHBoxLayout(choice);
    layout->setContentsMargins(0, 11, 0, 11);
    layout->setSpacing(18);
    layout->addLayout(said, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);

    return choice;
}

QWidget* OptionsPage::CreateLinks()
{
    auto* pane = new QWidget(this);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kBodyMarginX, kBodyMarginY, kBodyMarginX, kBodyMarginY);
    layout->setSpacing(kBetweenGroups);

    auto* links = new QVBoxLayout;
    links->setContentsMargins(0, 0, 0, 0);
    links->setSpacing(kInsideGroup);
    links->addWidget(Heading(tr("Tipo de link"), pane));

    linkTypes_ = new QButtonGroup(this);

    auto* junction = new QRadioButton(pane);
    junction->setObjectName(QStringLiteral("JunctionChoice"));
    linkTypes_->addButton(junction, static_cast<int>(LinkType::Junction));

    auto* symbolic = new QRadioButton(pane);
    symbolic->setObjectName(QStringLiteral("SymbolicChoice"));
    linkTypes_->addButton(symbolic, static_cast<int>(LinkType::Symbolic));

    links->addWidget(Choice(tr("Junção de diretório"),
                            tr("Não exige administrador e cruza volumes locais. É o único caminho testado do MVP."),
                            junction, false));
    links->addWidget(Choice(tr("Link simbólico"),
                            tr("Só para biblioteca em caminho de rede, onde a junção não chega. Exige privilégio; sem "
                               "ele o app explica a recusa em vez de falhar calado."),
                            symbolic, true));
    layout->addLayout(links);

    connect(linkTypes_, &QButtonGroup::idClicked, this,
            [this](const int chosen)
            {
                viewModel_.ChooseTypeOfLink(static_cast<LinkType>(chosen));
                emit StatusChanged(
                    chosen == static_cast<int>(LinkType::Symbolic)
                        ? tr("Os links novos passam a ser link simbólico. Os que já existem continuam junção de "
                             "diretório.")
                        : tr("Os links novos passam a ser junção de diretório."));
            });

    auto* verification = new QVBoxLayout;
    verification->setContentsMargins(0, 0, 0, 0);
    verification->setSpacing(kInsideGroup);
    verification->addWidget(Heading(tr("Verificação depois de copiar"), pane));

    auto* structure = new QRadioButton(pane);
    structure->setChecked(true);
    structure->setEnabled(false);
    verification->addWidget(Choice(
        tr("Por estrutura"), tr("Confere contagem e tamanho de cada arquivo. É o que roda hoje."), structure, false));

    auto* hashed = new QRadioButton(pane);
    hashed->setObjectName(QStringLiteral("HashChoice"));
    hashed->setEnabled(false);

    auto* waiting = new QWidget(pane);
    auto* waitingLayout = new QHBoxLayout(waiting);
    waitingLayout->setContentsMargins(0, 0, 0, 0);
    waitingLayout->setSpacing(8);
    waitingLayout->addWidget(Tag(tr("Fase 2"), "muted", waiting));
    waitingLayout->addWidget(hashed);

    verification->addWidget(Choice(tr("Por hash"),
                                   tr("Lê os dois lados inteiros e compara. Correto e caro: uma importação de 400 MB "
                                      "passa a ler 800 MB."),
                                   waiting, true));
    layout->addLayout(verification);

    layout->addWidget(Note(tr("Aparência não tem opção aqui, de propósito. O app segue o tema claro ou escuro do "
                              "Windows ao vivo e não guarda chave de tema. Trocar o tema do sistema com o app aberto "
                              "troca a paleta e a barra de título na hora."),
                           pane));
    layout->addStretch();

    return pane;
}

QWidget* OptionsPage::CreateUpdates()
{
    auto* pane = new QWidget(this);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kBodyMarginX, kBodyMarginY, kBodyMarginX, kBodyMarginY);
    layout->setSpacing(kBetweenGroups);

    auto* modes = new QVBoxLayout;
    modes->setContentsMargins(0, 0, 0, 0);
    modes->setSpacing(kInsideGroup);
    modes->addWidget(Heading(tr("Atualizações"), pane));

    updateModes_ = new QButtonGroup(this);

    const struct
    {
        UpdateMode mode;
        QString name;
        QString explanation;
        const char* objectName;
    } offered[] = {
        {UpdateMode::Automatic, tr("Automática"),
         tr("Baixa a versão nova sozinha e a aplica quando você fecha o programa."), "AutomaticUpdateChoice"},
        {UpdateMode::Notify, tr("Avisar"),
         tr("Procura versão nova e diz que ela existe. Nada é baixado antes de você mandar."), "NotifyUpdateChoice"},
        {UpdateMode::Manual, tr("Manual"), tr("Só procura quando você clica em Procurar agora."), "ManualUpdateChoice"},
    };

    bool follows = false;
    for (const auto& choice : offered)
    {
        auto* button = new QRadioButton(pane);
        button->setObjectName(QLatin1String(choice.objectName));
        button->setEnabled(updates_.UpdatesAreOn());
        updateModes_->addButton(button, static_cast<int>(choice.mode));

        modes->addWidget(Choice(choice.name, choice.explanation, button, follows));
        follows = true;
    }

    layout->addLayout(modes);

    connect(updateModes_, &QButtonGroup::idClicked, this,
            [this](const int chosen)
            {
                updates_.ChooseMode(static_cast<UpdateMode>(chosen));
            });

    connect(&updates_, &UpdateViewModel::ModeChosen, this,
            [this](const UpdateMode chosen)
            {
                viewModel_.ChooseUpdateMode(chosen);
            });

    updateStatus_ = Quiet(QString{}, pane);
    updateStatus_->setObjectName(QStringLiteral("UpdateStatus"));
    layout->addWidget(updateStatus_);

    checkForUpdates_ = new QPushButton(tr("Procurar agora"), pane);
    downloadUpdate_ = new QPushButton(tr("Baixar"), pane);
    applyUpdate_ = new QPushButton(tr("Aplicar e reiniciar"), pane);

    auto* buttons = new QWidget(pane);
    auto* row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(checkForUpdates_);
    row->addWidget(downloadUpdate_);
    row->addWidget(applyUpdate_);
    row->addStretch();
    layout->addWidget(buttons);

    connect(checkForUpdates_, &QPushButton::clicked, &updates_, &UpdateViewModel::Check);
    connect(downloadUpdate_, &QPushButton::clicked, &updates_, &UpdateViewModel::Download);
    connect(applyUpdate_, &QPushButton::clicked, &updates_, &UpdateViewModel::ApplyAndRestart);
    connect(&updates_, &UpdateViewModel::Changed, this, &OptionsPage::ReloadUpdates);

    layout->addStretch();

    return pane;
}

void OptionsPage::ReloadUpdates() const
{
    if (QAbstractButton* chosen = updateModes_->button(static_cast<int>(updates_.Mode())); chosen != nullptr)
    {
        chosen->setChecked(true);
    }

    updateStatus_->setText(updates_.WhatIsGoingOn());
    checkForUpdates_->setEnabled(updates_.CanCheck());
    downloadUpdate_->setEnabled(updates_.CanDownload());
    applyUpdate_->setEnabled(updates_.State() == UpdateState::ReadyToApply);
}

QWidget* OptionsPage::CreateWaitingOn(const QString& heading, const QString& explanation)
{
    auto* pane = new QWidget(this);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kBodyMarginX, kBodyMarginY, kBodyMarginX, kBodyMarginY);
    layout->setSpacing(kBetweenGroups);
    layout->addWidget(Heading(heading, pane));
    layout->addWidget(Note(explanation, pane));
    layout->addStretch();

    return pane;
}

QWidget* OptionsPage::CreateAbout()
{
    auto* pane = new QWidget(this);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kBodyMarginX, kBodyMarginY, kBodyMarginX, kBodyMarginY);
    layout->setSpacing(kBetweenGroups);

    layout->addWidget(Heading(tr("FS Organizer"), pane));

    auto* version = new QLabel(QCoreApplication::applicationVersion(), pane);
    version->setObjectName(QStringLiteral("AboutVersion"));
    layout->addWidget(version);

    layout->addWidget(Quiet(tr("Distribuído sob a GNU General Public License versão 2."), pane));
    layout->addWidget(Quiet(tr("A tipografia é a Archivo, de Omnibus-Type, sob a SIL Open Font License 1.1."), pane));
    layout->addStretch();

    return pane;
}

void OptionsPage::Reload()
{
    const bool inUse = viewModel_.ShowsTheProfileInUse();
    const QString whose = NameOf(viewModel_.ProfileShown().variant);

    destinationsHeading_->setText(tr("Destinos de %1").arg(whose).toUpper());
    librariesHeading_->setText(tr("Bibliotecas de %1").arg(whose).toUpper());
    addLibrary_->setEnabled(inUse);
    importLegacy_->setEnabled(inUse);
    onlyForTheProfileInUse_->setVisible(!inUse);

    ReloadProfiles();
    ReloadDestinations();
    ReloadLibraries();
    ReloadUpdates();

    if (QAbstractButton* chosen = linkTypes_->button(static_cast<int>(viewModel_.TypeOfLink())); chosen != nullptr)
    {
        chosen->setChecked(true);
    }

    emit SummaryChanged(tr("%1 · gravado a cada mudança").arg(AsText(settingsFile_)));
}

void OptionsPage::ReloadProfiles()
{
    ClearInto(profiles_);

    delete profileChoices_;
    profileChoices_ = new QButtonGroup(this);

    for (const ProfileLine& profile : viewModel_.Profiles())
    {
        QWidget* row = Row(profiles_->parentWidget());
        SeparateFromWhatCameBefore(row, profiles_->count() > 0);
        auto* layout = qobject_cast<QHBoxLayout*>(row->layout());

        auto* chosen = new QRadioButton(profile.label, row);
        chosen->setChecked(profile.active);
        profileChoices_->addButton(chosen);
        layout->addWidget(chosen);

        const QString counted = profile.active
            ? tr("%1 destino(s) · %2 biblioteca(s) · %3 addons")
                  .arg(profile.destinations)
                  .arg(profile.libraries)
                  .arg(viewModel_.AddonsInTheActiveProfile())
            : tr("%1 destino(s) · %2 biblioteca(s)").arg(profile.destinations).arg(profile.libraries);

        layout->addWidget(Detail(counted, row));
        layout->addStretch();

        const std::string id = profile.id;

        auto* edit = new QPushButton(tr("Ver…"), row);
        edit->setEnabled(id != viewModel_.ProfileShown().id);
        layout->addWidget(edit);

        auto* remove = new QPushButton(tr("Remover"), row);
        remove->setEnabled(viewModel_.Profiles().size() > 1);
        layout->addWidget(remove);

        connect(chosen, &QRadioButton::clicked, this,
                [this, id]
                {
                    emit ProfileChosen(id);
                });

        connect(edit, &QPushButton::clicked, this,
                [this, id]
                {
                    viewModel_.ShowProfile(id);
                });

        const ProfileLine line = profile;
        connect(remove, &QPushButton::clicked, this,
                [this, line]
                {
                    Remove(line);
                });

        profiles_->addWidget(row);
    }
}

void OptionsPage::ReloadDestinations()
{
    ClearInto(destinations_);

    for (const DestinationLine& destination : viewModel_.Destinations())
    {
        QWidget* row = Row(destinations_->parentWidget());
        SeparateFromWhatCameBefore(row, destinations_->count() > 0);
        auto* layout = qobject_cast<QHBoxLayout*>(row->layout());

        layout->addWidget(Tag(destination.isDefault ? tr("Padrão") : tr("Extra"),
                              destination.isDefault ? "filled" : "outlined", row));
        layout->addWidget(new QLabel(AsText(destination.path.filename()), row));
        layout->addWidget(Detail(AsText(destination.path), row), 1);

        auto* repoint = new QPushButton(tr("Trocar…"), row);
        repoint->setEnabled(viewModel_.ShowsTheProfileInUse());
        layout->addWidget(repoint);

        const std::filesystem::path path = destination.path;
        connect(repoint, &QPushButton::clicked, this,
                [this, path]
                {
                    Repoint(path);
                });

        destinations_->addWidget(row);
    }
}

void OptionsPage::ReloadLibraries()
{
    ClearInto(libraries_);

    for (const LibraryLine& library : viewModel_.Libraries())
    {
        QWidget* row = Row(libraries_->parentWidget());
        SeparateFromWhatCameBefore(row, libraries_->count() > 0);
        auto* layout = qobject_cast<QHBoxLayout*>(row->layout());

        auto* name = new QLabel(library.label, row);
        QFont bold = name->font();
        bold.setWeight(QFont::DemiBold);
        name->setFont(bold);
        layout->addWidget(name);

        const QString counted = library.counted ? tr("%1 · %2 categoria(s), %3 addons")
                                                      .arg(AsText(library.path))
                                                      .arg(library.categories)
                                                      .arg(library.addons)
                                                : AsText(library.path);

        layout->addWidget(Detail(counted, row), 1);

        auto* open = new QPushButton(tr("Abrir"), row);
        layout->addWidget(open);

        auto* unregister = new QPushButton(tr("Descadastrar"), row);
        unregister->setEnabled(library.counted);
        layout->addWidget(unregister);

        const std::filesystem::path path = library.path;
        connect(open, &QPushButton::clicked, this,
                [path]
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(path)));
                });

        const LibraryLine line = library;
        connect(unregister, &QPushButton::clicked, this,
                [this, line]
                {
                    Unregister(line);
                });

        libraries_->addWidget(row);
    }
}

void OptionsPage::Repoint(const std::filesystem::path& destination)
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Escolha a pasta nova deste destino"),
                                                             AsText(destination.parent_path()));
    if (chosen.isEmpty())
    {
        return;
    }

    const std::filesystem::path landing = AsPath(chosen);
    if (landing == destination)
    {
        return;
    }

    QMessageBox question(QMessageBox::Question, tr("Trocar o destino"),
                         tr("O perfil passa a usar %1.\n\nOs links que já existem em %2 continuam lá, funcionando, e o "
                            "FS Organizer deixa de mexer neles. As fixações de destino que apontavam para a pasta "
                            "antiga passam a apontar para a nova.")
                             .arg(chosen, AsText(destination)),
                         QMessageBox::NoButton, this);

    const QPushButton* proceed = question.addButton(tr("Trocar"), QMessageBox::AcceptRole);
    question.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != proceed)
    {
        return;
    }

    viewModel_.RepointDestination(destination, landing);
    Reload();

    emit StatusChanged(tr("Destino trocado para %1.").arg(chosen));
}

void OptionsPage::AddLibrary()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Escolha a pasta da biblioteca"));
    if (chosen.isEmpty())
    {
        return;
    }

    const LibraryReport report = viewModel_.RegisterLibrary(AsPath(chosen));
    if (!report.Accepted())
    {
        QMessageBox::warning(this, tr("Biblioteca recusada"),
                             tr("%1 está dentro de uma biblioteca que já é cadastrada.").arg(chosen));
        return;
    }

    Reload();

    emit StatusChanged(
        tr("Biblioteca cadastrada: %1 categoria(s), %2 addons").arg(report.categories).arg(report.addons));
}

void OptionsPage::Remove(const ProfileLine& profile)
{
    QMessageBox question(QMessageBox::Question, tr("Remover %1?").arg(profile.label), QString{}, QMessageBox::NoButton,
                         this);

    question.setText(tr("O perfil sai da configuração com as bibliotecas e os destinos dele. Nenhum arquivo é apagado "
                        "ou movido."));

    QCheckBox* disabling = nullptr;
    const std::size_t enabled = profile.active ? viewModel_.EnabledInTheProfileInUse() : 0;

    if (enabled > 0)
    {
        question.setInformativeText(tr("%1 addon(s) deste perfil estão habilitados agora. Os links continuam no "
                                       "destino e continuam funcionando no simulador, mas o FS Organizer passa a "
                                       "tratá-los como links de terceiros e não mexe mais neles.")
                                        .arg(enabled));

        disabling = new QCheckBox(tr("Desabilitar os %1 antes de remover").arg(enabled), &question);
        disabling->setChecked(false);
        question.setCheckBox(disabling);
    }

    const QPushButton* proceed = question.addButton(tr("Remover"), QMessageBox::AcceptRole);
    question.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != proceed)
    {
        return;
    }

    if (!viewModel_.RemoveProfile(profile.id, disabling != nullptr && disabling->isChecked()))
    {
        emit StatusChanged(tr("%1 não foi removido: o programa precisa de pelo menos um perfil.").arg(profile.label));
        return;
    }

    emit StatusChanged(tr("%1 saiu da configuração.").arg(profile.label));
}

void OptionsPage::Unregister(const LibraryLine& library)
{
    QMessageBox question(QMessageBox::Question, tr("Descadastrar %1?").arg(library.label), QString{},
                         QMessageBox::NoButton, this);

    question.setText(tr("A biblioteca sai da configuração. Nenhum arquivo é apagado ou movido."));

    QCheckBox* disabling = nullptr;

    if (library.enabled > 0)
    {
        question.setInformativeText(tr("%1 addon(s) dela estão habilitados agora. Os links continuam no destino e "
                                       "continuam funcionando no simulador, mas o FS Organizer passa a tratá-los como "
                                       "links de terceiros e não mexe mais neles.")
                                        .arg(library.enabled));

        disabling = new QCheckBox(tr("Desabilitar os %1 antes de descadastrar").arg(library.enabled), &question);
        disabling->setChecked(false);
        question.setCheckBox(disabling);
    }

    const QPushButton* proceed = question.addButton(tr("Descadastrar"), QMessageBox::AcceptRole);
    question.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != proceed)
    {
        return;
    }

    viewModel_.UnregisterLibrary(library.id, disabling != nullptr && disabling->isChecked());

    emit StatusChanged(tr("%1 saiu da configuração.").arg(library.label));
}
