#include "viewmodel/PresetViewModel.h"

#include <algorithm>

#include "domain/support/PathSegment.h"
#include "domain/support/StringUtils.h"
#include "support/MomentText.h"

PresetViewModel::PresetViewModel(Session& session, PresetService& service, QObject* parent)
    : QObject(parent), session_(session), service_(service)
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

QList<PresetRow> PresetViewModel::Rows() const
{
    const SimulatorProfile& profile = session_.Profile();
    QList<PresetRow> rows;

    for (const PresetListing& listing : service_.List(profile.id))
    {
        const std::optional<Preset> preset = service_.Load(profile.id, listing.name);
        const PresetContent content =
            preset.has_value() ? ContentOf(*preset, profile, session_.Snapshot().libraries) : PresetContent{};

        rows.append(PresetRow{QString::fromStdString(listing.name),
                              tr("%n addon(s)", nullptr, static_cast<int>(content.addons))
                                  + tr(" · %n categoria(s)", nullptr, static_cast<int>(content.categories)),
                              listing.writtenAt.has_value() ? AsDay(*listing.writtenAt) : QString{}});
    }

    return rows;
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

    emit Refused(tr("Não deu para gravar a mudança no preset \"%1\". Ele pode ter mudado desde que a tabela "
                    "foi montada, ou a pasta dos presets pode estar cheia ou protegida.")
                     .arg(name));

    return false;
}

PresetPreview PresetViewModel::Preview(const Preset& preset, const ApplyMode mode) const
{
    const ProfileSnapshot& snapshot = session_.Snapshot();
    const PresetPlan plan =
        PlanPresetApplication(preset, mode, session_.Profile(), snapshot.libraries, snapshot.enabled);

    const auto leftAlone = std::ranges::count_if(snapshot.entries,
                                                 [](const DestinationEntry& entry)
                                                 {
                                                     return !CountsAsEnabled(entry.classification);
                                                 });

    return {plan.toEnable.size(),
            plan.toDisable.size(),
            plan.alreadyInPlace.size(),
            plan.unresolved.size(),
            static_cast<std::size_t>(leftAlone),
            plan.notNamedByThePreset.size()};
}

void PresetViewModel::Apply(const Preset& preset, const ApplyMode mode)
{
    const PresetApplyReport report = service_.Apply(session_.Profile(), session_.Snapshot(), preset, mode);

    session_.RefreshEntries();

    session_.NoteLinkResults(report.results);

    QStringList unresolved;
    for (const AddonId& addonId : report.unresolved)
    {
        unresolved.append(QString::fromStdString(addonId.folderName));
    }

    emit Applied(unresolved);
}

void PresetViewModel::RefuseTheWriteOf(const QString& name)
{
    emit Refused(tr("Não deu para gravar o preset \"%1\". O nome pode ser longo demais para o disco, "
                    "ou a pasta dos presets pode estar cheia ou protegida.")
                     .arg(name));
}

bool PresetViewModel::Accepts(const QString& name)
{
    const QString wanted = name.trimmed();

    if (wanted.isEmpty())
    {
        emit Refused(tr("Dê um nome ao preset."));
        return false;
    }

    if (!PathSegment::From(wanted.toStdString()).has_value())
    {
        emit Refused(tr("O nome do preset não pode conter %1, nem terminar com espaço ou ponto.")
                         .arg(QStringLiteral(R"(<>:"/\|?*)")));
        return false;
    }

    if (Names().contains(wanted, Qt::CaseInsensitive))
    {
        emit Refused(tr("Já existe um preset com esse nome."));
        return false;
    }

    return true;
}
