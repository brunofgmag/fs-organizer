#include "view/presets/PresetPlanPanel.h"

#include <QtCore/QEvent>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QVBoxLayout>

#include "view/theme/ModernistMetrics.h"

namespace
{
    constexpr int kBetweenTheTwoColumnsOfCounts = 44;

    QLabel* FieldName(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(QStringLiteral("DetailFieldName"));
        label->setWordWrap(true);

        return label;
    }

    QLabel* LoudFieldName(QWidget* parent)
    {
        QLabel* label = FieldName(parent);
        label->setObjectName(QStringLiteral("PresetPlanFor"));

        return label;
    }

    QLabel* FieldValue(const QString& name, QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setObjectName(name);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        return label;
    }

    void Say(QLabel* label, const std::size_t count)
    {
        label->setText(QString::number(count));
    }

    void ShowBoth(QLabel* name, QLabel* value, const bool showing)
    {
        name->setVisible(showing);
        value->setVisible(showing);
    }

    QString TheStartupSentenceOf(const PresetPreview& preview)
    {
        if (preview.notApplied > 0)
        {
            return QObject::tr("This preset asks for %n startup entry, and none will be applied, because startup "
                               "management is off in Options.",
                               nullptr, static_cast<int>(preview.notApplied));
        }

        if (preview.startupUnresolved > 0)
        {
            return QObject::tr("This preset asks for %1 startup entries. %2 of them are no longer in the simulator "
                               "file, and %3 will be switched.")
                .arg(preview.startupAsked)
                .arg(preview.startupUnresolved)
                .arg(preview.startupToApply);
        }

        return QObject::tr("This preset asks for %n startup entry, and all of them will be applied.", nullptr,
                           static_cast<int>(preview.startupAsked));
    }
}

QString CountsSentenceFor(const PresetPreview& preview)
{
    QString counted = QObject::tr("Enables %1, disables %2. %3 already match what the preset asks, %4 were not "
                                  "found, and %5 destination entries stay as they are.")
                          .arg(preview.toEnable)
                          .arg(preview.toDisable)
                          .arg(preview.alreadyInPlace)
                          .arg(preview.unresolved)
                          .arg(preview.leftAlone);

    if (preview.notNamedByThePreset > 0)
    {
        counted += QObject::tr(" Of the ones it disables, %1 entered the library after the preset was saved.")
                       .arg(preview.notNamedByThePreset);
    }

    return counted;
}

PresetPlanPanel::PresetPlanPanel(QWidget* parent) : QWidget(parent)
{
    planFor_ = new QLabel(this);
    planFor_->setObjectName(QStringLiteral("PresetPlanFor"));

    planTitle_ = new QLabel(this);
    planTitle_->setObjectName(QStringLiteral("PanelSubHeading"));

    omittedNote_ = new QLabel(this);
    omittedNote_->setObjectName(QStringLiteral("PanelPromise"));
    omittedNote_->setWordWrap(true);

    showOmitted_ = new QPushButton(this);
    showOmitted_->setObjectName(QStringLiteral("PresetShowOmitted"));

    auto* omittedRow = new QHBoxLayout;
    omittedRow->setContentsMargins(0, 0, 0, 0);
    omittedRow->setSpacing(12);
    omittedRow->addWidget(omittedNote_, 1);
    omittedRow->addWidget(showOmitted_);

    apply_ = new QPushButton(this);
    apply_->setObjectName(QStringLiteral("PresetApply"));
    apply_->setProperty("role", "primary");
    apply_->setDefault(true);

    auto* applyRow = new QHBoxLayout;
    applyRow->setContentsMargins(0, 0, 0, 0);
    applyRow->addWidget(apply_);
    applyRow->addStretch();

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(kPageGutter, kPageGutter, kPageGutter, kPageGutter);
    column->setSpacing(12);
    column->addWidget(planFor_);
    column->addWidget(CreateTheModeRow());
    column->addWidget(planTitle_);
    column->addWidget(CreateThePlanFields());
    column->addLayout(omittedRow);
    column->addWidget(CreateTheStartupSection());
    column->addSpacing(6);
    column->addLayout(applyRow);
    column->addStretch();

    connect(apply_, &QPushButton::clicked, this, &PresetPlanPanel::ApplyRequested);
    connect(showOmitted_, &QPushButton::clicked, this, &PresetPlanPanel::OmittedRequested);
    connect(modes_, &QButtonGroup::idClicked, this,
            [this]
            {
                emit ModeChanged();
            });

    RetranslateUi();
}

ApplyMode PresetPlanPanel::Mode() const
{
    return static_cast<ApplyMode>(modes_->checkedId());
}

QWidget* PresetPlanPanel::CreateTheModeRow()
{
    auto* row = new QWidget(this);

    applyAs_ = new QLabel(row);
    applyAs_->setObjectName(QStringLiteral("DetailFieldName"));

    modes_ = new QButtonGroup(row);
    auto* replace = new QRadioButton(row);
    replace->setObjectName(QStringLiteral("ModeReplace"));
    auto* cumulative = new QRadioButton(row);
    cumulative->setObjectName(QStringLiteral("ModeCumulative"));
    auto* disable = new QRadioButton(row);
    disable->setObjectName(QStringLiteral("ModeDisable"));
    modes_->addButton(replace, static_cast<int>(ApplyMode::Replace));
    modes_->addButton(cumulative, static_cast<int>(ApplyMode::Cumulative));
    modes_->addButton(disable, static_cast<int>(ApplyMode::Disable));
    replace->setChecked(true);

    modeExplained_ = new QLabel(row);
    modeExplained_->setObjectName(QStringLiteral("ModeExplained"));

    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(14);
    line->addWidget(applyAs_);
    line->addWidget(replace);
    line->addWidget(cumulative);
    line->addWidget(disable);
    line->addSpacing(6);
    line->addWidget(modeExplained_);
    line->addStretch();

    return row;
}

