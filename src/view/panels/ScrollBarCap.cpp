#include "view/panels/ScrollBarCap.h"

#include <QtCore/QEvent>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QWidget>

namespace
{
    constexpr auto kCapName = "ScrollBarCap";

    class ScrollBarCap final : public QWidget
    {
    public:
        explicit ScrollBarCap(QHeaderView* header) : QWidget(nullptr), header_(header)
        {
            setObjectName(QLatin1String(kCapName));
            setAttribute(Qt::WA_StyledBackground, true);
            header_->installEventFilter(this);
            MatchTheColumnHeader();
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (watched == header_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
            {
                MatchTheColumnHeader();
            }

            return QWidget::eventFilter(watched, event);
        }

    private:
        void MatchTheColumnHeader()
        {
            if (header_->height() > 0)
            {
                setFixedHeight(header_->height());
            }
        }

        QHeaderView* header_;
    };
}

void CapTheScrollBarOf(QAbstractScrollArea* area, QHeaderView* header)
{
    if (area == nullptr || header == nullptr)
    {
        return;
    }

    area->addScrollBarWidget(new ScrollBarCap(header), Qt::AlignTop);
}
