#ifndef FS_ORGANIZER_VIEWMODEL_ADDON_TREE_FILTER_MODEL_H
#define FS_ORGANIZER_VIEWMODEL_ADDON_TREE_FILTER_MODEL_H

#include <QtCore/QSortFilterProxyModel>

class AddonTreeFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AddonTreeFilterModel(QObject* parent = nullptr);

    void HideEmptyCategories(bool hide);

    void Search(const QString& text);

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool hideEmpty_ = false;
    QString search_;
};

#endif // FS_ORGANIZER_VIEWMODEL_ADDON_TREE_FILTER_MODEL_H
