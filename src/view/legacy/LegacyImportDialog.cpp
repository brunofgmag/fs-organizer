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

    constexpr int kLibraryKind = 1;
    constexpr int kCategoryKind = 2;
    constexpr int kPresetsKind = 3;

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

    void Mark(QTreeWidgetItem* item, const int kind, const std::filesystem::path& path)
    {
        item->setData(0, kKindRole, kind);
        item->setData(0, kPathRole, AsText(path));
    }

    void CollectChecked(const QTreeWidgetItem& item, const int kind, std::vector<std::filesystem::path>& into)
    {
        if (item.data(0, kKindRole).toInt() == kind && item.checkState(0) == Qt::Checked)
        {
            into.push_back(AsPath(item.data(0, kPathRole).toString()));
        }

        for (int child = 0; child < item.childCount(); ++child)
        {
            CollectChecked(*item.child(child), kind, into);
        }
    }

    std::vector<std::filesystem::path> CheckedPathsOfKind(const QTreeWidget& tree, const int kind)
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
    setWindowTitle(tr("Importar do MSFS Addons Linker"));
    resize(kDialogWidth, kDialogHeight);

    auto* layout = new QVBoxLayout(this);

    auto* promise = new QLabel(tr("O FS Organizer leu a configuração do programa antigo e propõe o que segue. Nada é "
                                  "aplicado antes de você confirmar, e nenhum arquivo é movido ou apagado: importar "
                                  "cadastra a biblioteca e as categorias que ainda não existem aqui."),
                               this);
    promise->setWordWrap(true);
    layout->addWidget(promise);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("LegacyProposal"));
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({tr("Proposta"), tr("Situação")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(tree_, 1);

    auto* buttons = new QDialogButtonBox(this);
    import_ = buttons->addButton(tr("Importar"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Cancelar"), QDialogButtonBox::RejectRole);
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
            installation->setText(1, tr("configuração ilegível"));
            continue;
        }

        for (const MigratableLibrary& library : migration.libraries)
        {
            QTreeWidgetItem* row = LineUnder(installation);
            row->setText(0, AsText(library.proposal.root));
            row->setExpanded(true);

            if (!library.rootExists)
            {
                row->setText(1, tr("a pasta não existe mais"));
            }
            else if (library.proposal.state == ProposedState::AlreadyPresent)
            {
                row->setText(1, tr("já cadastrada"));
            }
            else
            {
                row->setText(1, tr("nova"));
                Mark(row, kLibraryKind, library.proposal.root);
                Offer(row);
            }

            for (const ProposedCategory& category : library.proposal.categories)
            {
                QTreeWidgetItem* line = LineUnder(row);
                line->setText(0, AsText(category.relativePath));

                if (!library.rootExists)
                {
                    line->setText(1, tr("indisponível"));
                    continue;
                }

                if (category.state == ProposedState::AlreadyPresent)
                {
                    line->setText(1, tr("já presente"));
                    continue;
                }

                line->setText(1, tr("nova"));
                Mark(line, kCategoryKind, library.proposal.root / category.relativePath);
                Offer(line);
            }

            for (const std::filesystem::path& refused : library.proposal.refused)
            {
                QTreeWidgetItem* line = LineUnder(row);
                line->setText(0, AsText(refused));
                line->setText(1, tr("recusada: o nome não vira pasta"));
            }
        }

        if (const std::size_t presets = viewModel_.PresetsWaitingIn(migration.presetsPath); presets > 0)
        {
            QTreeWidgetItem* line = LineUnder(installation);
            line->setText(0, tr("%1 preset(s)").arg(presets));
            line->setText(1, tr("a importar"));
            Mark(line, kPresetsKind, migration.presetsPath);
            Offer(line);
        }
    }

    if (tree_->topLevelItemCount() == 0)
    {
        LineUnder(tree_)->setText(0, tr("Nenhuma instalação do MSFS Addons Linker foi encontrada."));
    }

    RefreshTheImportButton();
}

LegacyImportRequest LegacyImportDialog::WhatWasChecked() const
{
    return LegacyImportRequest{CheckedPathsOfKind(*tree_, kLibraryKind), CheckedPathsOfKind(*tree_, kCategoryKind)};
}

std::vector<std::filesystem::path> LegacyImportDialog::PresetFoldersChecked() const
{
    return CheckedPathsOfKind(*tree_, kPresetsKind);
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

    QString said = tr("%1 biblioteca(s) e %2 categoria(s) importadas.")
                       .arg(report.librariesRegistered)
                       .arg(report.categoriesDeclared);

    if (presets.imported > 0 || presets.nameAlreadyTaken > 0)
    {
        said += tr(" %1 preset(s) importados, %2 com nome já usado aqui.")
                    .arg(presets.imported)
                    .arg(presets.nameAlreadyTaken);
    }

    if (!report.refused.empty())
    {
        said += tr(" %1 recusada(s) por estar dentro de biblioteca já cadastrada.").arg(report.refused.size());
    }

    emit StatusChanged(said);

    accept();
}
