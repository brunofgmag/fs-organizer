#include "view/shell/LongOperationProgress.h"

#include <QtWidgets/QProgressDialog>

#include "viewmodel/FailureText.h"
#include "viewmodel/SizeSummary.h"

LongOperationProgress::LongOperationProgress(ImportViewModel& viewModel, QWidget* over)
    : QObject(over), viewModel_(viewModel), over_(over)
{
    connect(&viewModel_, &ImportViewModel::Started, this, &LongOperationProgress::Open);
    connect(&viewModel_, &ImportViewModel::Progressed, this, &LongOperationProgress::ShowTheBytes);
    connect(&viewModel_, &ImportViewModel::StepChanged, this, &LongOperationProgress::ShowTheStep);
    connect(&viewModel_, &ImportViewModel::Idle, this, &LongOperationProgress::Close);
}

void LongOperationProgress::Open(const int folders)
{
    folders_ = folders;
    folder_ = 0;
    step_ = NameOfImportStep(OperationKind::ImportCopyToStaging);

    progress_ = new QProgressDialog(step_, tr("Cancel"), 0, 100, over_);
    progress_->setWindowModality(Qt::ApplicationModal);
    progress_->setMinimumDuration(0);
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    progress_->setValue(0);

    connect(progress_, &QProgressDialog::canceled, &viewModel_, &ImportViewModel::Cancel);
}

void LongOperationProgress::ShowTheBytes(const qulonglong copiedBytes, const qulonglong totalBytes, const int folder)
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->setRange(0, 100);
    progress_->setLabelText(
        tr("%1 · %2 · %3 of %4")
            .arg(tr("Folder %1 of %2").arg(folder).arg(folders_), step_, AsSize(copiedBytes), AsSize(totalBytes)));

    progress_->setValue(totalBytes == 0 ? 0 : static_cast<int>(copiedBytes * 100 / totalBytes));
}

void LongOperationProgress::ShowTheStep(const QString& step)
{
    const bool copying = step == NameOfImportStep(OperationKind::ImportCopyToStaging);

    step_ = step;
    folder_ += copying ? 1 : 0;

    if (progress_ == nullptr)
    {
        return;
    }

    progress_->setLabelText(tr("Folder %1 of %2 · %3").arg(folder_).arg(folders_).arg(step_));

    if (!copying)
    {
        progress_->setRange(0, 0);
    }
}

void LongOperationProgress::Close()
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->close();
    progress_->deleteLater();
    progress_ = nullptr;
}
