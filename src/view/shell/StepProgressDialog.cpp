#include "view/shell/StepProgressDialog.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>

#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kFinest = 1000;

    [[nodiscard]] QLabel* AQuietLine(QWidget* on)
    {
        auto* line = new QLabel(on);
        line->setObjectName(QStringLiteral("ImportQuiet"));
        line->setFixedWidth(kReadableWidth);

        return line;
    }

    void GiveItTheStage(QWidget& widget, const char* stage)
    {
        widget.setProperty("stage", QString::fromLatin1(stage));
        widget.style()->unpolish(&widget);
        widget.style()->polish(&widget);
    }
}

StepProgressDialog::StepProgressDialog(const QString& title, const QStringList& steps, QWidget* over) : QDialog(over)
{
    setWindowTitle(title);
    setWindowModality(Qt::ApplicationModal);
    setSizeGripEnabled(false);

    auto* column = new QVBoxLayout(this);

    subject_ = AQuietLine(this);
    column->addWidget(subject_);

    for (const QString& step : steps)
    {
        auto* name = new QLabel(step, this);
        name->setObjectName(QStringLiteral("StepName"));

        auto* bar = new QProgressBar(this);
        bar->setObjectName(QStringLiteral("ImportMeter"));
        bar->setRange(0, kFinest);
        bar->setValue(0);
        bar->setTextVisible(false);
        bar->setFixedHeight(kMeterHeight);

        column->addWidget(name);
        column->addWidget(bar);

        names_.push_back(name);
        bars_.push_back(bar);
    }

    detail_ = AQuietLine(this);

    cancel_ = new QPushButton(tr("Cancel"), this);
    cancel_->setObjectName(QStringLiteral("CancelTheSteps"));
    cancel_->setAutoDefault(false);
    cancel_->setDefault(false);

    column->addWidget(detail_);
    column->addWidget(cancel_, 0, Qt::AlignRight);
    column->setSizeConstraint(QLayout::SetFixedSize);

    connect(cancel_, &QPushButton::clicked, this, &StepProgressDialog::Cancelled);

    ShowTheStep(0);
}

void StepProgressDialog::ShowCutInTheMiddle(QLabel& line, const QString& said) const
{
    const QString fitted = line.fontMetrics().elidedText(said, Qt::ElideMiddle, kReadableWidth);

    line.setText(fitted);
    line.setToolTip(fitted == said ? QString() : said);
}

void StepProgressDialog::NameTheSteps(const QStringList& steps)
{
    for (std::size_t at = 0; at < names_.size() && at < static_cast<std::size_t>(steps.size()); ++at)
    {
        names_[at]->setText(steps[static_cast<qsizetype>(at)]);
    }
}

void StepProgressDialog::ShowTheSubject(const QString& subject)
{
    ShowCutInTheMiddle(*subject_, subject);
}

void StepProgressDialog::ShowTheStep(const int step)
{
    step_ = step;

    for (std::size_t at = 0; at < names_.size(); ++at)
    {
        const auto index = static_cast<int>(at);

        bars_[at]->setRange(0, kFinest);
        bars_[at]->setValue(index < step ? kFinest : 0);

        if (index == step)
        {
            GiveItTheStage(*names_[at], "now");
        }
        else
        {
            GiveItTheStage(*names_[at], index < step ? "done" : "next");
        }
    }
}

void StepProgressDialog::ShowTheProgress(const qulonglong done, const qulonglong total)
{
    QProgressBar* bar = bars_[static_cast<std::size_t>(step_)];

    bar->setRange(0, kFinest);
    bar->setValue(total == 0 ? 0 : static_cast<int>(done * kFinest / total));
}

void StepProgressDialog::ShowThatItCannotBeMeasured()
{
    bars_[static_cast<std::size_t>(step_)]->setRange(0, 0);
}

void StepProgressDialog::ShowTheDetail(const QString& detail)
{
    ShowCutInTheMiddle(*detail_, detail);
}

void StepProgressDialog::ShowItIsCancelling(const QString& detail)
{
    cancel_->setEnabled(false);
    cancel_->setText(tr("Cancelling…"));

    ShowCutInTheMiddle(*detail_, detail);
}
