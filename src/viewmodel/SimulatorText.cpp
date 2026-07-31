#include "viewmodel/SimulatorText.h"

#include <QtCore/QCoreApplication>

QString NameOf(const SimulatorVariant variant)
{
    return variant == SimulatorVariant::MSFS2020
        ? QCoreApplication::translate("SimulatorText", "Flight Simulator 2020")
        : QCoreApplication::translate("SimulatorText", "Flight Simulator 2024");
}
