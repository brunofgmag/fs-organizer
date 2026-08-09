#include "view/legacy/LegacyImportDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"

namespace
{
    constexpr int kKindRole = Qt::UserRole;
    constexpr int kPathRole = Qt::UserRole + 1;

    enum class ImportKind
    {
        Library = 1,
        Category = 2,
        Presets = 3
    };

    constexpr int kDialogWidth = 720;
    constexpr int kDialogHeight = 520;

    template<typename Parent>
    QTreeWidgetItem* LineUnder(Parent* parent)
    {
        auto* item = new QTreeWidgetItem(parent);
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

        return item;
    }

    void Offer(QTreeWidgetItem* item)
    {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Checked);
    }

    void Mark(QTreeWidgetItem* item, const ImportKind kind, const std::filesystem::path& path)
    {
        item->setData(0, kKindRole, static_cast<int>(kind));
        item->setData(0, kPathRole, AsText(path));
    }

    void CollectChecked(const QTreeWidgetItem& item, const ImportKind kind, std::vector<std::filesystem::path>& into)
    {
        if (item.data(0, kKindRole).toInt() == static_cast<int>(kind) && item.checkState(0) == Qt::Checked)
        {
            into.push_back(AsPath(item.data(0, kPathRole).toString()));
        }

        for (int child = 0; child < item.childCount(); ++child)
        {
            CollectChecked(*item.child(child), kind, into);
        }
    }

    std::vector<std::filesystem::path> CheckedPathsOfKind(const QTreeWidget& tree, const ImportKind kind)
    {
        std::vector<std::filesystem::path> paths;

        for (int top = 0; top < tree.topLevelItemCount(); ++top)
        {
            CollectChecked(*tree.topLevelItem(top), kind, paths);
        }

        return paths;
    }
}

LegacyImportDialog::LegacyImportDialog(LegacyImportViewModel& viewModel, QWidget* parent)
    : QDialog(parent), viewModel_(viewModel)
{
    setWindowTitle(tr("Import from MSFS Addons Linker"));
    resize(kDialogWidth, kDialogHeight);

    auto* layout = new QVBoxLayout(this);

    auto* promise = new QLabel(tr("FS Organizer has read the old program's configuration and proposes what follows. "
                                  "Importing registers the library and the categories that are not here yet."),
                               this);
    promise->setWordWrap(true);
    layout->addWidget(promise);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("LegacyProposal"));
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({tr("Proposal"), tr("Status")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(tree_, 1);

    auto* buttons = new QDialogButtonBox(this);
    import_ = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(import_, &QPushButton::clicked, this, &LegacyImportDialog::Import);
    connect(tree_, &QTreeWidget::itemChanged, this,
            [this]
            {
                RefreshTheImportButton();
            });

    Fill();
}

void LegacyImportDialog::Fill()
{
    tree_->clear();

    for (const LegacyMigration& migration : viewModel_.Migrations())
    {
        QTreeWidgetItem* installation = LineUnder(tree_);
        installation->setText(0, AsText(migration.folder));
        installation->setExpanded(true);

        if (!migration.configurationWasRead)
        {
            installation->setText(1, tr("unreadable configuration"));
            continue;
        }

        for (const MigratableLibrary& library : migration.libraries)
        {
            FillLibrary(installation, library);
        }

        if (const std::size_t presets = viewModel_.PresetsWaitingIn(migration.presetsPath); presets > 0)
        {
            QTreeWidgetItem* line = LineUnder(installation);
            line->setText(0, tr("%n preset", nullptr, static_cast<int>(presets)));
            line->setText(1, tr("to import"));
            Mark(line, ImportKind::Presets, migration.presetsPath);
            Offer(line);
        }
    }

    if (tree_->topLevelItemCount() == 0)
    {
        LineUnder(tree_)->setText(0, tr("No MSFS Addons Linker installation was found."));
    }

    RefreshTheImportButton();
}

void LegacyImportDialog::FillLibrary(QTreeWidgetItem* installation, const MigratableLibrary& library) const
{
    QTreeWidgetItem* row = LineUnder(installation);
    row->setText(0, AsText(library.proposal.root));
    row->setExpanded(true);

    if (!library.rootExists)
    {
        row->setText(1, tr("the folder no longer exists"));
    }
    else if (library.proposal.state == ProposedState::AlreadyPresent)
    {
        row->setText(1, tr("already registered"));
    }
    else
    {
        row->setText(1, tr("new"));
        Mark(row, ImportKind::Library, library.proposal.root);
        Offer(row);
    }

    FillCategories(row, library);

    for (const std::filesystem::path& refused : library.proposal.refused)
    {
        QTreeWidgetItem* line = LineUnder(row);
        line->setText(0, AsText(refused));
        line->setText(1, tr("refused: the name does not become a folder"));
    }
}

void LegacyImportDialog::FillCategories(QTreeWidgetItem* row, const MigratableLibrary& library) const
{
    for (const ProposedCategory& category : library.proposal.categories)
    {
        QTreeWidgetItem* line = LineUnder(row);
        line->setText(0, AsText(category.relativePath));

        if (!library.rootExists)
        {
            line->setText(1, tr("unavailable"));
            continue;
        }

        if (category.state == ProposedState::AlreadyPresent)
        {
            line->setText(1, tr("already present"));
            continue;
        }

        line->setText(1, tr("new"));
        Mark(line, ImportKind::Category, library.proposal.root / category.relativePath);
        Offer(line);
    }
}

LegacyImportRequest LegacyImportDialog::WhatWasChecked() const
{
    return LegacyImportRequest{.libraryRoots = CheckedPathsOfKind(*tree_, ImportKind::Library),
                               .categories = CheckedPathsOfKind(*tree_, ImportKind::Category)};
}

std::vector<std::filesystem::path> LegacyImportDialog::PresetFoldersChecked() const
{
    return CheckedPathsOfKind(*tree_, ImportKind::Presets);
}

void LegacyImportDialog::RefreshTheImportButton() const
{
    const LegacyImportRequest request = WhatWasChecked();

    import_->setEnabled(!request.libraryRoots.empty() || !request.categories.empty()
                        || !PresetFoldersChecked().empty());
}

void LegacyImportDialog::Import()
{
    const LegacyImportReport report = viewModel_.Import(WhatWasChecked());

    LegacyPresetReport presets;
    for (const std::filesystem::path& folder : PresetFoldersChecked())
    {
        const LegacyPresetReport one = viewModel_.ImportPresets(folder);
        presets.imported += one.imported;
        presets.nameAlreadyTaken += one.nameAlreadyTaken;
        presets.entriesNotFound += one.entriesNotFound;
    }

    QString said = tr("%1 and %2 imported.")
                       .arg(tr("%n library", nullptr, static_cast<int>(report.librariesRegistered)),
                            tr("%n category", nullptr, static_cast<int>(report.categoriesDeclared)));

    if (presets.imported > 0 || presets.nameAlreadyTaken > 0)
    {
        said += tr(" %1 imported, %2 with a name already used here.")
                    .arg(tr("%n preset", nullptr, static_cast<int>(presets.imported)))
                    .arg(presets.nameAlreadyTaken);
    }

    if (presets.entriesNotFound > 0)
    {
        said += tr(" %n name the presets cite was not found in any library.", nullptr,
                   static_cast<int>(presets.entriesNotFound));
    }

    if (!report.refused.empty())
    {
        said += tr(" %n refused for being inside an already registered library.", nullptr,
                   static_cast<int>(report.refused.size()));
    }

    emit StatusChanged(said);

    accept();
}
