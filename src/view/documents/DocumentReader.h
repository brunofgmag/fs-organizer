#ifndef FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENT_READER_H
#define FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENT_READER_H

#include <filesystem>
#include <vector>

#include <QtCore/QPoint>
#include <QtWidgets/QWidget>

#include "domain/documents/DocumentBookmarks.h"
#include "domain/documents/DocumentClassification.h"

class QAction;
class QLabel;
class QLayout;
class QLineEdit;
class QMenu;
class QModelIndex;
class QPdfBookmarkModel;
class QPdfDocument;
class QPdfSearchModel;
class QPdfView;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class DocumentReader final : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentReader(QWidget* parent = nullptr);

    ~DocumentReader() override;

    void Read(const std::filesystem::path& document,
              int page,
              DocumentKind kind,
              const std::vector<DocumentBookmark>& bookmarks);

    void ShowTheBookmarks(const std::vector<DocumentBookmark>& bookmarks);

    void SayItIsShowing(const QString& caption);

    void SayItIsDetached(bool detached);

signals:
    void ThePageChanged(int page);

    void TheMarkOfThePageWasTurned(int page, bool marked);

    void TheBookmarkWasNamed(int page, const QString& name);

    void TheFolderWasAskedFor();

    void TheDetachWasAskedFor();

protected:
    void changeEvent(QEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void BuildTheOutlinePane();

    [[nodiscard]] QLayout* TheBar();

    void ConnectTheBar();

    void ConnectThePane();

    void ConnectTheDocument();

    void Retranslate() const;

    void SayWhereTheReadingIs() const;

    void SearchFor(const QString& wanted);

    void StepThroughTheResults(int by);

    void JumpToTheResult(int result);

    [[nodiscard]] std::vector<bool> WhichSectionsAreOpen() const;

    void OpenAgainWhatWasOpen(const std::vector<bool>& opened) const;

    void RebuildThePane();

    void PutTheSectionsIn(const QModelIndex& parent, QTreeWidgetItem* under);

    void PutTheBookmarksIn();

    [[nodiscard]] const DocumentBookmark* TheBookmarkOn(int page) const;

    [[nodiscard]] QString NameOf(const DocumentBookmark& bookmark) const;

    [[nodiscard]] QString TheHeadingOfThePane() const;

    void MarkTheSectionOfThePage() const;

    void SayWhetherThisPageIsMarked() const;

    void SayWhatTheMenuCanDo() const;

    void OfferTheMenuAt(const QPoint& where);

    void RenameWhatIsChosen();

    void ForgetWhatIsChosen();

    [[nodiscard]] bool TheChartAnswersThe(QEvent* event);

    void ZoomBy(int notches);

    QPdfDocument* document_ = nullptr;
    QPdfView* view_ = nullptr;
    QPdfSearchModel* search_ = nullptr;
    QPdfBookmarkModel* outline_ = nullptr;
    QTreeWidget* outlineView_ = nullptr;
    QLabel* outlineHeading_ = nullptr;
    QLineEdit* wanted_ = nullptr;
    QLabel* found_ = nullptr;
    QLabel* position_ = nullptr;
    QPushButton* previous_ = nullptr;
    QPushButton* previousResult_ = nullptr;
    QPushButton* nextResult_ = nullptr;
    QPushButton* closer_ = nullptr;
    QPushButton* further_ = nullptr;
    QPushButton* next_ = nullptr;
    QPushButton* fitWidth_ = nullptr;
    QPushButton* bookmark_ = nullptr;
    QPushButton* detach_ = nullptr;
    QPushButton* openFolder_ = nullptr;
    QLabel* caption_ = nullptr;
    QWidget* outlinePane_ = nullptr;
    QMenu* menu_ = nullptr;
    QAction* rename_ = nullptr;
    QAction* forget_ = nullptr;
    std::vector<DocumentSection> sections_{};
    std::vector<QTreeWidgetItem*> sectionItems_{};
    std::vector<DocumentBookmark> bookmarks_{};
    QPoint grabbedAt_{};
    DocumentKind kind_ = DocumentKind::Document;
    bool detached_ = false;
    bool grabbing_ = false;
    int result_ = -1;
};

#endif // FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENT_READER_H
