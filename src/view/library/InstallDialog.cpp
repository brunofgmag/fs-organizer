#include "view/library/InstallDialog.h"

#include <algorithm>
#include <utility>

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyleOptionComboBox>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include "support/SizeText.h"
#include "view/WheelGuard.h"
#include "view/delegates/RowDelegate.h"
#include "view/panels/ScrollBarCap.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/RowTagRoles.h"

namespace
{
    constexpr int kWide = 800;
    constexpr int kMostRowsBeforeItScrolls = 12;
    constexpr int kAroundACell = 22;
    constexpr int kIndentation = 20;

    class ListWithoutBranches final : public QTreeView
    {
    public:
        using QTreeView::QTreeView;

    protected:
        void drawBranches(QPainter*, const QRect&, const QModelIndex&) const override
        {
        }
    };

    [[nodiscard]] QStandardItem* ACell(const QString& shown)
    {
        auto* cell = new QStandardItem(shown);
        cell->setFlags(Qt::ItemIsEnabled);

        return cell;
    }

    [[nodiscard]] int DepthOf(const QStandardItem& item)
    {
        int depth = 0;

        for (const QStandardItem* above = item.parent(); above != nullptr; above = above->parent())
        {
            ++depth;
        }

        return depth;
    }

    [[nodiscard]] bool Replaces(const InstallStandingInALibrary& stands)
    {
        return stands.standing == InstallStanding::Replaces
            || stands.standing == InstallStanding::ReplacesAndTheQuarantineHoldsOne;
    }

    [[nodiscard]] bool AllTheSame(const QStringList& names)
    {
        return std::all_of(names.begin(), names.end(),
                           [&names](const QString& name)
                           {
                               return name == names.front();
                           });
    }

    [[nodiscard]] bool AsksForTheDiscard(const QStandardItem& addon)
    {
        return addon.rowCount() > 0 && addon.child(0)->isCheckable();
    }

    void LetTheRowBeChosen(QStandardItem& addon, const bool chosen)
    {
        QStandardItem* archive = addon.parent();

        for (int column = 0; column < archive->columnCount(); ++column)
        {
            if (QStandardItem* cell = archive->child(addon.row(), column); cell != nullptr)
            {
                cell->setEnabled(chosen);
            }
        }

        addon.setCheckState(chosen ? Qt::Checked : Qt::Unchecked);
    }

    [[nodiscard]] const ChosenAddon* KeptAt(const std::vector<ChosenAddon>& kept, const int archive, const int addon)
    {
        const auto found = std::find_if(kept.begin(), kept.end(),
                                        [archive, addon](const ChosenAddon& chosen)
                                        {
                                            return chosen.archive == archive && chosen.addon == addon;
                                        });

        return found == kept.end() ? nullptr : &*found;
    }

    [[nodiscard]] int AddonsOf(const InstallOffer& offer, const int archive)
    {
        return static_cast<int>(offer.archives[static_cast<std::size_t>(archive)].addons.size());
    }
}

void CategoryField::SayInstead(const QString& text)
{
    if (said_ == text)
    {
        return;
    }

    said_ = text;
    update();
}

QString CategoryField::Shown() const
{
    if (!said_.isEmpty())
    {
        return said_;
    }

    return currentIndex() < 0 ? placeholderText() : currentText();
}

void CategoryField::paintEvent(QPaintEvent*)
{
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));

    QStyleOptionComboBox option;
    initStyleOption(&option);
    painter.drawComplexControl(QStyle::CC_ComboBox, option);

    if (!said_.isEmpty())
    {
        option.currentText = said_;
    }
    else if (currentIndex() < 0 && !placeholderText().isEmpty())
    {
        option.palette.setBrush(QPalette::ButtonText, option.palette.placeholderText());
        option.currentText = placeholderText();
    }

    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

