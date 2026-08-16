#include "view/options/OptionsPage.h"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
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
#include "view/shell/LanguageSwitch.h"
#include "view/theme/ModernistMetrics.h"
#include "viewmodel/SimulatorText.h"

namespace
{
    constexpr int kNavigationWidth = 190;
    constexpr int kUpdatesPane = 2;
    constexpr int kBetweenGroups = 22;
    constexpr int kInsideGroup = 10;
    constexpr int kBodyMarginX = 22;
    constexpr int kBodyMarginY = 18;
    constexpr int kRowPaddingX = 12;
    constexpr int kRowPaddingY = 7;
    constexpr int kRowSpacing = 9;

    constexpr auto kRepository = "https://github.com/brunofgmag/fs-organizer";

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
    FillPanes();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(CreateNavigation());
    layout->addWidget(panes_, 1);

    connect(navigation_, &QListWidget::currentRowChanged, panes_, &QStackedWidget::setCurrentIndex);

    connect(&viewModel_, &OptionsViewModel::Changed, this, &OptionsPage::Reload);

    Reload();
}

void OptionsPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

void OptionsPage::RetranslateUi()
{
    const QStringList names = PaneNames();
    for (int row = 0; row < names.size(); ++row)
    {
        navigation_->item(row)->setText(names.at(row));
    }

    const int shown = panes_->currentIndex();

    while (QWidget* pane = panes_->widget(0))
    {
        panes_->removeWidget(pane);
        delete pane;
    }

    delete linkTypes_;
    delete verifications_;
    delete updateModes_;
    delete languages_;

    FillPanes();
    panes_->setCurrentIndex(shown);

    Reload();
}

void OptionsPage::ShowTheUpdates() const
{
    navigation_->setCurrentRow(kUpdatesPane);
}

QStringList OptionsPage::PaneNames() const
{
    return {tr("Profiles and libraries"), tr("Links"), tr("Updates"), tr("Language"), tr("About")};
}

void OptionsPage::FillPanes()
{
    panes_->addWidget(CreateProfilesAndLibraries());
    panes_->addWidget(CreateLinks());
    panes_->addWidget(CreateUpdates());
    panes_->addWidget(CreateLanguage());
    panes_->addWidget(CreateAbout());
}

QWidget* OptionsPage::CreateNavigation()
{
    navigation_ = new QListWidget(this);
    navigation_->setObjectName(QStringLiteral("SectionRail"));
    navigation_->setFixedWidth(kNavigationWidth);
    navigation_->setFrameShape(QFrame::NoFrame);
    navigation_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    navigation_->addItems(PaneNames());
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
    QWidget* profileGroup = GroupWith(tr("Profiles"), profiles_, underTheProfiles, pane);

    auto* addProfile = new QPushButton(tr("Add profile…"), pane);
    connect(addProfile, &QPushButton::clicked, this, &OptionsPage::AddProfileRequested);
    underTheProfiles->addWidget(ButtonRow(addProfile, profileGroup));
    layout->addWidget(profileGroup);

    QVBoxLayout* underTheDestinations = nullptr;
    layout->addWidget(GroupWith(tr("Destinations"), destinations_, underTheDestinations, pane, &destinationsHeading_));

    QVBoxLayout* underTheLibraries = nullptr;
    QWidget* libraryGroup = GroupWith(tr("Libraries"), libraries_, underTheLibraries, pane, &librariesHeading_);

    addLibrary_ = new QPushButton(tr("Add library…"), pane);
    connect(addLibrary_, &QPushButton::clicked, this, &OptionsPage::AddLibrary);

    importLegacy_ = new QPushButton(tr("Import from MSFS Addons Linker…"), pane);
    connect(importLegacy_, &QPushButton::clicked, this, &OptionsPage::LegacyImportRequested);

    auto* underneath = new QWidget(libraryGroup);
    auto* buttons = new QHBoxLayout(underneath);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(addLibrary_);
    buttons->addWidget(importLegacy_);
    buttons->addStretch();
    underTheLibraries->addWidget(underneath);

    onlyForTheProfileInUse_ = Quiet(tr("This is another profile, and FS Organizer only touches what is in use. Mark it "
                                       "as active to switch a destination or a library."),
                                    libraryGroup);
    underTheLibraries->addWidget(onlyForTheProfileInUse_);
    layout->addWidget(libraryGroup);
    layout->addStretch();

    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(pane);

    return scroll;
}

