#include "view/shell/LongOperationProgress.h"

#include "view/shell/ImportProgressDialog.h"
#include "viewmodel/FailureText.h"

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
    progress_ = new ImportProgressDialog(folders, over_);

    connect(progress_, &ImportProgressDialog::Cancelled, &viewModel_, &ImportViewModel::Cancel);

    progress_->show();
}

void LongOperationProgress::ShowTheBytes(const qulonglong copiedBytes,
                                         const qulonglong totalBytes,
                                         const int folder,
                                         const OperationKind step)
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->ShowTheBytes(copiedBytes, totalBytes, folder, step);
}

void LongOperationProgress::ShowTheStep(const OperationKind kind)
{
    if (progress_ == nullptr)
    {
        return;
    }

    progress_->ShowTheStep(kind, NameOfImportStep(kind));
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
