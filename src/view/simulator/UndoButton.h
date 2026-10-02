#ifndef FS_ORGANIZER_VIEW_SIMULATOR_UNDO_BUTTON_H
#define FS_ORGANIZER_VIEW_SIMULATOR_UNDO_BUTTON_H

#include <algorithm>

#include <QtGui/QFontMetrics>
#include <QtWidgets/QPushButton>

#include "support/MenuText.h"

inline constexpr int kUndoNameAtLeast = 56;

class UndoButton final : public QPushButton
{
public:
    explicit UndoButton(QWidget* parent = nullptr) : QPushButton(parent)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    void Name(const QString& effect, const QString& entry)
    {
        effect_ = effect;
        entry_ = entry;

        setEnabled(true);
        setToolTip(effect_.arg(entry_));
        updateGeometry();
        ShowWhatFits();
    }

    void NothingToUndo(const QString& label)
    {
        effect_.clear();
        entry_.clear();

        setEnabled(false);
        setToolTip(QString());
        setText(label);
        updateGeometry();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return effect_.isEmpty() ? QPushButton::sizeHint() : SizeSaying(effect_.arg(entry_));
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return effect_.isEmpty() ? QPushButton::minimumSizeHint() : SizeSaying(effect_.arg(Cut(kUndoNameAtLeast)));
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QPushButton::resizeEvent(event);
        ShowWhatFits();
    }

private:
    [[nodiscard]] int AroundTheText() const
    {
        return QPushButton::sizeHint().width() - fontMetrics().horizontalAdvance(text());
    }

    [[nodiscard]] QSize SizeSaying(const QString& said) const
    {
        return {AroundTheText() + fontMetrics().horizontalAdvance(said), QPushButton::sizeHint().height()};
    }

    [[nodiscard]] QString Cut(const int room) const
    {
        return fontMetrics().elidedText(entry_, Qt::ElideRight, room);
    }

    void ShowWhatFits()
    {
        if (effect_.isEmpty())
        {
            return;
        }

        const int roomForTheName = width() - AroundTheText() - fontMetrics().horizontalAdvance(effect_.arg(QString()));
        const QString fitting = effect_.arg(AsMenuText(Cut(std::max(roomForTheName, kUndoNameAtLeast))));

        if (fitting != text())
        {
            setText(fitting);
        }
    }

    QString effect_;
    QString entry_;
};

#endif // FS_ORGANIZER_VIEW_SIMULATOR_UNDO_BUTTON_H
