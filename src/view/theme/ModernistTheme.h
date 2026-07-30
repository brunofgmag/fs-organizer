#pragma once

#include <QtCore/Qt>
#include <QtGui/QIcon>
#include <QtGui/QPalette>

class QApplication;

void ApplyModernistTheme(QApplication& app);
void RefreshModernistTheme(QApplication& app);
QPalette ModernistPalette(Qt::ColorScheme scheme);
QString ModernistStyleSheet(Qt::ColorScheme scheme);
QIcon BrandIcon();