QWidget* OptionsPage::Choice(const QString& name, QWidget* control, const bool follows)
{
    return Choice(name, {}, control, follows);
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

    if (!explanation.isEmpty())
    {
        said->addWidget(Quiet(explanation, choice));
    }

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
    links->addWidget(Heading(tr("Link type"), pane));

    linkTypes_ = new QButtonGroup(this);

    auto* junction = new QRadioButton(pane);
    junction->setObjectName(QStringLiteral("JunctionChoice"));
    linkTypes_->addButton(junction, static_cast<int>(LinkType::Junction));

    auto* symbolic = new QRadioButton(pane);
    symbolic->setObjectName(QStringLiteral("SymbolicChoice"));
    linkTypes_->addButton(symbolic, static_cast<int>(LinkType::Symbolic));

    links->addWidget(
        Choice(tr("Directory junction"),
               tr("Needs no administrator and crosses local volumes. It is the only path the MVP has tested."),
               junction, false));
    links->addWidget(Choice(tr("Symbolic link"),
                            tr("Only for a library on a network path, where the junction does not reach. Needs "
                               "privilege; without it the app explains the refusal instead of failing quietly."),
                            symbolic, true));
    layout->addLayout(links);

    connect(linkTypes_, &QButtonGroup::idClicked, this,
            [this](const int chosen)
            {
                viewModel_.ChooseTypeOfLink(static_cast<LinkType>(chosen));
                emit StatusChanged(
                    chosen == static_cast<int>(LinkType::Symbolic)
                        ? tr("New links become symbolic links. The ones that already exist stay directory junctions.")
                        : tr("New links become directory junctions."));
            });

    auto* verification = new QVBoxLayout;
    verification->setContentsMargins(0, 0, 0, 0);
    verification->setSpacing(kInsideGroup);
    verification->addWidget(Heading(tr("Check after copying"), pane));

    verifications_ = new QButtonGroup(this);

    auto* structure = new QRadioButton(pane);
    structure->setObjectName(QStringLiteral("StructureChoice"));
    verifications_->addButton(structure, static_cast<int>(Verification::ByStructure));

    auto* hashed = new QRadioButton(pane);
    hashed->setObjectName(QStringLiteral("HashChoice"));
    verifications_->addButton(hashed, static_cast<int>(Verification::ByHash));

    verification->addWidget(Choice(tr("By structure"),
                                   tr("Checks the count and the size of every file. It is what runs today."), structure,
                                   false));
    verification->addWidget(Choice(tr("By hash"),
                                   tr("Reads both sides in full and compares the bytes, which catches a change the "
                                      "size hides. Makes an import several times slower, and the number of files "
                                      "weighs more than their size."),
                                   hashed, true));
    layout->addLayout(verification);

    connect(verifications_, &QButtonGroup::idClicked, this,
            [this](const int chosen)
            {
                viewModel_.ChooseVerification(static_cast<Verification>(chosen));
                emit StatusChanged(chosen == static_cast<int>(Verification::ByHash)
                                       ? tr("Imports now read both sides in full before removing the folder they "
                                            "copied.")
                                       : tr("Imports now check the count and the size of every file."));
            });
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
    modes->addWidget(Heading(tr("Updates"), pane));

    updateModes_ = new QButtonGroup(this);

    const struct
    {
        UpdateMode mode;
        QString name;
        QString explanation;
        const char* objectName;
    } offered[] = {
        {.mode = UpdateMode::Automatic,
         .name = tr("Automatic"),
         .explanation = tr("Downloads the new version on its own and applies it when you close the program."),
         .objectName = "AutomaticUpdateChoice"},
        {.mode = UpdateMode::Notify,
         .name = tr("Notify"),
         .explanation = tr("Looks for a new version and says it exists, but only downloads it if you say so."),
         .objectName = "NotifyUpdateChoice"},
        {.mode = UpdateMode::Manual,
         .name = tr("Manual"),
         .explanation = tr("Only looks when you click Check now."),
         .objectName = "ManualUpdateChoice"},
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

    checkForUpdates_ = new QPushButton(tr("Check now"), pane);
    downloadUpdate_ = new QPushButton(tr("Download"), pane);
    applyUpdate_ = new QPushButton(tr("Apply and restart"), pane);

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

QWidget* OptionsPage::CreateLanguage()
{
    auto* pane = new QWidget(this);

    auto* layout = new QVBoxLayout(pane);
    layout->setContentsMargins(kBodyMarginX, kBodyMarginY, kBodyMarginX, kBodyMarginY);
    layout->setSpacing(kBetweenGroups);

    auto* choices = new QVBoxLayout;
    choices->setContentsMargins(0, 0, 0, 0);
    choices->setSpacing(kInsideGroup);
    choices->addWidget(Heading(tr("Language"), pane));

    languages_ = new QButtonGroup(this);

    bool follows = false;
    int row = 0;
    for (const LanguageSwitch::Offer& offer : LanguageSwitch::Offered())
    {
        auto* button = new QRadioButton(pane);
        button->setObjectName(QLatin1String(offer.objectName));
        languages_->addButton(button, row++);

        choices->addWidget(Choice(QString::fromUtf8(offer.name), button, follows));
        follows = true;
    }

    layout->addLayout(choices);
    layout->addWidget(
        Quiet(tr("Choosing a language writes the language key in settings.json and changes the interface right away."),
              pane));
    layout->addStretch();

    connect(languages_, &QButtonGroup::idClicked, this,
            [this](const int chosen)
            {
                viewModel_.ChooseLanguage(LanguageSwitch::Offered().at(static_cast<std::size_t>(chosen)).code);
            });

    return pane;
}

void OptionsPage::ReloadLanguage() const
{
    const QString inUse = LanguageSwitch::Resolve(QString::fromStdString(viewModel_.Language()));

    int row = 0;
    for (const LanguageSwitch::Offer& offer : LanguageSwitch::Offered())
    {
        if (inUse == QLatin1String(offer.code))
        {
            if (QAbstractButton* chosen = languages_->button(row); chosen != nullptr)
            {
                chosen->setChecked(true);
            }

            return;
        }

        ++row;
    }
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

    layout->addWidget(Quiet(tr("Created by %1.").arg(QStringLiteral("Bruno Magalhães")), pane));

    auto* repository = new QLabel(pane);
    repository->setText(QStringLiteral("<a href=\"%1\">%1</a>").arg(kRepository));
    repository->setTextInteractionFlags(Qt::TextBrowserInteraction);
    repository->setOpenExternalLinks(true);
    layout->addWidget(repository);

    layout->addWidget(Quiet(tr("Distributed under the GNU General Public License version 2."), pane));
    layout->addWidget(
        Quiet(tr("The typeface is Archivo, by Omnibus-Type, under the SIL Open Font License 1.1."), pane));
    layout->addStretch();

    return pane;
}

void OptionsPage::Reload()
{
    const bool inUse = viewModel_.ShowsTheProfileInUse();
    const QString whose = NameOf(viewModel_.ProfileShown().variant);

    destinationsHeading_->setText(tr("Destinations of %1").arg(whose).toUpper());
    librariesHeading_->setText(tr("Libraries of %1").arg(whose).toUpper());
    addLibrary_->setEnabled(inUse);
    importLegacy_->setEnabled(inUse);
    onlyForTheProfileInUse_->setVisible(!inUse);

    ReloadProfiles();
    ReloadDestinations();
    ReloadLibraries();
    ReloadUpdates();
    ReloadLanguage();

    if (QAbstractButton* chosen = linkTypes_->button(static_cast<int>(viewModel_.TypeOfLink())); chosen != nullptr)
    {
        chosen->setChecked(true);
    }

    if (QAbstractButton* chosen = verifications_->button(static_cast<int>(viewModel_.VerificationUsed()));
        chosen != nullptr)
    {
        chosen->setChecked(true);
    }

    emit SummaryChanged(tr("%1 · written on every change").arg(AsText(settingsFile_)));
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

        const QString destinations = tr("%n destination", nullptr, static_cast<int>(profile.destinations));
        const QString libraries = tr("%n library", nullptr, static_cast<int>(profile.libraries));

        const QString counted = profile.active
            ? tr("%1 · %2 · %3")
                  .arg(destinations, libraries,
                       tr("%n addon", nullptr, static_cast<int>(viewModel_.AddonsInTheActiveProfile())))
            : tr("%1 · %2").arg(destinations, libraries);

        layout->addWidget(Detail(counted, row));
        layout->addStretch();

        const std::string id = profile.id;

        auto* edit = new QPushButton(tr("View…"), row);
        edit->setEnabled(id != viewModel_.ProfileShown().id);
        layout->addWidget(edit);

        auto* remove = new QPushButton(tr("Remove"), row);
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

        layout->addWidget(Tag(destination.isDefault ? tr("Default") : tr("Extra"),
                              destination.isDefault ? "filled" : "outlined", row));
        layout->addWidget(new QLabel(AsText(destination.path.filename()), row));
        layout->addWidget(Detail(AsText(destination.path), row), 1);

        auto* repoint = new QPushButton(tr("Switch…"), row);
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

        const QString counted = library.counted
            ? tr("%1 · %2, %3")
                  .arg(AsText(library.path), tr("%n category", nullptr, static_cast<int>(library.categories)),
                       tr("%n addon", nullptr, static_cast<int>(library.addons)))
            : AsText(library.path);

        layout->addWidget(Detail(counted, row), 1);

        auto* open = new QPushButton(tr("Open"), row);
        layout->addWidget(open);

        auto* categories = new QPushButton(tr("Categories"), row);
        categories->setObjectName(QStringLiteral("DeclareCategories"));
        categories->setEnabled(library.counted);
        layout->addWidget(categories);

        auto* unregister = new QPushButton(tr("Unregister"), row);
        unregister->setObjectName(QStringLiteral("UnregisterLibrary"));
        unregister->setEnabled(library.counted);
        layout->addWidget(unregister);

        const std::filesystem::path path = library.path;
        connect(open, &QPushButton::clicked, this,
                [path]
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(AsText(path)));
                });

        const LibraryLine line = library;
        connect(categories, &QPushButton::clicked, this,
                [this, line]
                {
                    DeclareCategories(line);
                });

        connect(unregister, &QPushButton::clicked, this,
                [this, line]
                {
                    Unregister(line);
                });

        libraries_->addWidget(row);
    }
}

