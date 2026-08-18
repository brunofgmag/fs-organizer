#ifndef FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENTS_PAGE_H
#define FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENTS_PAGE_H

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QtCore/QRect>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include "viewmodel/DocumentsViewModel.h"

class DocumentReader;
class EmptyState;
class QDialog;
class QSplitter;
class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

class DocumentsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentsPage(DocumentsViewModel& viewModel, QWidget* parent = nullptr);

    void Show(DocumentPanel panel);

    void Reveal(const std::string& addon);

    void DetachTheReading();

    void BringTheReadingBack();

protected:
    void changeEvent(QEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct PanelOfTheTab
    {
        QTreeWidget* index = nullptr;
        QPushButton* button = nullptr;
        std::vector<DocumentLine> lines{};
    };

    [[nodiscard]] QWidget* TheBar();

    [[nodiscard]] QWidget* TheIndexSide();

    [[nodiscard]] QWidget* TheReadingSide();

    [[nodiscard]] QTreeWidget* AnIndex(const QString& named);

    void ConnectTheBar();

    void ConnectTheIndex();

    void ConnectTheReader();

    void Rebuild();

    void RebuildTheIndexOf(DocumentPanel panel);

    void Retranslate();

    void ShowWhatIsHappening();

    void RetellTheManual();

    void ShowTheReadingSide();

    void ShowTheIndex();

    void Open(const DocumentLine& line);

    [[nodiscard]] bool AnswerTheClickOn(DocumentPanel panel, const QPoint& where);

    void PutTheGroupIn(QTreeWidget& index,
                       QTreeWidgetItem* parent,
                       const DocumentGroup& group,
                       PanelOfTheTab& built,
                       const QSet<QString>& opened,
                       const QString& trail,
                       int depth);

    void TurnTheStarOf(DocumentPanel panel, const DocumentLine& line);

    [[nodiscard]] const DocumentLine* LineOf(DocumentPanel panel, const QTreeWidgetItem* item) const;

    [[nodiscard]] static QRect TheArrowOf(const QTreeWidget& index, const QTreeWidgetItem& group);

    DocumentsViewModel& viewModel_;
    std::array<PanelOfTheTab, 2> panels_{};
    DocumentPanel showing_ = DocumentPanel::Documents;
    QSplitter* split_ = nullptr;
    QStackedWidget* lists_ = nullptr;
    QStackedWidget* readingSide_ = nullptr;
    DocumentReader* reader_ = nullptr;
    EmptyState* nothingIndexed_ = nullptr;
    EmptyState* nothingOpen_ = nullptr;
    EmptyState* elsewhere_ = nullptr;
    EmptyState* manual_ = nullptr;
    QPushButton* getTheManual_ = nullptr;
    QPushButton* readAgain_ = nullptr;
    QPushButton* stop_ = nullptr;
    QPushButton* bringItBack_ = nullptr;
    QPushButton* readTheLibrary_ = nullptr;
    QLabel* readAt_ = nullptr;
    QLabel* howFar_ = nullptr;
    QProgressBar* meter_ = nullptr;
    QWidget* progress_ = nullptr;
    QDialog* window_ = nullptr;
    std::optional<DocumentLine> open_{};
    bool settled_ = false;
    bool askedForTheManual_ = false;
    std::size_t indexed_ = 0;
    std::size_t outOf_ = 0;
};

#endif // FS_ORGANIZER_VIEW_DOCUMENTS_DOCUMENTS_PAGE_H
