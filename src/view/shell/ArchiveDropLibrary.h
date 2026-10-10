#ifndef FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_LIBRARY_H
#define FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_LIBRARY_H

#include <filesystem>
#include <optional>
#include <vector>

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QString>

class QWidget;

struct DropCategory
{
    std::filesystem::path path{};
    QString shown{};
    QRect row{};
};

class ArchiveDropLibrary
{
public:
    virtual ~ArchiveDropLibrary() = default;

    [[nodiscard]] virtual bool HasALibrary() const = 0;

    [[nodiscard]] virtual std::optional<DropCategory> CategoryUnder(const QWidget& over, const QPoint& point) const = 0;

    virtual void SelectOnceShown(const std::vector<std::filesystem::path>& folders) = 0;

    virtual void EnableAddons(const std::vector<std::filesystem::path>& folders) = 0;

    virtual void RefreshUndoState() const = 0;
};

#endif // FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_LIBRARY_H
