#include "view/ImportDialog.h"

#include <utility>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "domain/tree/AddonTree.h"
#include "support/PathText.h"
#include "support/SizeText.h"

ImportDialog::ImportDialog(std::vector<std::filesystem::path> folders,
                           const std::vector<TreeNode>& libraries,
                           const SimulatorProfile& profile,
                           const std::uintmax_t totalBytes,
                           QWidget* parent)
    : QDialog(parent), folders_(std::move(folders)), libraries_(libraries)
{
    setWindowTitle(tr("Importar para a biblioteca"));

    auto* chosen = new QListWidget(this);
    chosen->setSelectionMode(QAbstractItemView::NoSelection);
    for (const std::filesystem::path& folder : folders_)
    {
        chosen->addItem(AsText(folder));
    }

    library_ = new QComboBox(this);
    for (const Library& library : profile.libraries)
    {
        library_->addItem(library.label.empty() ? AsText(library.path)
                                                : QStringLiteral("%1 — %2").arg(QString::fromStdString(library.label),
                                                                                AsText(library.path)),
                          AsText(library.path));
    }

    category_ = new QComboBox(this);

    landing_ = new QLabel(this);
    landing_->setWordWrap(true);

    auto* total =
        new QLabel(tr("%1 em %2 serão copiados para a biblioteca e substituídos por links.")
                       .arg(AsSize(totalBytes), tr("%n pasta(s)", nullptr, static_cast<int>(folders_.size()))),
                   this);
    total->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow(tr("Biblioteca:"), library_);
    form->addRow(tr("Categoria:"), category_);
    form->addRow(tr("Vai virar:"), landing_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* confirm = buttons->addButton(tr("Importar"), QDialogButtonBox::AcceptRole);
    confirm->setDefault(true);
    confirm->setEnabled(!folders_.empty() && !profile.libraries.empty());

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(library_, &QComboBox::currentIndexChanged, this, &ImportDialog::ShowCategoriesOfTheChosenLibrary);
    connect(category_, &QComboBox::currentIndexChanged, this,
            [this]
            {
                landing_->setText(ChosenRequests().empty() ? QString() : AsText(ChosenRequests().front().Target()));
            });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Pastas selecionadas:"), this));
    layout->addWidget(chosen, 1);
    layout->addWidget(total);
    layout->addLayout(form);
    layout->addWidget(buttons);

    ShowCategoriesOfTheChosenLibrary();

    resize(640, 460);
}

void ImportDialog::ShowCategoriesOfTheChosenLibrary()
{
    category_->clear();

    const TreeNode* tree = LibraryTreeAt(libraries_, AsPath(library_->currentData().toString()));
    if (tree == nullptr)
    {
        return;
    }

    for (const TreeNode* folder : CategoriesUnder(*tree))
    {
        if (folder != tree && !HoldsAddonsOrWasDeclared(*folder))
        {
            continue;
        }

        const std::filesystem::path relative = folder->path.lexically_relative(tree->path);

        category_->addItem(folder == tree ? tr("(raiz da biblioteca)") : AsText(relative), AsText(folder->path));
    }
}

std::vector<ImportRequest> ImportDialog::ChosenRequests() const
{
    const std::filesystem::path category = AsPath(category_->currentData().toString());
    if (category.empty())
    {
        return {};
    }

    std::vector<ImportRequest> requests;
    requests.reserve(folders_.size());

    for (const std::filesystem::path& folder : folders_)
    {
        requests.push_back(ImportRequest{folder, category});
    }

    return requests;
}
