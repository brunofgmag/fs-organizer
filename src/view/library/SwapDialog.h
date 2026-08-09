#ifndef FS_ORGANIZER_VIEW_LIBRARY_SWAP_DIALOG_H
#define FS_ORGANIZER_VIEW_LIBRARY_SWAP_DIALOG_H

#include <vector>

#include <QtWidgets/QDialog>

#include "application/ProfileService.h"
#include "viewmodel/AddonTreeViewModel.h"

class QGridLayout;
class QLabel;

class SwapDialog final : public QDialog
{
    Q_OBJECT

public:
    SwapDialog(const std::vector<TakenPlace>& swaps, const AddonTreeViewModel& viewModel, QWidget* parent = nullptr);

    void ShowTheSizes(const std::vector<WeighedSwap>& weighed);

private:
    [[nodiscard]] static QLabel* AddTheSide(QGridLayout& grid, int row, int column, const QString& nameAndVersion);

    static void Retell(QLabel& side, const MeasuredFolder& measured);

    [[nodiscard]] QString NameAndVersionOf(const std::filesystem::path& addonFolder) const;

    const AddonTreeViewModel& viewModel_;
    std::vector<QLabel*> goesOff_;
    std::vector<QLabel*> goesOn_;
};

#endif // FS_ORGANIZER_VIEW_LIBRARY_SWAP_DIALOG_H
