#include "viewmodel/DependencyText.h"

#include <QtCore/QObject>

#include "support/MomentText.h"

QString AnswerFor(const DependencyAnswer& answer)
{
    switch (answer.resolution)
    {
    case DependencyResolution::InThisLibrary:
        return answer.enabled ? QObject::tr("In this library, on") : QObject::tr("In this library, off");
    case DependencyResolution::InTheSimulator: return QObject::tr("In the simulator, outside the library");
    case DependencyResolution::Unverifiable: return QObject::tr("Not verifiable");
    }

    return {};
}

QString WhereTheListCameFrom(const DependencyReport& report)
{
    if (!report.listTakenAt.has_value())
    {
        return {};
    }

    if (report.listAccountFolder.empty())
    {
        return QObject::tr("From the simulator package list of %1.").arg(AsMoment(*report.listTakenAt));
    }

    return QObject::tr("From the simulator package list of %1, account %2.")
        .arg(AsMoment(*report.listTakenAt), QString::fromStdString(report.listAccountFolder));
}
