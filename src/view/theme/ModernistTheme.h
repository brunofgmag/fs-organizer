#ifndef FS_ORGANIZER_VIEW_THEME_MODERNIST_THEME_H
#define FS_ORGANIZER_VIEW_THEME_MODERNIST_THEME_H

#include <QtCore/Qt>
#include <QtGui/QIcon>
#include <QtGui/QPalette>

class QApplication;

void ApplyModernistTheme(QApplication& app);
void RefreshModernistTheme(QApplication& app);
QPalette ModernistPalette(Qt::ColorScheme scheme);
QString ModernistStyleSheet(Qt::ColorScheme scheme);
QIcon BrandIcon();

#endif // FS_ORGANIZER_VIEW_THEME_MODERNIST_THEME_H
