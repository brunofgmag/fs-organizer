#include "AppScroll.h"

#include <algorithm>
#include <vector>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTextStream>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeView>

#include "view/JournalPage.h"
#include "view/MainWindow.h"
#include "viewmodel/AddonTreeModel.h"
#include "viewmodel/JournalModel.h"
#include "viewmodel/JournalViewModel.h"

namespace
{
    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    void LetTheWindowSettle()
    {
        for (int pass = 0; pass < 40; ++pass)
        {
            QApplication::processEvents();
        }
    }

    class PaintSpy final : public QObject
    {
    public:
        int paints = 0;
        int repaintedHeight = 0;
        double spent = 0;

        void Forget()
        {
            paints = 0;
            repaintedHeight = 0;
            spent = 0;
        }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() != QEvent::Paint)
            {
                return QObject::eventFilter(watched, event);
            }

            ++paints;
            repaintedHeight += static_cast<QPaintEvent*>(event)->region().boundingRect().height();

            QElapsedTimer timer;
            timer.start();
            const bool handled = QObject::eventFilter(watched, event);
            spent += static_cast<double>(timer.nsecsElapsed()) / 1e6;

            return handled;
        }
    };

    double Median(std::vector<double> samples)
    {
        std::ranges::sort(samples);

        return samples.empty() ? 0 : samples.at(samples.size() / 2);
    }

    void ReportHover(const QString& what, QTreeView& view)
    {
        constexpr int kMoves = 30;

        LetTheWindowSettle();

        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);

        std::vector<double> moves;

        for (int move = 0; move < kMoves; ++move)
        {
            const QPointF where(200, 40 + move * 19);
            QMouseEvent moved(QEvent::MouseMove, where, view.viewport()->mapToGlobal(where),
                              Qt::NoButton, Qt::NoButton, Qt::NoModifier);

            QElapsedTimer timer;
            timer.start();

            QApplication::sendEvent(view.viewport(), &moved);
            QApplication::processEvents();

            moves.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }

        view.viewport()->removeEventFilter(&spy);

        Out() << what.leftJustified(34)
            << "linhas " << QString::number(view.model()->rowCount({})).rightJustified(5)
            << "   " << QString::number(Median(moves), 'f', 2).rightJustified(6)
            << " ms por movimento"
            << "   pinturas " << QString::number(spy.paints / double{kMoves}, 'f', 1)
            << "   repinta " << QString::number(spy.repaintedHeight / double{kMoves}, 'f', 0)
            << " de " << view.viewport()->height() << " px\n";
        Out().flush();
    }

    void ReportScroll(const QString& what, QTreeView& view)
    {
        constexpr int kNotches = 30;

        view.verticalScrollBar()->setValue(0);
        LetTheWindowSettle();

        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);

        std::vector<double> notches;

        for (int notch = 0; notch < kNotches; ++notch)
        {
            QWheelEvent wheel(QPointF(200, 200), view.viewport()->mapToGlobal(QPointF(200, 200)),
                              QPoint(0, 0), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                              Qt::NoScrollPhase, false);

            QElapsedTimer timer;
            timer.start();

            QApplication::sendEvent(view.viewport(), &wheel);
            QApplication::processEvents();

            notches.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }

        view.viewport()->removeEventFilter(&spy);

        Out() << what.leftJustified(34)
            << "linhas " << QString::number(view.model()->rowCount({})).rightJustified(5)
            << "   " << QString::number(Median(notches), 'f', 2).rightJustified(6)
            << " ms por entalhe"
            << "   pinturas " << QString::number(spy.paints / double{kNotches}, 'f', 1)
            << "   repinta " << QString::number(spy.repaintedHeight / double{kNotches}, 'f', 0)
            << " de " << view.viewport()->height() << " px\n";
        Out().flush();
    }
}

int MeasureTheAppJournal(MainWindow& window, JournalPage& page, JournalViewModel& viewModel,
                         JournalModel& model)
{
    QObject::connect(&window, &MainWindow::PageSelected, &viewModel,
                     [&page, &viewModel](const QWidget* selected)
                     {
                         if (selected == &page)
                         {
                             viewModel.Show();
                         }
                     });

    window.showMaximized();
    LetTheWindowSettle();

    QToolButton* open = nullptr;
    for (QToolButton* button : window.findChildren<QToolButton*>())
    {
        if (button->text().startsWith(QStringLiteral("Di")))
        {
            open = button;
        }
    }

    if (open == nullptr)
    {
        Out() << "não achei o botão do diário\n";
        Out().flush();
        return 2;
    }

    QElapsedTimer timer;
    timer.start();
    open->click();
    LetTheWindowSettle();
    const double firstShow = static_cast<double>(timer.nsecsElapsed()) / 1e6;

    timer.restart();
    viewModel.Show();
    LetTheWindowSettle();
    const double secondShow = static_cast<double>(timer.nsecsElapsed()) / 1e6;

    auto* view = page.findChild<QTreeView*>();
    if (view == nullptr)
    {
        Out() << "não achei a lista do diário\n";
        Out().flush();
        return 2;
    }

    Out() << "entradas no diario: " << model.rowCount({})
        << "  primeira abertura: " << QString::number(firstShow, 'f', 0) << " ms"
        << "  segunda: " << QString::number(secondShow, 'f', 0) << " ms"
        << "  viewport " << view->viewport()->width() << "x" << view->viewport()->height() << "\n";

    timer.restart();
    for (int column = 0; column < model.columnCount({}); ++column)
    {
        view->resizeColumnToContents(column);
    }
    Out() << "resizeColumnToContents nas 7 colunas: "
        << QString::number(static_cast<double>(timer.nsecsElapsed()) / 1e6, 'f', 0) << " ms\n\n";

    ReportScroll("diario inteiro", *view);
    ReportHover("passando o mouse", *view);

    QAbstractItemDelegate* ours = view->itemDelegate();
    view->setItemDelegate(new QStyledItemDelegate(view));
    LetTheWindowSettle();
    ReportScroll("  com o delegate do Qt", *view);
    view->setItemDelegate(ours);
    LetTheWindowSettle();


    if (auto* failuresOnly = page.findChild<QCheckBox*>())
    {
        failuresOnly->setChecked(true);
        LetTheWindowSettle();
        ReportScroll("so o que falhou", *view);
        ReportHover("  passando o mouse", *view);
        failuresOnly->setChecked(false);
        LetTheWindowSettle();
    }

    return 0;
}
