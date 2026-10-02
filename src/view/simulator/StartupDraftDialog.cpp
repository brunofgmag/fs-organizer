#include "view/simulator/StartupDraftDialog.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QtCore/QTimer>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include "support/PathText.h"
#include "view/TextThatIsNeverCut.h"
#include "view/theme/ModernistMetrics.h"
#include "view/theme/ModernistPaint.h"
#include "viewmodel/FailureText.h"

namespace
{
    constexpr int kDialogWidth = 620;
    constexpr int kFieldColumnGap = 12;
    constexpr int kFieldRowGap = 8;
    constexpr int kTextLineInAButton = 6;
    constexpr int kSectionGap = 16;
    constexpr int kLinesGap = 2;
    constexpr int kBelowTheLines = 4;
    constexpr int kPathWrittenAt = 1;

    QLabel* FieldName(const QString& text, QWidget* parent)
    {
        auto* name = new QLabel(text, parent);
        name->setObjectName(QStringLiteral("DetailFieldName"));

        return name;
    }

    QLabel* HiddenLine(const QString& objectName, QWidget* parent)
    {
        auto* line = new QLabel(parent);
        line->setObjectName(objectName);
        line->setWordWrap(true);
        line->hide();

        return line;
    }

    void ReplaceTheText(TextThatIsNeverCut*& slot, QVBoxLayout* column, const int at, const QString& text)
    {
        delete slot;
        slot = nullptr;

        if (text.isEmpty())
        {
            return;
        }

        slot = new TextThatIsNeverCut(text, column->parentWidget());
        column->insertWidget(at, slot);
    }
}