InstallDialog::InstallDialog(InstallOffer offer, QWidget* parent) : QDialog(parent), offer_(std::move(offer))
{
    setWindowTitle(tr("Install into the library"));

    model_ = new QStandardItemModel(this);

    tree_ = new ListWithoutBranches(this);
    tree_->setObjectName(QStringLiteral("InstallList"));
    tree_->setItemDelegate(new RowDelegate(tree_));
    tree_->setSelectionMode(QAbstractItemView::NoSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setRootIsDecorated(false);
    tree_->setItemsExpandable(false);
    tree_->setIndentation(kIndentation);
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    DressTheHeaderOf(tree_->header());
    tree_->header()->setSectionsMovable(false);
    tree_->header()->setSectionsClickable(false);
    tree_->header()->setStretchLastSection(false);
    tree_->setModel(model_);

    all_ = new QCheckBox(tr("Check all"), this);
    all_->setObjectName(QStringLiteral("CheckAll"));
    all_->setTristate(true);

    total_ = new QLabel(this);
    total_->setObjectName(QStringLiteral("InstallTotal"));

    library_ = new QComboBox(this);
    library_->setObjectName(QStringLiteral("InstallLibrary"));

    category_ = new CategoryField(this);
    category_->setObjectName(QStringLiteral("InstallCategory"));
    category_->setPlaceholderText(tr("Choose a category"));

    LetTheWheelScrollPastUnlessTheWidgetHasFocus(library_);
    LetTheWheelScrollPastUnlessTheWidgetHasFocus(category_);

    hint_ = new QLabel(tr("Choose the category the new addons go to."), this);
    hint_->setObjectName(QStringLiteral("InstallHint"));

    QSizePolicy kept = hint_->sizePolicy();
    kept.setRetainSizeWhenHidden(true);
    hint_->setSizePolicy(kept);

    auto* form = new QFormLayout;
    form->addRow(tr("Library:"), library_);
    form->addRow(tr("Category:"), category_);
    form->addRow(QString(), hint_);

    promise_ = new QLabel(this);
    promise_->setObjectName(QStringLiteral("PanelPromise"));
    promise_->setWordWrap(true);

    enable_ = new QCheckBox(tr("Enable after installing"), this);
    enable_->setObjectName(QStringLiteral("EnableAfterInstalling"));

    blocked_ = new QWidget(this);
    blockedBy_ = new QLabel(blocked_);
    blockedBy_->setObjectName(QStringLiteral("InstallNotice"));
    blockedBy_->setWordWrap(true);

    auto* again = new QPushButton(tr("Check again"), blocked_);
    again->setObjectName(QStringLiteral("CheckAgain"));
    again->setAutoDefault(false);

    auto* gate = new QHBoxLayout(blocked_);
    gate->setContentsMargins(0, 0, 0, 0);
    gate->setSpacing(kToolbarGap);
    gate->addWidget(blockedBy_, 1);
    gate->addWidget(again);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    install_ = buttons->addButton(tr("Install"), QDialogButtonBox::AcceptRole);
    install_->setObjectName(QStringLiteral("InstallChecked"));
    install_->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(all_);
    layout->addWidget(tree_);
    layout->addWidget(total_);
    layout->addLayout(form);
    layout->addWidget(promise_);
    layout->addWidget(enable_);
    layout->addWidget(blocked_);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(again, &QPushButton::clicked, this, &InstallDialog::CheckAgainRequested);
    connect(all_, &QCheckBox::clicked, this,
            [this]
            {
                CheckThemAll(all_->checkState() != Qt::Unchecked);
            });
    connect(library_, &QComboBox::currentIndexChanged, this, &InstallDialog::ShowCategoriesOfTheChosenLibrary);
    connect(category_, &QComboBox::currentIndexChanged, this,
            &InstallDialog::ShowHowEachAddonStandsInTheChosenCategory);
    connect(model_, &QStandardItemModel::itemChanged, this, &InstallDialog::FollowTheCheck);

    FillTheLibraries();
    ShowTheOffer();

    if (!offer_.droppedOn.isEmpty())
    {
        LandOnWhereItWasDropped();
    }

    CapTheScrollBarOf(tree_, tree_->header());

    ShowTheSimulatorThatIsRunning();
}

void InstallDialog::FillTheLibraries()
{
    const QSignalBlocker quiet(library_);

    library_->clear();

    for (const InstallLibraryChoice& library : offer_.libraries)
    {
        library_->addItem(library.shown, library.path);
    }
}

void InstallDialog::ShowTheOffer()
{
    ListWhatWasFound();
    GiveTheCategoryColumnItsWidth();
    ShowCategoriesOfTheChosenLibrary();
}

void InstallDialog::TakeAFreshOffer(InstallOffer offer)
{
    const std::vector<ChosenAddon> kept = Chosen();
    const QString library = library_->currentData().toString();
    const bool choseACategory = category_->currentIndex() >= 0;
    const QString category = choseACategory ? category_->currentData().toString() : QString();

    offer_ = std::move(offer);

    model_->removeRows(0, model_->rowCount());

    FillTheLibraries();

    if (const int again = library_->findData(library); again >= 0)
    {
        const QSignalBlocker quiet(library_);

        library_->setCurrentIndex(again);
    }

    ShowTheOffer();

    if (choseACategory)
    {
        category_->setCurrentIndex(category_->findData(category));
    }

    KeepWhatWasChecked(kept);
    ShowTheSimulatorThatIsRunning();
}

void InstallDialog::KeepWhatWasChecked(const std::vector<ChosenAddon>& kept)
{
    for (int row = 0; row < model_->rowCount(); ++row)
    {
        const QStandardItem* archive = model_->item(row);

        for (int at = 0; at < AddonsOf(offer_, row); ++at)
        {
            const ChosenAddon* was = KeptAt(kept, row, at);
            const bool wasChecked = was != nullptr;
            const bool wasDiscarding = wasChecked && was->discardsTheQuarantinedCopy;
            QStandardItem* addon = archive->child(at);

            if (AsksForTheDiscard(*addon))
            {
                addon->child(0)->setCheckState(wasDiscarding ? Qt::Checked : Qt::Unchecked);
            }
            else if (addon->isEnabled())
            {
                addon->setCheckState(wasChecked ? Qt::Checked : Qt::Unchecked);
            }
        }
    }
}

const InstallStandingInALibrary& InstallDialog::StandingOf(const int archive, const int addon) const
{
    static const InstallStandingInALibrary fresh;

    const InstallAddonLine& line =
        offer_.archives[static_cast<std::size_t>(archive)].addons[static_cast<std::size_t>(addon)];
    const auto library = static_cast<std::size_t>(std::max(library_->currentIndex(), 0));

    if (category_->currentIndex() >= 0 && library < line.inEachCategory.size())
    {
        const std::vector<InstallStandingInALibrary>& inTheCategories = line.inEachCategory[library];
        const auto category = static_cast<std::size_t>(category_->currentIndex());

        if (category < inTheCategories.size())
        {
            return inTheCategories[category];
        }
    }

    return library < line.inEachLibrary.size() ? line.inEachLibrary[library] : fresh;
}

void InstallDialog::ListWhatWasFound()
{
    settling_ = true;

    model_->setHorizontalHeaderLabels({tr("Addon"), tr("Version"), tr("Installed"), tr("Size"), tr("Category")});

    int offered = 0;

    for (const InstallArchiveLine& archive : offer_.archives)
    {
        QStandardItem* named = ACell(archive.fileName);
        named->setData(true, EmphasisRole);

        qulonglong bytes = 0;

        for (const InstallAddonLine& addon : archive.addons)
        {
            const QList<QStandardItem*> row = {ACell(addon.folderName),
                                               ACell(addon.version.isEmpty() ? tr("not declared") : addon.version),
                                               ACell(QString()), ACell(AsSize(addon.bytes)), ACell(QString())};

            row.front()->setCheckable(true);
            row.front()->setCheckState(Qt::Checked);

            named->appendRow(row);

            bytes += addon.bytes;
        }

        if (archive.addons.empty())
        {
            QStandardItem* why = ACell(archive.refusal);
            why->setData(true, QuietRole);

            named->appendRow(why);
        }
        else
        {
            named->setData(QStringLiteral("%1 · %2").arg(
                               tr("%n addon", nullptr, static_cast<int>(archive.addons.size())), AsSize(bytes)),
                           QuietSuffixRole);
        }

        model_->appendRow(named);
        tree_->setFirstColumnSpanned(model_->rowCount() - 1, QModelIndex(), true);

        if (archive.addons.empty())
        {
            tree_->setFirstColumnSpanned(0, named->index(), true);
        }

        offered += static_cast<int>(archive.addons.size());
    }

    all_->setVisible(offered > 1);

    tree_->header()->setSectionResizeMode(AddonColumn, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(VersionColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(InstalledColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(SizeColumn, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(CategoryColumn, QHeaderView::Fixed);

    settling_ = false;
}

void InstallDialog::ShowHowEachAddonStands(const Discards discards)
{
    settling_ = true;

    int replacing = 0;
    bool theQuarantineHoldsOne = false;

    for (int row = 0; row < model_->rowCount(); ++row)
    {
        QStandardItem* archive = model_->item(row);

        for (int at = 0; at < AddonsOf(offer_, row); ++at)
        {
            const InstallStandingInALibrary& stands = StandingOf(row, at);
            QStandardItem* addon = archive->child(at);

            const bool wasDiscarding =
                discards == Discards::Keep && AsksForTheDiscard(*addon) && addon->child(0)->checkState() == Qt::Checked;
            const bool waits = (stands.standing == InstallStanding::ReplacesAndTheQuarantineHoldsOne && !wasDiscarding)
                || stands.standing == InstallStanding::AFolderIsInTheWay;
            const bool wasChosen = !addon->isEnabled() || addon->checkState() == Qt::Checked;
            const QString installed = stands.installedVersion.isEmpty() ? tr("not declared") : stands.installedVersion;

            addon->removeRows(0, addon->rowCount());
            archive->child(at, InstalledColumn)->setText(Replaces(stands) ? installed : QString());
            archive->child(at, CategoryColumn)->setText(Replaces(stands) ? stands.installedIn : QString());

            LetTheRowBeChosen(*addon, !waits && wasChosen);

            for (int column = 0; column < ColumnCount; ++column)
            {
                archive->child(at, column)->setEnabled(!waits);
            }

            replacing += Replaces(stands) ? 1 : 0;

            if (stands.standing == InstallStanding::ReplacesAndTheQuarantineHoldsOne)
            {
                QStandardItem* discard = ACell(
                    stands.quarantinedVersion.isEmpty()
                        ? tr("Permanently discard the copy in the quarantine to replace this one. It does not go "
                             "to the recycle bin.")
                        : tr("Permanently discard version %1 from the quarantine to replace this one. It does not "
                             "go to the recycle bin.")
                              .arg(stands.quarantinedVersion));
                discard->setCheckable(true);
                discard->setCheckState(wasDiscarding ? Qt::Checked : Qt::Unchecked);
                discard->setData(true, AlertRole);

                addon->appendRow(discard);
                theQuarantineHoldsOne = true;
            }

            if (stands.standing == InstallStanding::AFolderIsInTheWay)
            {
                QStandardItem* inTheWay = ACell(tr("This category has a folder with this name that is not an addon "
                                                   "(no manifest.json), and FS Organizer does not replace it."));
                inTheWay->setData(true, QuietRole);

                addon->appendRow(inTheWay);
            }

            if (addon->rowCount() > 0)
            {
                tree_->setFirstColumnSpanned(0, addon->index(), true);
            }
        }
    }

    QStringList promised;

    if (replacing > 0)
    {
        promised.append(tr("%n addon in this list is already installed in this library. It is replaced where it is, "
                           "whatever category you choose: the installed copy goes to the quarantine, where you can "
                           "restore it, and the new one keeps its links.",
                           nullptr, replacing));
    }

    if (theQuarantineHoldsOne)
    {
        promised.append(tr("The quarantine keeps one copy of each folder name, so the copy it already holds has to be "
                           "discarded before the addon is replaced again."));
    }

    promise_->setText(promised.join(QLatin1Char(' ')));
    promise_->setVisible(!promised.isEmpty());

    tree_->expandAll();

    settling_ = false;

    GiveTheListItsHeight();
}

void InstallDialog::GiveTheCategoryColumnItsWidth()
{
    const QFontMetrics measured(tree_->font());
    int widest = tree_->header()->sectionSizeHint(CategoryColumn) - kAroundACell;

    for (const InstallLibraryChoice& library : offer_.libraries)
    {
        for (const InstallCategoryChoice& category : library.categories)
        {
            widest = std::max(widest, measured.horizontalAdvance(category.shown));
        }
    }

    for (const InstallArchiveLine& archive : offer_.archives)
    {
        for (const InstallAddonLine& addon : archive.addons)
        {
            for (const InstallStandingInALibrary& stands : addon.inEachLibrary)
            {
                widest = std::max(widest, measured.horizontalAdvance(stands.installedIn));
            }
        }
    }

    tree_->header()->resizeSection(CategoryColumn, widest + kAroundACell);
}

void InstallDialog::GiveTheListItsHeight()
{
    QModelIndex last;
    int oneRow = 0;

    for (int row = 0; row < model_->rowCount(); ++row)
    {
        const QModelIndex archive = model_->index(row, AddonColumn);
        const int addons = model_->rowCount(archive);

        last = archive;

        if (addons == 0)
        {
            continue;
        }

        const QModelIndex lastAddon = model_->index(addons - 1, AddonColumn, archive);

        last = model_->rowCount(lastAddon) > 0 ? model_->index(0, AddonColumn, lastAddon) : lastAddon;

        if (oneRow == 0)
        {
            oneRow = tree_->visualRect(model_->index(0, AddonColumn, archive)).height();
        }
    }

    const int content = last.isValid() ? tree_->visualRect(last).bottom() + 1 + tree_->verticalScrollBar()->value() : 0;
    const int most = kMostRowsBeforeItScrolls * std::max(oneRow, 1);

    tree_->setFixedHeight(tree_->header()->sizeHint().height() + std::min(content, most) + 2 * tree_->frameWidth());

    SizeToTheContent(*this, kWide);
}

void InstallDialog::ShowCategoriesOfTheChosenLibrary()
{
    {
        const QSignalBlocker quiet(category_);

        category_->clear();

        if (library_->currentIndex() >= 0)
        {
            const InstallLibraryChoice& library = offer_.libraries[static_cast<std::size_t>(library_->currentIndex())];

            for (const InstallCategoryChoice& category : library.categories)
            {
                category_->addItem(category.shown, category.path);
            }
        }

        category_->setCurrentIndex(-1);
    }

    ShowHowEachAddonStands(Discards::Forget);
    ShowWhatIsChosen();
}

void InstallDialog::ShowHowEachAddonStandsInTheChosenCategory()
{
    ShowHowEachAddonStands(Discards::Keep);
    ShowWhatIsChosen();
}

void InstallDialog::LandOnWhereItWasDropped()
{
    for (std::size_t at = 0; at < offer_.libraries.size(); ++at)
    {
        const std::vector<InstallCategoryChoice>& categories = offer_.libraries[at].categories;

        const auto dropped = std::find_if(categories.begin(), categories.end(),
                                          [this](const InstallCategoryChoice& category)
                                          {
                                              return category.path == offer_.droppedOn;
                                          });

        if (dropped == categories.end())
        {
            continue;
        }

        library_->setCurrentIndex(static_cast<int>(at));
        category_->setCurrentIndex(static_cast<int>(dropped - categories.begin()));

        return;
    }
}

void InstallDialog::FollowTheCheck(QStandardItem* changed)
{
    if (settling_ || changed->column() != AddonColumn)
    {
        return;
    }

    settling_ = true;

    const int depth = DepthOf(*changed);

    if (depth == 2)
    {
        LetTheRowBeChosen(*changed->parent(), changed->checkState() == Qt::Checked);
    }

    if (depth == 1 && AsksForTheDiscard(*changed) && changed->checkState() != Qt::Checked)
    {
        changed->child(0)->setCheckState(Qt::Unchecked);
        LetTheRowBeChosen(*changed, false);
    }

    settling_ = false;

    ShowWhatIsChosen();
}

void InstallDialog::CheckThemAll(const bool checked)
{
    settling_ = true;

    for (int row = 0; row < model_->rowCount(); ++row)
    {
        for (int at = 0; at < AddonsOf(offer_, row); ++at)
        {
            QStandardItem* addon = model_->item(row)->child(at);

            if (!addon->isEnabled())
            {
                continue;
            }

            if (AsksForTheDiscard(*addon) && !checked)
            {
                addon->child(0)->setCheckState(Qt::Unchecked);
                LetTheRowBeChosen(*addon, false);

                continue;
            }

            addon->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    }

    settling_ = false;

    ShowWhatIsChosen();
}

void InstallDialog::ShowWhatIsChosen()
{
    const bool wasSettling = settling_;
    settling_ = true;

    const QString category = category_->currentIndex() < 0 ? QString() : category_->currentText();

    qulonglong bytes = 0;
    int chosen = 0;
    int fresh = 0;
    int offered = 0;
    QStringList replacedIn;

    for (int row = 0; row < model_->rowCount(); ++row)
    {
        const QStandardItem* archive = model_->item(row);
        const InstallArchiveLine& found = offer_.archives[static_cast<std::size_t>(row)];

        for (int at = 0; at < AddonsOf(offer_, row); ++at)
        {
            const InstallStandingInALibrary& stands = StandingOf(row, at);
            const bool isNew = stands.standing == InstallStanding::New;

            if (isNew)
            {
                archive->child(at, CategoryColumn)->setText(category);
            }

            offered += archive->child(at)->isEnabled() ? 1 : 0;

            if (archive->child(at)->checkState() != Qt::Checked)
            {
                continue;
            }

            ++chosen;
            bytes += found.addons[static_cast<std::size_t>(at)].bytes;
            fresh += isNew ? 1 : 0;

            if (Replaces(stands))
            {
                replacedIn.append(stands.installedIn);
            }
        }
    }

    const bool lacksACategory = fresh > 0 && category_->currentIndex() < 0;
    const Qt::CheckState some = chosen == offered ? Qt::Checked : Qt::PartiallyChecked;

    all_->setCheckState(chosen == 0 ? Qt::Unchecked : some);
    total_->setText(chosen == 0 ? tr("No addon checked")
                                : tr("%n addon checked · %1", nullptr, chosen).arg(AsSize(bytes)));
    category_->setEnabled(fresh > 0);
    category_->SayInstead(fresh > 0 ? QString() : WhereTheCheckedReplacementsStay(replacedIn));
    hint_->setVisible(lacksACategory);
    install_->setText(chosen == 0 ? tr("Install") : tr("Install %n addon", nullptr, chosen));
    install_->setEnabled(chosen > 0 && !lacksACategory && blocked_->isHidden());

    settling_ = wasSettling;
}

QString InstallDialog::WhereTheCheckedReplacementsStay(const QStringList& installedIn) const
{
    if (installedIn.isEmpty())
    {
        return {};
    }

    return AllTheSame(installedIn) ? installedIn.front() : tr("Each addon stays where it is");
}

void InstallDialog::ShowTheSimulatorThatIsRunning()
{
    const QString& process = offer_.runningSimulator;

    blockedBy_->setText(QStringLiteral("%1 %2").arg(tr("Files cannot be changed while the simulator is running."),
                                                    tr("Close %1 and check again.").arg(process)));
    blocked_->setVisible(!process.isEmpty());

    ShowWhatIsChosen();
    SizeToTheContent(*this, kWide);
}

std::vector<ChosenAddon> InstallDialog::Chosen() const
{
    std::vector<ChosenAddon> chosen;

    for (int row = 0; row < model_->rowCount(); ++row)
    {
        const QStandardItem* archive = model_->item(row);

        for (int at = 0; at < AddonsOf(offer_, row); ++at)
        {
            const QStandardItem* addon = archive->child(at);

            if (addon->checkState() == Qt::Checked)
            {
                const InstallArchiveLine& found = offer_.archives[static_cast<std::size_t>(row)];
                const InstallAddonLine& line = found.addons[static_cast<std::size_t>(at)];

                chosen.push_back({.archive = row,
                                  .addon = at,
                                  .discardsTheQuarantinedCopy = AsksForTheDiscard(*addon),
                                  .archivePath = found.path,
                                  .root = line.root,
                                  .folderName = line.folderName,
                                  .replaces = StandingOf(row, at).occupant,
                                  .source = found.source});
            }
        }
    }

    return chosen;
}

QString InstallDialog::ChosenCategory() const
{
    return category_->currentIndex() < 0 ? library_->currentData().toString() : category_->currentData().toString();
}

bool InstallDialog::EnablesAfterInstalling() const
{
    return enable_->isChecked();
}