void OptionsPage::DeclareCategories(const LibraryLine& library)
{
    const LibraryGrouping grouping = viewModel_.GroupingOf(library.id);

    QMessageBox question(QMessageBox::Question, tr("Categories of %1").arg(library.label),
                         tr("FS Organizer marks the folders you built, so that a category keeps counting as one even "
                            "after it loses its last addon. It never marks what it imported, and it never looks inside "
                            "an addon.\n\nFolders that already carry the marker: %1\nFolders that would receive it "
                            "now: %2")
                             .arg(grouping.alreadyDeclared.size())
                             .arg(grouping.notYetDeclared.size()),
                         QMessageBox::NoButton, this);

    QPushButton* declare = question.addButton(tr("Mark them"), QMessageBox::AcceptRole);
    QPushButton* takeBack = question.addButton(tr("Take every marker back"), QMessageBox::DestructiveRole);
    question.addButton(QMessageBox::Cancel);

    declare->setEnabled(!grouping.notYetDeclared.empty());
    takeBack->setEnabled(!grouping.alreadyDeclared.empty());

    question.exec();

    if (question.clickedButton() == declare)
    {
        viewModel_.DeclareTheCategoriesOf(library.id);
    }
    else if (question.clickedButton() == takeBack)
    {
        viewModel_.TakeBackTheMarkersOf(library.id);
    }
}

