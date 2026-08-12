#include "view/community/ImportDialog.h"

#include <algorithm>
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
#include "view/WheelGuard.h"
#include "view/theme/ModernistMetrics.h"

ImportDialog::ImportDialog(std::vector<ImportRequest> chosen,
                           const std::vector<TreeNode>& libraries,
                           const SimulatorProfile& profile,
                           const std::uintmax_t totalBytes,
                           QWidget* parent)
    : QDialog(parent), chosen_(std::move(chosen)), libraries_(libraries)
{
    setWindowTitle(tr("Import into the library"));

    auto* picked = new QListWidget(this);
    picked->setSelectionMode(QAbstractItemView::NoSelection);
    picked->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    for (const ImportRequest& request : chosen_)
    {
        picked->addItem(request.CameFromAnotherProgram()
                            ? tr("%1 · installed by another program in %2")
                                  .arg(AsText(request.source), AsText(request.externalSource))
                            : AsText(request.source));
    }

    library_ = new QComboBox(this);
    for (const Library& library : profile.libraries)
    {
        library_->addItem(library.label.empty() ? AsText(library.path)
                                                : QStringLiteral("%1 · %2").arg(QString::fromStdString(library.label),
                                                                                AsText(library.path)),
                          AsText(library.path));
    }

    category_ = new QComboBox(this);

    LetTheWheelScrollPastUnlessTheWidgetHasFocus(library_);
    LetTheWheelScrollPastUnlessTheWidgetHasFocus(category_);

    landing_ = new QLabel(this);
    landing_->setWordWrap(true);

    auto* total = new QLabel(tr("%1 in %2 will be copied to the library and replaced by links.")
                                 .arg(AsSize(totalBytes), tr("%n folder", nullptr, static_cast<int>(chosen_.size()))),
                             this);
    total->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow(tr("Library:"), library_);
    form->addRow(tr("Category:"), category_);
    form->addRow(tr("Will become:"), landing_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* confirm = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    confirm->setDefault(true);
    confirm->setEnabled(!chosen_.empty() && !profile.libraries.empty());

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(library_, &QComboBox::currentIndexChanged, this, &ImportDialog::ShowCategoriesOfTheChosenLibrary);
    connect(category_, &QComboBox::currentIndexChanged, this,
            [this]
            {
                landing_->setText(ChosenRequests().empty() ? QString() : AsText(ChosenRequests().front().Target()));
            });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->addWidget(new QLabel(tr("Selected folders:"), this));
    layout->addWidget(picked, 1);
    layout->addWidget(total);
    layout->addLayout(form);

    const auto owned = static_cast<int>(std::count_if(chosen_.begin(), chosen_.end(),
                                                      [](const ImportRequest& request)
                                                      {
                                                          return request.CameFromAnotherProgram();
                                                      }));

    if (owned > 0)
    {
        auto* caveat =
            new QLabel(tr("%n folder above is installed by another program, and that program does not know about the "
                          "link this leaves behind: its next update can write inside the link, or replace it and give "
                          "you two copies. You can give it back later, from Delete in the library.",
                          nullptr, owned),
                       this);
        caveat->setWordWrap(true);

        layout->addWidget(caveat);
    }

    layout->addWidget(buttons);

    ShowCategoriesOfTheChosenLibrary();

    SizeToTheContent(*this, 640);
}

void ImportDialog::ShowCategoriesOfTheChosenLibrary() const
{
    category_->clear();

    const TreeNode* tree = LibraryTreeAt(libraries_, AsPath(library_->currentData().toString()));
    if (tree == nullptr)
    {
        return;
    }

    for (const TreeNode* folder : CategoriesOfferedIn(*tree, true))
    {
        const std::filesystem::path relative = folder->path.lexically_relative(tree->path);

        category_->addItem(folder == tree ? tr("(library root)") : AsText(relative), AsText(folder->path));
    }
}

std::vector<ImportRequest> ImportDialog::ChosenRequests() const
{
    const std::filesystem::path category = AsPath(category_->currentData().toString());
    if (category.empty())
    {
        return {};
    }

    std::vector<ImportRequest> requests = chosen_;

    for (ImportRequest& request : requests)
    {
        request.category = category;
    }

    return requests;
}
