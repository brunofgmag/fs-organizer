#include "viewmodel/QtBackgroundRunner.h"

#include <utility>

#include <QtCore/QThread>

QtBackgroundRunner::QtBackgroundRunner(QObject* parent) : QObject(parent)
{
}

void QtBackgroundRunner::Run(std::function<void()> work, std::function<void()> doneOnTheCallingThread)
{
    QThread* thread = QThread::create(std::move(work));

    connect(thread, &QThread::finished, this,
            [thread, done = std::move(doneOnTheCallingThread)]
            {
                thread->deleteLater();
                done();
            });

    thread->start();
}
