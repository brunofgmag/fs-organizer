#include "viewmodel/PresetViewModel.h"

#include <algorithm>

#include "domain/support/PathSegment.h"
#include "domain/support/PathUtils.h"
#include "domain/support/StringUtils.h"
#include "support/MomentText.h"
#include "support/PathText.h"

PresetViewModel::PresetViewModel(Session& session, PresetService& service, ProfileService& profiles, QObject* parent)
    : QObject(parent), session_(session), service_(service), profiles_(profiles)
{
}

QStringList PresetViewModel::Names() const
{
    QStringList names;

    for (const PresetListing& listing : service_.List(session_.Profile().id))
    {
        names.append(QString::fromStdString(listing.name));
    }

    return names;
}

PresetRow PresetViewModel::RowFor(const Preset& preset, const PresetListing& listing, const ApplyMode mode) const
{
    const SimulatorProfile& profile = session_.Profile();
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const PresetContent content = ContentOf(preset, profile, snapshot.libraries);
    const PresetPlan plan = PlanPresetApplication(preset, mode, profile, snapshot.libraries, snapshot.enabled);

    return {.name = QString::fromStdString(listing.name),
            .content = tr("%n addon", nullptr, static_cast<int>(content.addons))
                + tr(" · %n category", nullptr, static_cast<int>(content.categories)),
            .updated = listing.writtenAt.has_value() ? AsDay(*listing.writtenAt) : QString{},
            .changes = AddonsThatWouldChange(plan),
            .satisfied = service_.IsSatisfied(profile, snapshot, preset)};
}

QList<PresetRow> PresetViewModel::Rows(const ApplyMode mode) const
{
    const SimulatorProfile& profile = session_.Profile();
    QList<PresetRow> rows;

    for (const PresetListing& listing : service_.List(profile.id))
    {
        const std::optional<Preset> preset = service_.Load(profile.id, listing.name);

        rows.append(RowFor(preset.value_or(Preset{}), listing, mode));
    }

    return rows;
}

std::optional<Preset> PresetViewModel::ReturnPreset() const
{
    return service_.ReturnPreset(session_.Profile().id);
}

std::optional<PresetRow> PresetViewModel::ReturnRow(const ApplyMode mode) const
{
    const std::optional<Preset> preset = ReturnPreset();

    if (!preset.has_value())
    {
        return std::nullopt;
    }

    return RowFor(*preset, PresetListing{}, mode);
}

std::optional<Preset> PresetViewModel::Load(const QString& name) const
{
    return service_.Load(session_.Profile().id, name.toStdString());
}

QString PresetViewModel::LibraryLabel(const std::string& libraryId) const
{
    for (const Library& library : session_.Profile().libraries)
    {
        if (EqualsIgnoringCase(library.id, libraryId))
        {
            return QString::fromStdString(library.label);
        }
    }

    return QString::fromStdString(libraryId);
}

void PresetViewModel::Create(const QString& name)
{
    if (!Accepts(name))
    {
        return;
    }

    const QString wanted = name.trimmed();

    if (!service_.Create(session_.Profile(), session_.Snapshot(), wanted.toStdString()))
    {
        RefuseTheWriteOf(wanted);
    }

    emit Changed();
}

void PresetViewModel::Update(const QString& name)
{
    if (!service_.Update(session_.Profile(), session_.Snapshot(), name.toStdString()))
    {
        RefuseTheWriteOf(name);
    }

    emit Changed();
}

void PresetViewModel::Rename(const QString& from, const QString& to)
{
    if (from == to.trimmed())
    {
        return;
    }

    if (!Accepts(to))
    {
        return;
    }

    const QString wanted = to.trimmed();

    if (!service_.Rename(session_.Profile().id, from.toStdString(), wanted.toStdString()))
    {
        RefuseTheWriteOf(wanted);
    }

    emit Changed();
}

void PresetViewModel::Remove(const QString& name)
{
    service_.Remove(session_.Profile().id, name.toStdString());

    emit Changed();
}

bool PresetViewModel::SetAction(const QString& name,
                                const std::size_t index,
                                const AddonId& expected,
                                const PresetAction action)
{
    if (service_.SetAction(session_.Profile().id, name.toStdString(), index, expected, action))
    {
        return true;
    }

    emit Refused(tr("The change could not be written to the preset \"%1\". It may have changed since the table was "
                    "built, or the presets folder may be full or protected.")
                     .arg(name));

    return false;
}

bool PresetViewModel::SetStartupAction(const QString& name,
                                       const std::size_t index,
                                       const std::filesystem::path& expected,
                                       const PresetAction action)
{
    if (service_.SetStartupAction(session_.Profile().id, name.toStdString(), index, expected, action))
    {
        return true;
    }

    emit Refused(tr("The change could not be written to the preset \"%1\". It may have changed since the table was "
                    "built, or the presets folder may be full or protected.")
                     .arg(name));

    return false;
}

QList<PresetStartupRow> PresetViewModel::StartupRows(const Preset& preset) const
{
    const std::vector<StartupEntry>& lines = session_.Snapshot().startupEntries;

    QList<PresetStartupRow> rows;

    for (const PresetStartupEntry& entry : preset.startupEntries)
    {
        QString label;

        for (const StartupEntry& line : lines)
        {
            if (ComparablePath(line.path) == ComparablePath(entry.path))
            {
                label = QString::fromStdString(line.label);
                break;
            }
        }

        rows.append(PresetStartupRow{.label = label.isEmpty() ? AsText(entry.path.stem()) : label,
                                     .target = AsText(entry.path),
                                     .path = entry.path,
                                     .action = entry.action});
    }

    return rows;
}

