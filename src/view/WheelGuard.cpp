#include "view/WheelGuard.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtWidgets/QWidget>

namespace
{
    class WheelGuard final : public QObject
    {
    public:
        explicit WheelGuard(QWidget* parent) : QObject(parent)
        {
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() != QEvent::Wheel)
            {
                return QObject::eventFilter(watched, event);
            }

            const auto* widget = qobject_cast<QWidget*>(watched);

            if (widget == nullptr || widget->hasFocus())
            {
                return QObject::eventFilter(watched, event);
            }

            if (QWidget* container = widget->parentWidget(); container != nullptr)
            {
                QCoreApplication::sendEvent(container, event);
            }

            return true;
        }
    };
}

void LetTheWheelScrollPastUnlessTheWidgetHasFocus(QWidget* widget)
{
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->installEventFilter(new WheelGuard(widget));
}
