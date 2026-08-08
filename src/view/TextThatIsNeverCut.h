#ifndef FS_ORGANIZER_VIEW_TEXT_THAT_IS_NEVER_CUT_H
#define FS_ORGANIZER_VIEW_TEXT_THAT_IS_NEVER_CUT_H

#include <QtWidgets/QTextEdit>

class TextThatIsNeverCut final : public QTextEdit
{
public:
    explicit TextThatIsNeverCut(const QString& shown, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

    void wheelEvent(QWheelEvent* event) override;
};

#endif // FS_ORGANIZER_VIEW_TEXT_THAT_IS_NEVER_CUT_H
