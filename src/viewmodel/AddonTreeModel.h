#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_MODEL_H

#include <memory>
#include <vector>

#include <QtCore/QAbstractItemModel>

#include "application/model/ProfileSnapshot.h"
#include "domain/model/SimulatorProfile.h"

Q_DECLARE_METATYPE(CopyConflict)

class AddonTreeModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    void Retranslated()
    {
        emit layoutChanged();
    }

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

    void Refresh(const ProfileSnapshot& snapshot, const SimulatorProfile& profile);

    [[nodiscard]] static const TreeNode* NodeAt(const QModelIndex& position);

    [[nodiscard]] std::size_t AddonCount() const;

    [[nodiscard]] std::size_t EnabledCount() const;

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
    struct Item
    {
        const TreeNode* node = nullptr;
        Item* parent = nullptr;
        int row = 0;
        std::vector<Item*> children;
    };

    void Rebuild();

    Item* AddItem(const TreeNode& node, Item* parent);

    void AnnounceValues(const QModelIndex& parent);

    [[nodiscard]] QString NameOf(const TreeNode& node) const;

    [[nodiscard]] static QString CountedSuffixOf(const TreeNode& node);

    [[nodiscard]] QString DisplayTextOf(const TreeNode& node, int column) const;

    [[nodiscard]] QString ToolTipOf(const TreeNode& node, const CopyConflict* conflict) const;

    [[nodiscard]] std::filesystem::path WhereItIsLinked(const TreeNode& node) const;

    [[nodiscard]] bool WandersFromTheDefault(const TreeNode& node) const;

    [[nodiscard]] bool LinksNowhere(const TreeNode& node) const;

    [[nodiscard]] const std::vector<Item*>& ChildrenOf(const QModelIndex& parent) const;

    std::vector<TreeNode> libraries_;
    std::vector<DestinationEntry> entries_;
    EnabledAddons enabled_;
    CopyConflicts conflicts_;
    SimulatorProfile profile_;
    std::vector<std::unique_ptr<Item>> items_;
    std::vector<Item*> roots_;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_MODEL_H
