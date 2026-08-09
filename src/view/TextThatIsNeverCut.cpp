#include "view/TextThatIsNeverCut.h"

#include <QtCore/qmath.h>
#include <QtGui/QTextDocument>
#include <QtGui/QWheelEvent>

TextThatIsNeverCut::TextThatIsNeverCut(const QString& shown, QWidget* parent) : QTextEdit(parent)
{
    setObjectName(QStringLiteral("UncutText"));
    setReadOnly(true);
    setFrameShape(NoFrame);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    viewport()->setAutoFillBackground(false);
    document()->setDocumentMargin(0);
    setPlainText(shown);
}

void TextThatIsNeverCut::resizeEvent(QResizeEvent* event)
{
    QTextEdit::resizeEvent(event);

    document()->setTextWidth(viewport()->width());

    if (const int tall = qCeil(document()->size().height()); tall != height())
    {
        setFixedHeight(tall);
    }
}

void TextThatIsNeverCut::wheelEvent(QWheelEvent* event)
{
    event->ignore();
}