StartupDraftDialog::StartupDraftDialog(CheckOfTheFile check,
                                       const std::optional<StartupDraft>& editing,
                                       QWidget* parent)
    : QDialog(parent),
      check_(std::move(check)),
      editing_(editing.has_value()),
      nameFollowsTheFile_(!editing.has_value())
{
    setWindowTitle(editing_ ? tr("Edit startup entry") : tr("Add startup entry"));

    name_ = new QLineEdit(this);
    name_->setObjectName(QStringLiteral("EntryName"));

    commandLine_ = new QLineEdit(this);
    commandLine_->setObjectName(QStringLiteral("EntryCommandLine"));
    commandLine_->setPlaceholderText(tr("Optional"));

    auto* choose = new QPushButton(tr("Choose…"), this);
    choose->setAutoDefault(false);

    auto* programName = FieldName(tr("Program"), this);
    programName->setContentsMargins(0, kTextLineInAButton, 0, 0);

    auto* fields = new QGridLayout;
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setHorizontalSpacing(kFieldColumnGap);
    fields->setVerticalSpacing(kFieldRowGap);
    fields->setColumnStretch(1, 1);
    fields->addWidget(programName, 0, 0, Qt::AlignTop);
    fields->addWidget(CreateProgramRow(), 0, 1, Qt::AlignTop);
    fields->addWidget(choose, 0, 2, Qt::AlignTop);
    fields->addWidget(CreateWhatTheCheckSays(), 1, 1, 1, 2);
    fields->addWidget(FieldName(tr("Name"), this), 2, 0);
    fields->addWidget(name_, 2, 1, 1, 2);
    fields->addWidget(FieldName(tr("Command line"), this), 3, 0);
    fields->addWidget(commandLine_, 3, 1, 1, 2);

    auto* buttons = new QDialogButtonBox(this);
    confirm_ = buttons->addButton(editing_ ? tr("Save") : tr("Add"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    GiveItTheRole(confirm_, QStringLiteral("primary"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    layout->setSpacing(kSectionGap);
    layout->addLayout(fields);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(choose, &QPushButton::clicked, this, &StartupDraftDialog::ChooseTheProgram);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(name_, &QLineEdit::textEdited, this,
            [this]
            {
                nameFollowsTheFile_ = false;
            });
    connect(name_, &QLineEdit::textChanged, this, &StartupDraftDialog::AllowTheConfirmation);

    if (editing.has_value())
    {
        name_->setText(QString::fromStdString(editing->label));
        commandLine_->setText(QString::fromStdString(editing->commandLine));
        file_ = editing->file;
    }

    ShowWhatTheCheckSays();
}

void StartupDraftDialog::TakeTheProgram(const std::filesystem::path& file)
{
    file_ = file;

    if (nameFollowsTheFile_)
    {
        name_->setText(AsText(file.stem()));
    }

    ShowWhatTheCheckSays();
}

StartupDraft StartupDraftDialog::Draft() const
{
    return StartupDraft{.label = name_->text().trimmed().toStdString(),
                        .file = file_,
                        .commandLine = commandLine_->text().trimmed().toStdString()};
}

void StartupDraftDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    QTimer::singleShot(0, this, &StartupDraftDialog::Fit);
}

QWidget* StartupDraftDialog::CreateProgramRow()
{
    auto* row = new QWidget(this);

    programColumn_ = new QVBoxLayout(row);
    programColumn_->setContentsMargins(0, kTextLineInAButton, 0, 0);

    noProgram_ = new QLabel(tr("No program chosen"), row);
    noProgram_->setObjectName(QStringLiteral("PanelPromise"));
    programColumn_->addWidget(noProgram_);

    return row;
}

QWidget* StartupDraftDialog::CreateWhatTheCheckSays()
{
    auto* holder = new QWidget(this);

    checkColumn_ = new QVBoxLayout(holder);
    checkColumn_->setContentsMargins(0, 0, 0, kBelowTheLines);
    checkColumn_->setSpacing(kLinesGap);

    insideTheAddon_ = HiddenLine(QStringLiteral("PanelPromise"), holder);
    addonIsOff_ = HiddenLine(QStringLiteral("PanelPromise"), holder);
    refusal_ = HiddenLine(QStringLiteral("EntryRefusal"), holder);
    presetsFollow_ = HiddenLine(QStringLiteral("PanelPromise"), holder);
    goesToRemoved_ = HiddenLine(QStringLiteral("PanelPromise"), holder);

    checkColumn_->addWidget(insideTheAddon_);
    checkColumn_->addWidget(addonIsOff_);
    checkColumn_->addWidget(refusal_);
    checkColumn_->addWidget(presetsFollow_);
    checkColumn_->addWidget(goesToRemoved_);

    return holder;
}

void StartupDraftDialog::ChooseTheProgram()
{
    const QString chosen = QFileDialog::getOpenFileName(this, tr("Choose the program"), AsText(file_.parent_path()),
                                                        tr("Programs (*.exe)"));

    if (!chosen.isEmpty())
    {
        TakeTheProgram(AsPath(chosen));
    }
}

void StartupDraftDialog::ShowWhatTheCheckSays()
{
    const StartupDraftCheck check = file_.empty() ? StartupDraftCheck{} : check_(file_);

    refused_ = check.refusal != FileResult::Completed;

    const bool linked = !refused_ && check.insideAnAddon;
    const bool moved = !refused_ && editing_ && check.changesThePath;

    ShowTheProgram();

    insideTheAddon_->setText(tr("Inside the addon %1. The entry is written with the path of the addon's link:")
                                 .arg(QString::fromStdString(check.addonFolderName)));
    insideTheAddon_->setVisible(linked);
    ShowThePathWritten(linked ? AsText(check.pathToWrite) : QString());
    addonIsOff_->setText(tr("The addon is disabled now, so the program will not start until you enable it."));
    addonIsOff_->setVisible(linked && check.addonIsOff);
    ShowTheRefusal(check);
    presetsFollow_->setText(tr("%n preset names this entry and will follow the new path.", nullptr,
                               static_cast<int>(check.presetsNamingTheEntry)));
    presetsFollow_->setVisible(moved && check.presetsNamingTheEntry > 0);
    goesToRemoved_->setText(tr("The entry as it is now goes to Removed."));
    goesToRemoved_->setVisible(moved);

    AllowTheConfirmation();
    Fit();
}

void StartupDraftDialog::ShowTheProgram()
{
    noProgram_->setVisible(file_.empty());
    ReplaceTheText(program_, programColumn_, 1, file_.empty() ? QString() : AsText(file_));
}

void StartupDraftDialog::ShowTheRefusal(const StartupDraftCheck& check)
{
    refusal_->setVisible(refused_);

    switch (check.refusal)
    {
    case FileResult::TheStartupEntryIsAlreadyThere:
        refusal_->setText(
            tr("The startup file already lists this program, as %1.").arg(QString::fromStdString(check.occupiedBy)));
        break;
    case FileResult::TheProgramDoesNotExist: refusal_->setText(tr("That file does not exist.")); break;
    default: refusal_->setText(Explain(check.refusal)); break;
    }
}

void StartupDraftDialog::ShowThePathWritten(const QString& path)
{
    ReplaceTheText(pathWritten_, checkColumn_, kPathWrittenAt, path);
}

void StartupDraftDialog::AllowTheConfirmation()
{
    confirm_->setEnabled(!file_.empty() && !refused_ && !name_->text().trimmed().isEmpty());
}

void StartupDraftDialog::Fit()
{
    const std::array<const QLabel*, 5> lines{insideTheAddon_, addonIsOff_, refusal_, presetsFollow_, goesToRemoved_};
    const bool saysSomething = pathWritten_ != nullptr
        || std::ranges::any_of(lines,
                               [](const QLabel* line)
                               {
                                   return !line->isHidden();
                               });

    checkColumn_->parentWidget()->setVisible(saysSomething);

    for (int pass = 0; pass < 2; ++pass)
    {
        layout()->invalidate();
        layout()->activate();
    }

    SizeToTheContent(*this, kDialogWidth);
}
