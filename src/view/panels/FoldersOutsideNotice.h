#ifndef FS_ORGANIZER_VIEW_PANELS_FOLDERS_OUTSIDE_NOTICE_H
#define FS_ORGANIZER_VIEW_PANELS_FOLDERS_OUTSIDE_NOTICE_H

#include <cstddef>

#include <QtWidgets/QWidget>

class QLabel;
class QPushButton;

class FoldersOutsideNotice final : public QWidget
{
    Q_OBJECT

public:
    explicit FoldersOutsideNotice(QWidget* parent = nullptr);

    void ShowFolders(std::size_t folders);

signals:
    void ImportRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void RetranslateUi();

    QLabel* said_ = nullptr;
    QPushButton* import_ = nullptr;
    std::size_t folders_ = 0;
};

#endif // FS_ORGANIZER_VIEW_PANELS_FOLDERS_OUTSIDE_NOTICE_H
