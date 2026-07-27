#ifndef FS_ORGANIZER_VIEWMODEL_QT_BACKGROUND_RUNNER_H
#define FS_ORGANIZER_VIEWMODEL_QT_BACKGROUND_RUNNER_H

#include <functional>

#include <QtCore/QObject>

#include "application/ports/BackgroundRunner.h"

class QtBackgroundRunner final : public QObject, public BackgroundRunner
{
    Q_OBJECT

public:
    explicit QtBackgroundRunner(QObject* parent = nullptr);

    void Run(std::function<void()> work, std::function<void()> doneOnTheCallingThread) override;
};

#endif // FS_ORGANIZER_VIEWMODEL_QT_BACKGROUND_RUNNER_H
