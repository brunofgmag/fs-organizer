#include "view/shell/ArchiveDrop.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <QtCore/QMimeData>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QDropEvent>
#include <QtWidgets/QWidget>

#include "support/PathText.h"
#include "support/SizeText.h"
#include "view/library/InstallDialog.h"
#include "view/library/InstallReportDialog.h"
#include "view/shell/ArchiveDropLibrary.h"
#include "view/shell/ArchiveDropSign.h"
#include "view/shell/StepProgressDialog.h"

namespace
{
    constexpr std::array<QLatin1StringView, 3> kArchiveSuffixes{QLatin1StringView(".zip"), QLatin1StringView(".rar"),
                                                                QLatin1StringView(".7z")};

    [[nodiscard]] bool IsAnArchive(const QString& file)
    {
        return std::ranges::any_of(kArchiveSuffixes,
                                   [&file](const QLatin1StringView suffix)
                                   {
                                       return file.endsWith(suffix, Qt::CaseInsensitive);
                                   });
    }

    [[nodiscard]] std::vector<QString> LocalFilesOf(const QMimeData& dragged)
    {
        std::vector<QString> files;

        for (const QUrl& url : dragged.urls())
        {
            if (url.isLocalFile())
            {
                files.push_back(url.toLocalFile());
            }
        }

        return files;
    }

    [[nodiscard]] QStringList StepsOf(const SourceKind source)
    {
        if (source == SourceKind::Folder)
        {
            return {ArchiveDrop::tr("Copying"), ArchiveDrop::tr("Checking what was copied"),
                    ArchiveDrop::tr("Putting into place")};
        }

        return {ArchiveDrop::tr("Extracting"), ArchiveDrop::tr("Checking what was extracted"),
                ArchiveDrop::tr("Putting into place")};
    }

    [[nodiscard]] bool AnyHoldsAnAddon(const InstallOffer& offer)
    {
        return std::ranges::any_of(offer.archives,
                                   [](const InstallArchiveLine& archive)
                                   {
                                       return !archive.addons.empty();
                                   });
    }

    [[nodiscard]] int IndexOf(const InstallStep step)
    {
        switch (step)
        {
        case InstallStep::Extracting:
        case InstallStep::Copying: return 0;
        case InstallStep::Checking: return 1;
        case InstallStep::PuttingIntoPlace: return 2;
        }

        return 0;
    }
}

ArchiveDrop::ArchiveDrop(QWidget& window, ArchiveInstallViewModel& viewModel, ArchiveDropLibrary& library)
    : QObject(&window), window_(window), viewModel_(viewModel), library_(library)
{
    window_.setAcceptDrops(true);
    window_.installEventFilter(this);

    sign_ = new ArchiveDropSign(&window_);

    readingIsSlow_ = new QTimer(this);
    readingIsSlow_->setObjectName(QStringLiteral("ReadingIsSlow"));
    readingIsSlow_->setSingleShot(true);
    readingIsSlow_->setInterval(kReadingIsSlowAfter);

    connect(readingIsSlow_, &QTimer::timeout, this, &ArchiveDrop::OpenTheReadingDialog);
    connect(&viewModel_, &ArchiveInstallViewModel::ReadingArchive, this, &ArchiveDrop::RememberWhatIsBeingRead);
    connect(&viewModel_, &ArchiveInstallViewModel::OfferReady, this, &ArchiveDrop::OnOffer);
    connect(&viewModel_, &ArchiveInstallViewModel::ReadingStopped, this, &ArchiveDrop::OnReadingStopped);
    connect(&viewModel_, &ArchiveInstallViewModel::InstallingArchive, this, &ArchiveDrop::ShowTheSubject);
    connect(&viewModel_, &ArchiveInstallViewModel::InstallingAddon, this, &ArchiveDrop::ShowTheAddon);
    connect(&viewModel_, &ArchiveInstallViewModel::StepChanged, this, &ArchiveDrop::ShowTheStep);
    connect(&viewModel_, &ArchiveInstallViewModel::Progressed, this, &ArchiveDrop::ShowTheBytes);
    connect(&viewModel_, &ArchiveInstallViewModel::Cancelling, this, &ArchiveDrop::ShowItIsCancelling);
    connect(&viewModel_, &ArchiveInstallViewModel::Finished, this, &ArchiveDrop::OnFinished);

    connect(&viewModel_, &ArchiveInstallViewModel::LandedAndReadable, this,
            [this](const std::vector<std::filesystem::path>& folders)
            {
                library_.EnableAddons(folders);
            });
}