bool PresetViewModel::GovernStartup(const QString& name, const bool governs)
{
    if (service_.GovernStartup(session_.Profile(), session_.Snapshot(), name.toStdString(), governs))
    {
        return true;
    }

    RefuseTheWriteOf(name);

    return false;
}

void PresetViewModel::RecaptureStartup(const QString& name)
{
    if (!service_.RecaptureStartup(session_.Profile(), session_.Snapshot(), name.toStdString()))
    {
        RefuseTheWriteOf(name);
    }

    emit Changed();
}

PresetPreview PresetViewModel::Preview(const Preset& preset, const ApplyMode mode) const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const PresetApplyPlan plan = service_.Plan(session_.Profile(), snapshot, preset, mode);

    const auto leftAlone = std::ranges::count_if(snapshot.entries,
                                                 [](const DestinationEntry& entry)
                                                 {
                                                     return !CountsAsEnabled(entry.classification);
                                                 });

    return {.toEnable = plan.addons.toEnable.size(),
            .toDisable = plan.addons.toDisable.size(),
            .alreadyInPlace = plan.addons.alreadyInPlace.size(),
            .unresolved = plan.addons.unresolved.size(),
            .leftAlone = static_cast<std::size_t>(leftAlone),
            .notNamedByThePreset = plan.addons.notNamedByThePreset.size(),
            .startupAsked = plan.startup.asked,
            .startupToApply = plan.startup.toTurnOn.size() + plan.startup.toTurnOff.size(),
            .startupUnresolved = plan.startup.unresolved.size(),
            .notApplied = plan.startup.notApplied};
}

QList<OmittedAddon> PresetViewModel::Omitted(const Preset& preset, const ApplyMode mode) const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const PresetPlan plan =
        PlanPresetApplication(preset, mode, session_.Profile(), snapshot.libraries, snapshot.enabled);

    QList<OmittedAddon> omitted;

    for (const TreeNode* addon : plan.notNamedByThePreset)
    {
        omitted.append(OmittedAddon{.name = AsText(addon->path.filename()),
                                    .category = AsText(addon->path.parent_path().filename())});
    }

    return omitted;
}

bool PresetViewModel::CanUndo() const
{
    return profiles_.CanUndo();
}

void PresetViewModel::UndoLastBatch()
{
    const std::vector<LinkOperationResult> results = profiles_.UndoLastBatch();

    session_.RefreshEntries();

    session_.NoteLinkResults(results);

    emit Changed();
}

void PresetViewModel::Apply(const Preset& preset, const ApplyMode mode)
{
    NoteApplied(service_.Apply(session_.Profile(), session_.Snapshot(), preset, mode));
}

void PresetViewModel::ApplyReturn(const Preset& preset)
{
    NoteApplied(service_.ApplyTheReturn(session_.Profile(), session_.Snapshot(), preset));
}

void PresetViewModel::NoteApplied(const PresetApplyReport& report)
{
    if (report.refusal == PresetApplyRefusal::TheReturnPresetCouldNotBeWritten)
    {
        emit Refused(tr("Nothing was applied. The app writes down what is enabled right now before applying a preset, "
                        "so that you can come back to it, and this time it could not: the presets folder may be full "
                        "or protected."));
        return;
    }

    session_.RefreshEntries();

    session_.NoteLinkResults(report.results);

    QStringList unresolved;
    for (const AddonId& addonId : report.unresolved)
    {
        unresolved.append(QString::fromStdString(addonId.folderName));
    }

    emit Applied(unresolved, WhatTheStartupHalfLeftUndone(report));
}

QString PresetViewModel::WhatTheStartupHalfLeftUndone(const PresetApplyReport& report)
{
    if (report.startupNotApplied > 0)
    {
        return tr("%n startup entry the preset asks for was not applied, because startup management is off in "
                  "Options.",
                  nullptr, static_cast<int>(report.startupNotApplied));
    }

    if (report.startupUnresolved.empty())
    {
        return {};
    }

    QStringList missing;
    for (const std::filesystem::path& path : report.startupUnresolved)
    {
        missing.append(AsText(path));
    }

    return tr("These startup entries of the preset are no longer in the simulator file:\n\n%1")
        .arg(missing.join(QStringLiteral("\n")));
}

void PresetViewModel::RefuseTheWriteOf(const QString& name)
{
    emit Refused(tr("The preset \"%1\" could not be written. The name may be too long for the disk, or the presets "
                    "folder may be full or protected.")
                     .arg(name));
}

bool PresetViewModel::Accepts(const QString& name)
{
    const QString wanted = name.trimmed();

    if (wanted.isEmpty())
    {
        emit Refused(tr("Give the preset a name."));
        return false;
    }

    if (!PathSegment::From(wanted.toStdString()).has_value())
    {
        emit Refused(tr("The preset name cannot contain %1, and cannot end with a space or a full stop.")
                         .arg(QStringLiteral(R"(<>:"/\|?*)")));
        return false;
    }

    if (Names().contains(wanted, Qt::CaseInsensitive))
    {
        emit Refused(tr("There is already a preset with that name."));
        return false;
    }

    return true;
}
