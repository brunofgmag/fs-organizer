#ifndef FS_ORGANIZER_VIEWMODEL_PRESET_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_PRESET_VIEW_MODEL_H

#include <cstddef>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "application/PresetService.h"
#include "application/Session.h"

struct PresetPreview
{
    std::size_t toEnable = 0;
    std::size_t toDisable = 0;
    std::size_t alreadyInPlace = 0;
    std::size_t unresolved = 0;
    std::size_t leftAlone = 0;
};

class PresetViewModel final : public QObject
{
    Q_OBJECT

public:
    PresetViewModel(Session& session, PresetService& service, QObject* parent = nullptr);

    [[nodiscard]] QStringList Names() const;

    [[nodiscard]] std::optional<Preset> Load(const QString& name) const;

    [[nodiscard]] QString LibraryLabel(const std::string& libraryId) const;

    void Create(const QString& name);

    void Update(const QString& name);

    void Rename(const QString& from, const QString& to);

    void Remove(const QString& name);

    [[nodiscard]] bool SetAction(const QString& name, std::size_t index, const AddonId& expected, PresetAction action);

    [[nodiscard]] PresetPreview Preview(const Preset& preset, ApplyMode mode) const;

    void Apply(const Preset& preset, ApplyMode mode);

signals:
    void Changed();

    void Refused(const QString& explanation);

    void Applied(const QStringList& unresolved);

private:
    void RefuseTheWriteOf(const QString& name);

    [[nodiscard]] bool Accepts(const QString& name);

    Session& session_;
    PresetService& service_;
};

#endif // FS_ORGANIZER_VIEWMODEL_PRESET_VIEW_MODEL_H