QWidget* PresetPlanPanel::CreateThePlanFields()
{
    auto* fields = new QWidget(this);

    auto* grid = new QGridLayout(fields);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(2, kBetweenTheTwoColumnsOfCounts);
    grid->setColumnStretch(3, 1);

    toEnableName_ = LoudFieldName(fields);
    toEnable_ = FieldValue(QStringLiteral("PlanToEnable"), fields);
    toDisableName_ = LoudFieldName(fields);
    toDisable_ = FieldValue(QStringLiteral("PlanToDisable"), fields);
    alreadyName_ = FieldName(fields);
    already_ = FieldValue(QStringLiteral("PlanAlreadyInPlace"), fields);
    unresolvedName_ = FieldName(fields);
    unresolved_ = FieldValue(QStringLiteral("PlanUnresolved"), fields);
    notNamedName_ = FieldName(fields);
    notNamed_ = FieldValue(QStringLiteral("PlanNotNamed"), fields);
    notAppliedName_ = FieldName(fields);
    notApplied_ = FieldValue(QStringLiteral("PlanNotApplied"), fields);

    QLabel* const names[] = {toEnableName_,   toDisableName_, alreadyName_,
                             unresolvedName_, notNamedName_,  notAppliedName_};
    QLabel* const values[] = {toEnable_, toDisable_, already_, unresolved_, notNamed_, notApplied_};

    for (int field = 0; field < 6; ++field)
    {
        const int column = field < 3 ? 0 : 3;

        grid->addWidget(names[field], field % 3, column);
        grid->addWidget(values[field], field % 3, column + 1);
    }

    return fields;
}

QWidget* PresetPlanPanel::CreateTheStartupSection()
{
    startupSection_ = new QWidget(this);
    startupSection_->setObjectName(QStringLiteral("PresetStartupSection"));

    startupSaid_ = new QLabel(startupSection_);
    startupSaid_->setObjectName(QStringLiteral("PanelPromise"));
    startupSaid_->setWordWrap(true);

    auto* inside = new QVBoxLayout(startupSection_);
    inside->setContentsMargins(0, 0, 0, 0);
    inside->addWidget(startupSaid_);

    startupSection_->hide();

    return startupSection_;
}

void PresetPlanPanel::Show(const PresetPlanState& state)
{
    const PresetPreview& preview = state.preview;

    planFor_->setText(state.planFor);

    switch (Mode())
    {
    case ApplyMode::Replace:
        modeExplained_->setText(tr("Leaves only what the preset enables."));
        planTitle_->setText(tr("The plan, as Replace"));
        break;
    case ApplyMode::Cumulative:
        modeExplained_->setText(tr("Enables what the preset names, without touching the rest."));
        planTitle_->setText(tr("The plan, as Accumulate"));
        break;
    case ApplyMode::Disable:
        modeExplained_->setText(tr("Disables what the preset enables."));
        planTitle_->setText(tr("The plan, as Disable"));
        break;
    }

    apply_->setEnabled(state.holdsOne);
    startupSection_->setVisible(state.governsStartup);

    if (!state.holdsOne)
    {
        apply_->setText(tr("Apply"));
        apply_->setToolTip({});
    }
    else
    {
        apply_->setToolTip(CountsSentenceFor(preview));
        apply_->setText(tr("Apply: enables %1, disables %2").arg(preview.toEnable).arg(preview.toDisable));
    }

    Say(toEnable_, preview.toEnable);
    Say(toDisable_, preview.toDisable);
    Say(already_, preview.alreadyInPlace);
    Say(unresolved_, preview.unresolved);
    Say(notNamed_, preview.notNamedByThePreset);
    Say(notApplied_, preview.notApplied);

    const bool replacing = Mode() == ApplyMode::Replace;

    ShowBoth(notNamedName_, notNamed_, replacing);
    showOmitted_->setVisible(replacing);
    showOmitted_->setEnabled(preview.notNamedByThePreset > 0);
    omittedNote_->setVisible(replacing && preview.notNamedByThePreset > 0);
    omittedNote_->setText(tr("The %1 omitted are part of the %2 being turned off, and not a pile on top of them.")
                              .arg(preview.notNamedByThePreset)
                              .arg(preview.toDisable));

    startupSaid_->setText(TheStartupSentenceOf(preview));
}

void PresetPlanPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        RetranslateUi();
    }

    QWidget::changeEvent(event);
}

void PresetPlanPanel::RetranslateUi()
{
    applyAs_->setText(tr("Apply as"));
    modes_->button(static_cast<int>(ApplyMode::Replace))->setText(tr("Replace"));
    modes_->button(static_cast<int>(ApplyMode::Cumulative))->setText(tr("Accumulate"));
    modes_->button(static_cast<int>(ApplyMode::Disable))->setText(tr("Disable"));
    toEnableName_->setText(tr("Turn on"));
    toDisableName_->setText(tr("Turn off"));
    alreadyName_->setText(tr("Already as the preset asks"));
    unresolvedName_->setText(tr("Named, but no addon found"));
    notNamedName_->setText(tr("Off because Replace omits them"));
    notAppliedName_->setText(tr("Asked for, but not applied"));
    showOmitted_->setText(tr("Show them…"));
}
