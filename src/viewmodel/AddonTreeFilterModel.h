#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_FILTER_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_FILTER_MODEL_H

#include <cstddef>
#include <unordered_map>
#include <vector>

#include <QtCore/QSortFilterProxyModel>

struct TreeNode;

enum class AddonStateFilter
{
    All,
    Enabled,
    Disabled,
};

class AddonTreeFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AddonTreeFilterModel(QObject* parent = nullptr);

    void HideEmptyCategories(bool hide);

    void Search(const QString& text);

    void ShowOnly(AddonStateFilter state);

    [[nodiscard]] AddonStateFilter ShowingOnly() const;

    [[nodiscard]] bool HidesAddons() const;

    [[nodiscard]] std::size_t TimesItWalkedTheTree() const;

    void setSourceModel(QAbstractItemModel* model) override;

    [[nodiscard]] QVariant data(const QModelIndex& position, int role) const override;

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    struct Reach
    {
        std::size_t addons = 0;
        std::size_t addonsShown = 0;
        std::size_t categories = 0;
        std::size_t categoriesShown = 0;
    };

    [[nodiscard]] bool TheStateAllows(const QModelIndex& position) const;

    [[nodiscard]] bool AnAddonShows(const QModelIndex& position, const TreeNode& addon) const;

    [[nodiscard]] Reach WalkBelow(const QModelIndex& position) const;

    [[nodiscard]] const Reach* ReachOf(const TreeNode& node) const;

    [[nodiscard]] QString CountTextOf(const TreeNode& node, const Reach& reach) const;

    void ForgetTheCounts();

    void AnnounceTheCounts(const QModelIndex& parent);

    bool hideEmpty_ = false;
    QString search_;
    AddonStateFilter state_ = AddonStateFilter::All;
    std::vector<QMetaObject::Connection> listening_;
    mutable std::unordered_map<const TreeNode*, Reach> reaches_;
    mutable bool counted_ = false;
    mutable std::size_t walks_ = 0;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_FILTER_MODEL_H
