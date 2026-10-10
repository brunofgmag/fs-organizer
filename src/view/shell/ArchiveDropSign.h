#ifndef FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_SIGN_H
#define FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_SIGN_H

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

inline constexpr int kDropOutline = 2;
inline constexpr int kDropPlateRightOfThePointer = 18;
inline constexpr int kDropPlateBelowThePointer = 24;
inline constexpr int kDropPlateBackOffFromThePointer = 8;
inline constexpr int kDropPlateAirAcross = 10;
inline constexpr int kDropPlateAirAlong = 6;

class ArchiveDropSign final : public QWidget
{
    Q_OBJECT

public:
    explicit ArchiveDropSign(QWidget* over);

    void InviteInto(const QRect& row, const QString& sentence, const QPoint& pointer);

    void Refuse(const QString& sentence, const QPoint& pointer);

    void Leave();

    [[nodiscard]] QRect PlateBox() const;

    [[nodiscard]] QRect Target() const;

    [[nodiscard]] QString Sentence() const;

    [[nodiscard]] bool Refuses() const;

    [[nodiscard]] bool IsCut() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void paintEvent(QPaintEvent* event) override;

private:
    void Say(const QRect& row, const QString& sentence, const QPoint& pointer, bool refuses);

    QRect row_;
    QString sentence_;
    QPoint pointer_;
    bool refuses_ = false;
};

#endif // FS_ORGANIZER_VIEW_SHELL_ARCHIVE_DROP_SIGN_H
