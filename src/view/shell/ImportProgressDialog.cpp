#include "view/shell/ImportProgressDialog.h"

#include <algorithm>

#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "viewmodel/FailureText.h"
#include "viewmodel/SizeSummary.h"

namespace
{
    QProgressBar* ABarThatStartsEmpty(QWidget* on)
    {
        auto* bar = new QProgressBar(on);
        bar->setRange(0, 100);
        bar->setValue(0);

        return bar;
    }
}

ImportProgressDialog::ImportProgressDialog(const int folders, QWidget* over) : QDialog(over), folders_(folders)
{
    setWindowModality(Qt::ApplicationModal);
    setSizeGripEnabled(false);

    folderLine_ = new QLabel(this);
    copyLine_ = new QLabel(NameOfImportStep(OperationKind::ImportCopyToStaging), this);
    copyBar_ = ABarThatStartsEmpty(this);
    checkLine_ = new QLabel(NameOfImportStep(OperationKind::ImportVerifyStaging), this);
    checkBar_ = ABarThatStartsEmpty(this);
    bytesLine_ = new QLabel(this);
    cancel_ = new QPushButton(tr("Cancel"), this);

    checkLine_->hide();
    checkBar_->hide();

    auto* column = new QVBoxLayout(this);
    column->addWidget(folderLine_);
    column->addWidget(copyLine_);
    column->addWidget(copyBar_);
    column->addWidget(checkLine_);
    column->addWidget(checkBar_);
    column->addWidget(bytesLine_);
    column->addWidget(cancel_, 0, Qt::AlignRight);

    connect(cancel_, &QPushButton::clicked, this, &ImportProgressDialog::Cancelled);

    const QMargins around = column->contentsMargins();
    const int widest = std::max(copyLine_->fontMetrics().horizontalAdvance(copyLine_->text()),
                                checkLine_->fontMetrics().horizontalAdvance(checkLine_->text()));

    setMinimumWidth(widest + around.left() + around.right());

    ShowTheFolder(1);
}

void ImportProgressDialog::ShowTheFolder(const int folder)
{
    folder_ = folder;
    folderLine_->setText(tr("Folder %1 of %2").arg(folder_).arg(folders_));
}

QProgressBar* ImportProgressDialog::BarOf(const OperationKind step) const
{
    if (step == OperationKind::ImportVerifyStaging)
    {
        return checkBar_;
    }

    if (step == OperationKind::ImportCopyToStaging)
    {
        return copyBar_;
    }

    return nullptr;
}

void ImportProgressDialog::ShowTheStep(const OperationKind kind, const QString& named)
{
    if (kind == OperationKind::ImportVerifyStaging)
    {
        checkLine_->show();
        checkBar_->show();

        return;
    }

    if (kind == OperationKind::ImportCopyToStaging)
    {
        checkLine_->hide();
        checkBar_->hide();
        checkBar_->setValue(0);
        copyBar_->setValue(0);

        return;
    }

    bytesLine_->setText(named);
}

void ImportProgressDialog::ShowTheBytes(const qulonglong done,
                                        const qulonglong total,
                                        const int folder,
                                        const OperationKind step)
{
    if (folder != folder_)
    {
        ShowTheFolder(folder);
    }

    bytesLine_->setText(tr("%1 of %2").arg(AsSize(done), AsSize(total)));

    QProgressBar* bar = BarOf(step);
    if (bar == nullptr)
    {
        return;
    }

    bar->setValue(total == 0 ? 0 : static_cast<int>(done * 100 / total));
}
