#ifndef FS_ORGANIZER_APPLICATION_MODEL_READING_GESTURES_H
#define FS_ORGANIZER_APPLICATION_MODEL_READING_GESTURES_H

struct ReadingGestures
{
    bool wheelZooms = false;
    bool dragMovesThePage = false;
};

inline constexpr ReadingGestures kGesturesAChartIsBornWith{.wheelZooms = true, .dragMovesThePage = true};
inline constexpr ReadingGestures kGesturesADocumentIsBornWith{.wheelZooms = false, .dragMovesThePage = false};

#endif // FS_ORGANIZER_APPLICATION_MODEL_READING_GESTURES_H
