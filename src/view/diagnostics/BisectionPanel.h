#ifndef FS_ORGANIZER_VIEW_DIAGNOSTICS_BISECTION_PANEL_H
#define FS_ORGANIZER_VIEW_DIAGNOSTICS_BISECTION_PANEL_H

#include <QtWidgets/QWidget>

#include "viewmodel/BisectionViewModel.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTreeWidget;

class BisectionPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit BisectionPanel(BisectionViewModel& viewModel, QWidget* parent = nullptr);

signals:
    void StatusChanged(const QString& message);

    void ImportRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    enum Page : int
    {
        NotStarted = 0,
        Asking = 1,
        ItDrifted = 2,
        Finished = 3,
        TheLibraryGainedAnAddon = 4,
    };

    [[nodiscard]] QWidget* CreateTheOpening();

    [[nodiscard]] QWidget* CreateTheRound();

    [[nodiscard]] QWidget* CreateWhatHappenedSoFar();

    [[nodiscard]] QWidget* CreateTheDrift();

    [[nodiscard]] QWidget* CreateWhatJoinedTheLibrary();

    [[nodiscard]] QWidget* CreateTheOutcome();

    void RetranslateUi();

    void ShowWhereItStands();

    void ShowWhatWillBeSearched() const;

    void ShowTheRound() const;

    void ShowWhatMoved() const;

    void ShowWhatJoinedTheLibrary() const;

    void ShowTheOutcome() const;

    void ListTheUnitsOf(QTreeWidget* tree, const std::vector<UnitOnScreen>& units) const;

    void ListTheDriftIn(QTreeWidget* tree) const;

    void ListWhatHappenedSoFar() const;

    [[nodiscard]] bool ItIsShowingTheEndOfTheStory() const;

    void KeepShowingTheEndOfTheStory() const;

    BisectionViewModel& viewModel_;
    QStackedWidget* body_ = nullptr;
    QLabel* headline_ = nullptr;
    QLabel* announced_ = nullptr;
    QLabel* outOfReach_ = nullptr;
    QLabel* promise_ = nullptr;
    QTreeWidget* toBeSearched_ = nullptr;
    QPushButton* start_ = nullptr;
    QLabel* standing_ = nullptr;
    QLabel* ask_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* crashed_ = nullptr;
    QPushButton* ranFine_ = nullptr;
    QPushButton* stop_ = nullptr;
    QTreeWidget* turnedOn_ = nullptr;
    QWidget* aside_ = nullptr;
    QLabel* soFar_ = nullptr;
    QWidget* story_ = nullptr;
    QScrollArea* scrolled_ = nullptr;
    QLabel* drifted_ = nullptr;
    QLabel* whatStartingOverCosts_ = nullptr;
    QLabel* notInTheJournal_ = nullptr;
    QTreeWidget* divergences_ = nullptr;
    QPushButton* startOver_ = nullptr;
    QPushButton* giveUp_ = nullptr;
    QLabel* joined_ = nullptr;
    QLabel* notInTheJournalEither_ = nullptr;
    QTreeWidget* whatJoined_ = nullptr;
    QPushButton* carryOn_ = nullptr;
    QPushButton* giveUpInstead_ = nullptr;
    QLabel* outcome_ = nullptr;
    QLabel* singleCulprit_ = nullptr;
    QLabel* aboutTheSecondPass_ = nullptr;
    QTreeWidget* whatIsLeft_ = nullptr;
    QPushButton* refine_ = nullptr;
    QPushButton* bringThemIn_ = nullptr;
    QPushButton* finish_ = nullptr;
};

#endif // FS_ORGANIZER_VIEW_DIAGNOSTICS_BISECTION_PANEL_H
