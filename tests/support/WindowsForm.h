#ifndef FS_ORGANIZER_TESTS_SUPPORT_WINDOWS_FORM_H
#define FS_ORGANIZER_TESTS_SUPPORT_WINDOWS_FORM_H

#include <filesystem>

#include "infrastructure/sim/WindowsPathMapper.h"

[[nodiscard]] inline std::filesystem::path InWindowsForm(const std::filesystem::path& path)
{
    return WindowsPathMapper{}.ToSimulator(SimulatorVariant::MSFS2024, path);
}

#endif // FS_ORGANIZER_TESTS_SUPPORT_WINDOWS_FORM_H
