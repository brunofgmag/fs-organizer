#include "infrastructure/platform/WindowsTitleBar.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <dwmapi.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtWidgets/QWidget>

void ApplySystemTitleBarTheme(const QWidget& window)
{
    const auto handle = reinterpret_cast<HWND>(window.winId());
    if (handle == nullptr)
    {
        return;
    }

    const BOOL dark =
        QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? TRUE : FALSE;

    DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}
