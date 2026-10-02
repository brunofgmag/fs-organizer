#include "view/panels/FoldersOutsideNotice.h"

#include <QtCore/QEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include "view/theme/ModernistMetrics.h"

FoldersOutsideNotice::FoldersOutsideNotice(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("FoldersOutsideTheLibrary"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    said_ = new QLabel(this);
    said_->setObjectName(QStringLiteral("TriageQuiet"));
    said_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    import_ = new QPushButton(this);
    import_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    import_->setCursor(Qt::PointingHandCursor);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kToolbarGap);
    row->addWidget(said_);
    row->addWidget(import_);

    connect(import_, &QPushButton::clicked, this, &FoldersOutsideNotice::ImportRequested);

    RetranslateUi();
    setVisible(false);
}

void FoldersOutsideNotice::ShowFolders(const std::size_t folders)
{
    folders_ = folders;
    RetranslateUi();
    setVisible(folders_ > 0);
}

void FoldersOutsideNotice::RetranslateUi()
{
    said_->setText(tr("%n folder outside the library", nullptr, static_cast<int>(folders_)));
    import_->setText(tr("Import into the library…"));
}

void FoldersOutsideNotice::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}