void OptionsPage::Repoint(const std::filesystem::path& destination)
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Choose the new folder for this destination"),
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

    QMessageBox question(QMessageBox::Question, tr("Switch the destination"),
                         tr("The profile starts using %1.\n\nThe links that already exist in %2 stay there, working, "
                            "and FS Organizer stops touching them. The destination pinnings that pointed at the old "
                            "folder start pointing at the new one.")
                             .arg(chosen, AsText(destination)),
                         QMessageBox::NoButton, this);

    const QPushButton* proceed = question.addButton(tr("Switch"), QMessageBox::AcceptRole);
    question.addButton(tr("Cancel"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != proceed)
    {
        return;
    }

    viewModel_.RepointDestination(destination, landing);
    Reload();

    emit StatusChanged(tr("Destination switched to %1.").arg(chosen));
}

void OptionsPage::AddLibrary()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Choose the library folder"));
    if (chosen.isEmpty())
    {
        return;
    }

    const LibraryReport report = viewModel_.RegisterLibrary(AsPath(chosen));
    if (!report.Accepted())
    {
        QMessageBox::warning(this, tr("Library refused"),
                             tr("%1 is inside a library that is already registered.").arg(chosen));
        return;
    }

    Reload();

    emit StatusChanged(tr("Library registered: %1, %2")
                           .arg(tr("%n category", nullptr, static_cast<int>(report.categories)),
                                tr("%n addon", nullptr, static_cast<int>(report.addons))));
}

