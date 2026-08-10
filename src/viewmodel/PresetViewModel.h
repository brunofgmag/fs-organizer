#ifndef FS_ORGANIZER_VIEWMODEL_PRESET_VIEW_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_PRESET_VIEW_MODEL_H

#include <cstddef>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "application/PresetService.h"
#include "application/Session.h"

struct PresetRow
{
    QString name{};
    QString content{};
    QString updated{};
    std::size_t changes = 0;
    bool satisfied = false;
};

struct PresetPreview
{
    std::size_t toEnable = 0;
    std::size_t toDisable = 0;
    std::size_t alreadyInPlace = 0;
    std::size_t unresolved = 0;
    std::size_t leftAlone = 0;
    std::size_t notNamedByThePreset = 0;
    std::size_t startupAsked = 0;
    std::size_t startupToApply = 0;
    std::size_t startupUnresolved = 0;
    std::size_t notApplied = 0;
};

struct OmittedAddon
{
    QString name{};
    QString category{};
};

class PresetViewModel final : public QObject
{
    Q_OBJECT

public:
    PresetViewModel(Session& session, PresetService& service, ProfileService& profiles, QObject* parent = nullptr);

    [[nodiscard]] QStringList Names() const;

    [[nodiscard]] QList<PresetRow> Rows(ApplyMode mode) const;

    [[nodiscard]] std::optional<Preset> ReturnPreset() const;

    [[nodiscard]] std::optional<PresetRow> ReturnRow(ApplyMode mode) const;

    [[nodiscard]] std::optional<Preset> Load(const QString& name) const;

    [[nodiscard]] QString LibraryLabel(const std::string& libraryId) const;

    void Create(const QString& name);

    void Update(const QString& name);

    void Rename(const QString& from, const QString& to);

    void Remove(const QString& name);

    [[nodiscard]] bool SetAction(const QString& name, std::size_t index, const AddonId& expected, PresetAction action);

    [[nodiscard]] bool GovernStartup(const QString& name, bool governs);

    [[nodiscard]] PresetPreview Preview(const Preset& preset, ApplyMode mode) const;

    [[nodiscard]] QList<OmittedAddon> Omitted(const Preset& preset, ApplyMode mode) const;

    [[nodiscard]] bool CanUndo() const;

    void UndoLastBatch();

    void Apply(const Preset& preset, ApplyMode mode);

signals:
    void Changed();

    void Refused(const QString& explanation);

    void Applied(const QStringList& unresolved, const QString& whatTheStartupHalfLeftUndone);

private:
    void RefuseTheWriteOf(const QString& name);

    [[nodiscard]] bool Accepts(const QString& name);

    [[nodiscard]] PresetRow RowFor(const Preset& preset, const PresetListing& listing, ApplyMode mode) const;

    [[nodiscard]] static QString WhatTheStartupHalfLeftUndone(const PresetApplyReport& report);

    Session& session_;
    PresetService& service_;
    ProfileService& profiles_;
};

#endif // FS_ORGANIZER_VIEWMODEL_PRESET_VIEW_MODEL_H
