#ifndef FS_ORGANIZER_VIEW_DOCUMENTS_SELECTABLE_PAGES_H
#define FS_ORGANIZER_VIEW_DOCUMENTS_SELECTABLE_PAGES_H

#include <vector>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtGui/QPolygonF>
#include <QtPdf/QPdfSelection>
#include <QtPdfWidgets/QPdfView>

class QPdfDocument;

struct WhereAPageSits
{
    QRect box{};
    qreal scale = 1.0;
};

struct APlaceInTheText
{
    int page = -1;
    int index = -1;
};

struct APieceOfTheSelection
{
    int page = 0;
    QString text{};
    QList<QPolygonF> shapes{};
};

class SelectablePages final : public QPdfView
{
    Q_OBJECT

public:
    explicit SelectablePages(QWidget* parent = nullptr);

    void FollowTheDocument(QPdfDocument* document);

    [[nodiscard]] WhereAPageSits WhereThePageSits(int page) const;

    void StartSelectingAt(const QPoint& where);

    void ExtendTheSelectionTo(const QPoint& where);

    void SelectTheWordAt(const QPoint& where);

    void SelectTheLineAt(const QPoint& where);

    void SelectTheWholePage(int page);

    void ForgetTheSelection();

    [[nodiscard]] bool CarriesASelection() const;

    [[nodiscard]] QString WhatIsSelected() const;

    void CopyWhatIsSelected() const;

protected:
    void paintEvent(QPaintEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;

private:
    void ForgetWhatWasReadFromTheDocument();

    [[nodiscard]] int PageUnder(const QPoint& where) const;

    [[nodiscard]] QPointF WhereOnThePage(int page, const QPoint& where) const;

    [[nodiscard]] QSizeF ThePointSizeOf(int page) const;

    [[nodiscard]] const QPdfSelection& TheWholeOf(int page) const;

    [[nodiscard]] APlaceInTheText ThePlaceUnder(const QPoint& where) const;

    void MarkFrom(const APlaceInTheText& from, const APlaceInTheText& to);

    void Mark(std::vector<APieceOfTheSelection> pieces);

    QPdfDocument* document_ = nullptr;
    mutable std::vector<QSizeF> pointSizes_{};
    mutable QHash<int, QPdfSelection> wholePages_{};
    std::vector<APieceOfTheSelection> pieces_{};
    APlaceInTheText anchor_{};
};

#endif // FS_ORGANIZER_VIEW_DOCUMENTS_SELECTABLE_PAGES_H