bool ArchiveDrop::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != &window_)
    {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type())
    {
    case QEvent::DragEnter:
        ForgetTheDrag();
        Invite(*static_cast<QDropEvent*>(event));
        return true;
    case QEvent::DragMove: Invite(*static_cast<QDropEvent*>(event)); return true;
    case QEvent::Drop: Receive(*static_cast<QDropEvent*>(event)); return true;
    case QEvent::DragLeave:
        ForgetTheDrag();
        sign_->Leave();
        return false;
    default: return QObject::eventFilter(watched, event);
    }
}

void ArchiveDrop::ForgetTheDrag()
{
    sources_.reset();
    draggedFrom_ = nullptr;
}

const std::vector<std::filesystem::path>& ArchiveDrop::SourcesIn(const QMimeData& dragged)
{
    if (!sources_.has_value() || draggedFrom_ != &dragged)
    {
        std::vector<std::filesystem::path> sources;

        for (const QString& file : LocalFilesOf(dragged))
        {
            const std::filesystem::path path = AsPath(file);

            if (IsAnArchive(file) || viewModel_.IsAFolder(path))
            {
                sources.push_back(path);
            }
        }

        sources_ = std::move(sources);
        draggedFrom_ = &dragged;
    }

    return *sources_;
}

ArchiveDrop::Welcome ArchiveDrop::WelcomeFor(const QMimeData& dragged)
{
    if (viewModel_.Busy())
    {
        return Welcome::Busy;
    }

    if (SourcesIn(dragged).empty())
    {
        return Welcome::NotASource;
    }

    return library_.HasALibrary() ? Welcome::Yes : Welcome::NoLibrary;
}

void ArchiveDrop::Invite(QDropEvent& event)
{
    switch (WelcomeFor(*event.mimeData()))
    {
    case Welcome::Busy:
        sign_->Leave();
        event.ignore();
        return;
    case Welcome::NotASource:
        Refuse(event, tr("Only .zip, .rar and .7z archives and addon folders can be installed."));
        return;
    case Welcome::NoLibrary: Refuse(event, tr("Add a library before installing an addon.")); return;
    case Welcome::Yes: break;
    }

    InviteIntoWhatIsUnder(event.position().toPoint());
    event.acceptProposedAction();
}

void ArchiveDrop::Refuse(QDropEvent& event, const QString& sentence) const
{
    sign_->Refuse(sentence, event.position().toPoint());
    event.ignore();
}

void ArchiveDrop::InviteIntoWhatIsUnder(const QPoint& point) const
{
    const std::optional<DropCategory> category = library_.CategoryUnder(window_, point);

    if (category.has_value())
    {
        sign_->InviteInto(category->row, tr("Drop to install into %1").arg(category->shown), point);
        return;
    }

    sign_->InviteInto({}, tr("Drop to install into the library. You choose the category next."), point);
}

void ArchiveDrop::Receive(QDropEvent& event)
{
    sign_->Leave();

    if (WelcomeFor(*event.mimeData()) != Welcome::Yes)
    {
        ForgetTheDrag();
        event.ignore();
        return;
    }

    const std::optional<DropCategory> category = library_.CategoryUnder(window_, event.position().toPoint());
    const std::vector<std::filesystem::path> sources = SourcesIn(*event.mimeData());

    ForgetTheDrag();
    event.acceptProposedAction();
    ReadWhatWasDropped(sources, category.has_value() ? category->path : std::filesystem::path{});
}

void ArchiveDrop::ReadWhatWasDropped(const std::vector<std::filesystem::path>& files,
                                     const std::filesystem::path& category)
{
    readingSubject_.clear();
    readingIsSlow_->start();
    viewModel_.Read(files, category);
}

QString ArchiveDrop::SubjectOf(const int archive, const int of, const QString& fileName)
{
    return of > 1 ? tr("Item %1 of %2 · %3").arg(archive).arg(of).arg(fileName) : fileName;
}

void ArchiveDrop::RememberWhatIsBeingRead(const int archive, const int of, const QString& fileName)
{
    readingSubject_ = SubjectOf(archive, of, fileName);

    if (reading_ != nullptr)
    {
        reading_->ShowTheSubject(readingSubject_);
    }
}

StepProgressDialog* ArchiveDrop::ProgressDialog(const QString& title, const QStringList& steps)
{
    auto* dialog = new StepProgressDialog(title, steps, &window_);

    connect(dialog, &StepProgressDialog::Cancelled, &viewModel_, &ArchiveInstallViewModel::Cancel);
    connect(dialog, &QDialog::rejected, &viewModel_, &ArchiveInstallViewModel::Cancel);

    dialog->show();

    return dialog;
}

