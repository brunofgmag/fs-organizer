#ifndef FS_ORGANIZER_INFRASTRUCTURE_SIM_LOADING_REPORT_TEXT_H
#define FS_ORGANIZER_INFRASTRUCTURE_SIM_LOADING_REPORT_TEXT_H

#include <string_view>

#include "application/ports/LoadingReportSource.h"

[[nodiscard]] LoadingReport LoadingReportFrom(std::string_view text);

#endif // FS_ORGANIZER_INFRASTRUCTURE_SIM_LOADING_REPORT_TEXT_H
