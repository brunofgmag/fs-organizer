#ifndef FS_ORGANIZER_VIEW_SCROLL_THAT_REPORTS_ITS_CONTENT_H
#define FS_ORGANIZER_VIEW_SCROLL_THAT_REPORTS_ITS_CONTENT_H

#include <QtWidgets/QScrollArea>

class ScrollThatReportsItsContent final : public QScrollArea
{
public:
    explicit ScrollThatReportsItsContent(QWidget* parent = nullptr);

    void MeasureTheContentAt(int wide);

    [[nodiscard]] QSize sizeHint() const override;

private:
    int wide_ = 0;
};

#endif // FS_ORGANIZER_VIEW_SCROLL_THAT_REPORTS_ITS_CONTENT_H
