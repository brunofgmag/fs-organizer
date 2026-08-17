#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_MODEL_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QtCore/QAbstractItemModel>

#include "application/model/ProfileSnapshot.h"
#include "domain/model/SimulatorProfile.h"
#include "domain/tree/AddonDestinations.h"

Q_DECLARE_METATYPE(CopyConflict)

struct SelectionTally
{
    std::vector<std::filesystem::path> addons{};
    std::size_t categories = 0;
    std::size_t enabled = 0;
    std::size_t broken = 0;
    std::size_t strayed = 0;
    std::size_t categoriesCrossed = 0;
    bool alarming = false;
};

class AddonTreeModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column
    {
        AddonColumn = 0,
        VersionColumn = 1,
        DestinationColumn = 2,
        Columns = 3,
    };

    enum Role
    {
        ConflictRole = Qt::UserRole,
        ConflictDetailsRole,
        EnabledRole,
        DivergentRole,
        BrokenRole,
    };

    explicit AddonTreeModel(QObject* parent = nullptr);

    void Show(const ProfileSnapshot& snapshot, const SimulatorProfile& profile);

    void Retranslate();

    void Refresh(const ProfileSnapshot& snapshot, const SimulatorProfile& profile);

    [[nodiscard]] static const TreeNode* NodeAt(const QModelIndex& position);

    [[nodiscard]] std::size_t AddonCount() const;

    [[nodiscard]] std::size_t EnabledCount() const;

    [[nodiscard]] SelectionTally TallyOf(const std::vector<const TreeNode*>& nodes) const;

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;

    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    bool setData(const QModelIndex& position, const QVariant& value, int role) override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& position) const override;

signals:
    void ToggleRequested(const TreeNode* node);

private:
    struct Reading
    {
        QString name{};
        const CopyConflict* conflict = nullptr;
        std::filesystem::path destination{};
        std::filesystem::path strayedTo{};
        std::size_t addons = 0;
        std::size_t categories = 0;
        Qt::CheckState checked = Qt::Unchecked;
        bool enabled = false;
        bool broken = false;
        bool pinned = false;
    };

    struct Item
    {
        const TreeNode* node = nullptr;
        Item* parent = nullptr;
        int row = 0;
        std::vector<Item*> children;
        Reading reading{};
    };

    void Rebuild();

    Item* AddItem(const TreeNode& node, Item* parent);

    void ReadEveryRow();

    [[nodiscard]] Reading ReadingOf(const TreeNode& node) const;

    void AnnounceValues(const QModelIndex& parent);

    [[nodiscard]] static const Item* ItemAt(const QModelIndex& position);

    [[nodiscard]] QString NameOf(const TreeNode& node) const;

    [[nodiscard]] static QString CountedSuffixOf(const TreeNode& node, const Reading& reading);

    [[nodiscard]] QString DisplayTextOf(const TreeNode& node, const Reading& reading, int column) const;

    [[nodiscard]] QString ToolTipOf(const Reading& reading) const;

    [[nodiscard]] const std::vector<Item*>& ChildrenOf(const QModelIndex& parent) const;

    std::vector<TreeNode> libraries_;
    std::vector<DestinationEntry> entries_;
    EnabledAddons enabled_;
    CopyConflicts conflicts_;
    SimulatorProfile profile_;
    std::optional<AddonDestinations> destinations_;
    std::vector<std::unique_ptr<Item>> items_;
    std::vector<Item*> roots_;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_MODEL_H
