#ifndef FS_ORGANIZER_VIEW_LIBRARY_SWAP_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_SWAP_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "application/ProfileService.h"

class AddonTreeViewModel;

class SwapDialog final : public QDialog
{
    Q_OBJECT

public:
    SwapDialog(const std::vector<TakenPlace>& swaps, const AddonTreeViewModel& viewModel, QWidget* parent = nullptr);

private:
    [[nodiscard]] QString NameAndVersionOf(const std::filesystem::path& addonFolder) const;

    const AddonTreeViewModel& viewModel_;
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_SWAP_DIALOG_H
