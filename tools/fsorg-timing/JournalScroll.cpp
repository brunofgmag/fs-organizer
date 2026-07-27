#include "JournalScroll.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTextStream>
#include <QtGui/QImage>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QStandardItemModel>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeView>

#include "view/JournalPage.h"
#include "view/PlainTextDelegate.h"
#include "viewmodel/JournalModel.h"
#include "viewmodel/JournalViewModel.h"

namespace
{
    constexpr int kFrames = 60;
    constexpr int kWarmupFrames = 10;
    constexpr int kRepetitions = 3;
    constexpr int kPixelsPerFrame = 24;
    int windowWidth = 1200;
    int windowHeight = 700;
    constexpr double kBudgetPerFrame = 8.0;

    QTextStream& Out()
    {
        static QTextStream stream(stdout);
        return stream;
    }

    void LetTheWindowSettle()
    {
        for (int pass = 0; pass < 20; ++pass)
        {
            QApplication::processEvents();
        }
    }

    void ShowAtAFixedSize(QWidget& widget)
    {
        widget.setFixedSize(windowWidth, windowHeight);
        widget.show();
        LetTheWindowSettle();
    }

    double Median(std::vector<double> samples)
    {
        std::ranges::sort(samples);

        return samples.at(samples.size() / 2);
    }

    double PaintCostOf(const QTreeView& view)
    {
        QScrollBar* bar = view.verticalScrollBar();
        QImage canvas(view.viewport()->size() * view.devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
        canvas.setDevicePixelRatio(view.devicePixelRatio());

        std::vector<double> rounds;

        for (int repetition = 0; repetition < kRepetitions; ++repetition)
        {
            std::vector<double> frames;

            for (int frame = 0; frame < kFrames; ++frame)
            {
                bar->setValue(std::min(bar->maximum(), frame * kPixelsPerFrame));

                QElapsedTimer paint;
                paint.start();

                view.viewport()->render(&canvas);

                if (frame >= kWarmupFrames)
                {
                    frames.push_back(static_cast<double>(paint.nsecsElapsed()) / 1e6);
                }
            }

            rounds.push_back(Median(frames));
        }

        return *std::ranges::min_element(rounds);
    }

    class PaintSpy final : public QObject
    {
    public:
        int repaintedHeight = 0;

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (const auto* paint = dynamic_cast<QPaintEvent*>(event))
            {
                repaintedHeight += paint->region().boundingRect().height();
            }

            return QObject::eventFilter(watched, event);
        }
    };

    double WhatANotchRepaints(const QTreeView& view)
    {
        constexpr int kNotches = 20;

        view.verticalScrollBar()->setValue(0);
        LetTheWindowSettle();

        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);

        for (int notch = 0; notch < kNotches; ++notch)
        {
            QWheelEvent wheel(QPointF(200, 200), view.viewport()->mapToGlobal(QPointF(200, 200)), QPoint(0, 0),
                              QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);

            QApplication::sendEvent(view.viewport(), &wheel);
            QApplication::processEvents();
        }

        view.viewport()->removeEventFilter(&spy);

        return spy.repaintedHeight / double{kNotches};
    }

    void ReportView(const QString& what, const QTreeView& view)
    {
        const double cost = PaintCostOf(view);
        const double repainted = WhatANotchRepaints(view);

        Out() << what.leftJustified(32) << QString::number(cost, 'f', 2).rightJustified(6) << " ms por pintura"
              << "   repinta " << QString::number(repainted, 'f', 0) << " de " << view.viewport()->height()
              << " px por entalhe\n";
        Out().flush();
    }

    QStandardItemModel* ConstantModel(const int rows, const int columns, const QString& text, QObject* parent)
    {
        auto* dumb = new QStandardItemModel(rows, columns, parent);

        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                dumb->setItem(row, column, new QStandardItem(text));
            }
        }

        return dumb;
    }

}

int MeasureTheJournalScroll(const OperationJournal& journal, const Session& session)
{
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() >= 4)
    {
        windowWidth = arguments.at(arguments.size() - 2).toInt();
        windowHeight = arguments.at(arguments.size() - 1).toInt();
    }

    JournalModel model;
    JournalViewModel viewModel(journal, session, model);

    JournalPage page(viewModel, model);
    ShowAtAFixedSize(page);

    QElapsedTimer timer;
    timer.start();
    viewModel.Show();
    LetTheWindowSettle();
    const qint64 opening = timer.elapsed();

    auto* view = page.findChild<QTreeView*>();
    if (view == nullptr)
    {
        Out() << "não achei a lista na página\n";
        Out().flush();
        return 2;
    }

    Out() << "linhas: " << model.rowCount({}) << "  abrir a aba: " << opening << " ms"
          << "  dpr " << view->devicePixelRatio() << "  viewport " << view->viewport()->width() << "x"
          << view->viewport()->height() << "\n\n";

    const double asShipped = PaintCostOf(*view);
    ReportView("pagina do diario", *view);

    view->setItemDelegate(new QStyledItemDelegate(view));
    LetTheWindowSettle();
    const double withTheStockDelegate = PaintCostOf(*view);
    ReportView("  com o delegate do Qt", *view);

    view->setItemDelegate(new PlainTextDelegate(view));
    LetTheWindowSettle();
    page.hide();

    const int rows = model.rowCount({});

    {
        QTreeView bare;
        bare.setModel(ConstantModel(rows, 7, QStringLiteral("D:/Library/Utils/simbridge"), &bare));
        bare.setUniformRowHeights(true);
        ShowAtAFixedSize(bare);
        ReportView("QTreeView cru, 7 col, caminho", bare);
        bare.hide();
    }

    {
        QTreeView bare;
        bare.setModel(ConstantModel(rows, 7, QStringLiteral("ok"), &bare));
        bare.setUniformRowHeights(true);
        ShowAtAFixedSize(bare);
        ReportView("QTreeView cru, 7 col, curto", bare);
        bare.hide();
    }

    {
        QTreeView bare;
        bare.setModel(ConstantModel(rows, 1, QStringLiteral("D:/Library/Utils/simbridge"), &bare));
        bare.setUniformRowHeights(true);
        ShowAtAFixedSize(bare);
        ReportView("QTreeView cru, 1 col, caminho", bare);
        bare.hide();
    }

    Out() << "\ndelegate do Qt " << QString::number(withTheStockDelegate, 'f', 2) << " ms -> nosso "
          << QString::number(asShipped, 'f', 2) << " ms\n";
    Out() << "orçamento por quadro: " << kBudgetPerFrame << " ms\n";
    Out() << (asShipped > kBudgetPerFrame ? "VERMELHO: o scroll engasga\n" : "VERDE\n");
    Out().flush();

    return asShipped > kBudgetPerFrame ? 1 : 0;
}
