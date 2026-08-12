#ifndef FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENT_READER_H
#define FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENT_READER_H

#include <filesystem>

#include <QtWidgets/QWidget>

class QLabel;
class QLayout;
class QLineEdit;
class QPdfBookmarkModel;
class QPdfDocument;
class QPdfSearchModel;
class QPdfView;
class QPushButton;
class QTreeView;

class DocumentReader final : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentReader(QWidget* parent = nullptr);

    void Read(const std::filesystem::path& document, int page);

signals:
    void ThePageChanged(int page);

    void TheFolderWasAskedFor();

protected:
    void changeEvent(QEvent* event) override;

private:
    void BuildTheOutlinePane();

    [[nodiscard]] QLayout* TheBar();

    void ConnectTheParts();

    void Retranslate() const;

    void SayWhereTheReadingIs() const;

    void SearchFor(const QString& wanted);

    void StepThroughTheResults(int by);

    void ShowTheOutlineOnlyWhenThereIsOne() const;

    void JumpToTheResult(int result);

    QPdfDocument* document_ = nullptr;
    QPdfView* view_ = nullptr;
    QPdfSearchModel* search_ = nullptr;
    QPdfBookmarkModel* outline_ = nullptr;
    QTreeView* outlineView_ = nullptr;
    QLabel* outlineHeading_ = nullptr;
    QLineEdit* wanted_ = nullptr;
    QLabel* found_ = nullptr;
    QLabel* position_ = nullptr;
    QPushButton* previous_ = nullptr;
    QPushButton* next_ = nullptr;
    QPushButton* fitWidth_ = nullptr;
    QPushButton* openFolder_ = nullptr;
    QWidget* outlinePane_ = nullptr;
    int result_ = -1;
};

#endif // FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENT_READER_H