void ArchiveDrop::Dismiss(QPointer<StepProgressDialog>& dialog)
{
    if (dialog == nullptr)
    {
        return;
    }

    disconnect(dialog.data(), nullptr, &viewModel_, nullptr);
    dialog->close();
    dialog->deleteLater();
    dialog.clear();
}

void ArchiveDrop::OpenTheReadingDialog()
{
    if (reading_ != nullptr || !viewModel_.Busy())
    {
        return;
    }

    reading_ = ProgressDialog(tr("Install into the library"), {tr("Reading what it holds")});
    reading_->ShowThatItCannotBeMeasured();
    reading_->ShowTheSubject(readingSubject_);
}

void ArchiveDrop::OnReadingStopped()
{
    readingIsSlow_->stop();
    Dismiss(reading_);
}

void ArchiveDrop::OnOffer(const InstallOffer& offer)
{
    readingIsSlow_->stop();
    Dismiss(reading_);

    if (offering_ != nullptr)
    {
        offering_->TakeAFreshOffer(offer);
        return;
    }

    if (AnyHoldsAnAddon(offer))
    {
        OfferTheInstall(offer);
        return;
    }

    ShowWhatCannotBeInstalled(offer);
}

void ArchiveDrop::ShowWhatCannotBeInstalled(const InstallOffer& offer)
{
    InstallReportDialog report(ArchiveInstallViewModel::ReportOfWhatCannotBeInstalled(offer), &window_);

    static_cast<void>(report.exec());
}

void ArchiveDrop::OfferTheInstall(const InstallOffer& offer)
{
    InstallDialog dialog(offer, &window_);
    offering_ = &dialog;

    connect(&dialog, &InstallDialog::CheckAgainRequested, &viewModel_, &ArchiveInstallViewModel::CheckAgain);

    const bool accepted = dialog.exec() == QDialog::Accepted;
    offering_.clear();

    if (accepted)
    {
        InstallWhatWasChosen(dialog);
    }
}

void ArchiveDrop::InstallWhatWasChosen(const InstallDialog& dialog)
{
    if (viewModel_.Busy())
    {
        return;
    }

    const std::vector<ChosenAddon> chosen = dialog.Chosen();
    const SourceKind first = chosen.empty() ? SourceKind::Archive : chosen.front().source;

    installing_ = ProgressDialog(tr("Installing into the library"), StepsOf(first));

    viewModel_.Install(chosen, dialog.ChosenCategory(), dialog.EnablesAfterInstalling());
}

void ArchiveDrop::ShowTheSubject(const int archive, const int of, const QString& fileName)
{
    if (installing_ != nullptr)
    {
        installing_->ShowTheSubject(SubjectOf(archive, of, fileName));
    }
}

void ArchiveDrop::ShowTheStep(const InstallStep step)
{
    if (installing_ == nullptr)
    {
        return;
    }

    if (step == InstallStep::Copying || step == InstallStep::Extracting)
    {
        installing_->NameTheSteps(StepsOf(step == InstallStep::Copying ? SourceKind::Folder : SourceKind::Archive));
    }

    installing_->ShowTheStep(IndexOf(step));
}

void ArchiveDrop::ShowTheBytes(const qulonglong copied, const qulonglong total)
{
    if (installing_ == nullptr)
    {
        return;
    }

    installing_->ShowTheProgress(copied, total);
    installing_->ShowTheDetail(tr("%1 of %2").arg(AsSize(copied), AsSize(total)));
}

void ArchiveDrop::ShowTheAddon(const int addon, const int of, const QString& folderName)
{
    if (installing_ != nullptr)
    {
        installing_->ShowTheDetail(tr("Addon %1 of %2 · %3").arg(addon).arg(of).arg(folderName));
    }
}

void ArchiveDrop::ShowItIsCancelling()
{
    if (installing_ != nullptr)
    {
        installing_->ShowItIsCancelling(tr("Deleting the temporary copy…"));
    }
}

void ArchiveDrop::OnFinished(const InstallOutcome& outcome)
{
    Dismiss(installing_);

    const bool somethingLanded = !outcome.landed.empty();

    if (somethingLanded)
    {
        emit LibraryRequested();
        library_.SelectOnceShown(outcome.landed);
        library_.RefreshUndoState();
    }

    if (outcome.report.has_value())
    {
        InstallReportDialog report(*outcome.report, &window_);

        static_cast<void>(report.exec());
    }

    if (somethingLanded)
    {
        emit StatusChanged(tr("%n addon installed into the library.", nullptr, outcome.installed));
    }
}