void OptionsPage::Remove(const ProfileLine& profile)
{
    QMessageBox question(QMessageBox::Question, tr("Remove %1?").arg(profile.label), QString{}, QMessageBox::NoButton,
                         this);

    question.setText(tr("The profile leaves the configuration along with its libraries and its destinations. No file "
                        "is deleted or moved."));

    QCheckBox* disabling = nullptr;
    const std::size_t enabled = profile.active ? viewModel_.EnabledInTheProfileInUse() : 0;

    if (enabled > 0)
    {
        question.setInformativeText(
            tr("%n addon of this profile is enabled right now. The links stay in the destination and keep working in "
               "the simulator, but FS Organizer starts treating them as third party links and no longer touches them.",
               nullptr, static_cast<int>(enabled)));

        disabling = new QCheckBox(tr("Disable the %1 before removing").arg(enabled), &question);
        disabling->setChecked(false);
        question.setCheckBox(disabling);
    }

    const QPushButton* proceed = question.addButton(tr("Remove"), QMessageBox::AcceptRole);
    question.addButton(tr("Cancel"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != proceed)
    {
        return;
    }

    if (!viewModel_.RemoveProfile(profile.id, disabling != nullptr && disabling->isChecked()))
    {
        emit StatusChanged(tr("%1 was not removed: the program needs at least one profile.").arg(profile.label));
        return;
    }

    emit StatusChanged(tr("%1 left the configuration.").arg(profile.label));
}

void OptionsPage::Unregister(const LibraryLine& library)
{
    QMessageBox question(QMessageBox::Question, tr("Unregister %1?").arg(library.label), QString{},
                         QMessageBox::NoButton, this);

    question.setText(tr("The library leaves the configuration. No file is deleted or moved."));

    QCheckBox* disabling = nullptr;

    if (library.enabled > 0)
    {
        question.setInformativeText(
            tr("%n addon of it is enabled right now. The links stay in the destination and keep working in the "
               "simulator, but FS Organizer starts treating them as third party links and no longer touches them.",
               nullptr, static_cast<int>(library.enabled)));

        disabling = new QCheckBox(tr("Disable the %1 before unregistering").arg(library.enabled), &question);
        disabling->setChecked(false);
        question.setCheckBox(disabling);
    }

    const QPushButton* proceed = question.addButton(tr("Unregister"), QMessageBox::AcceptRole);
    question.addButton(tr("Cancel"), QMessageBox::RejectRole);
    question.exec();

    if (question.clickedButton() != proceed)
    {
        return;
    }

    viewModel_.UnregisterLibrary(library.id, disabling != nullptr && disabling->isChecked());

    emit StatusChanged(tr("%1 left the configuration.").arg(library.label));
}
