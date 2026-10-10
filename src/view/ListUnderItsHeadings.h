#ifndef FS_ORGANIZER_VIEW_LIST_UNDER_ITS_HEADINGS_H
#define FS_ORGANIZER_VIEW_LIST_UNDER_ITS_HEADINGS_H

class HeadingsOverTheColumns;
class QScrollArea;
class QVBoxLayout;
class QWidget;

[[nodiscard]] QVBoxLayout* ListUnderItsHeadings(HeadingsOverTheColumns& headings, QScrollArea& list, QWidget& owner);

#endif // FS_ORGANIZER_VIEW_LIST_UNDER_ITS_HEADINGS_H
