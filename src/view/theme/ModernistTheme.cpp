#include "view/theme/ModernistTheme.h"

#include <QtGui/QFontDatabase>
#include <QtWidgets/QApplication>

#include "view/theme/ModernistStyle.h"
#include "view/theme/ModernistTones.h"

namespace
{
    void LoadTheArchivoFamilyOnce()
    {
        static const bool loaded = []
        {
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-Regular.ttf"));
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-SemiBold.ttf"));
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Archivo-ExtraBold.ttf"));
            return true;
        }();
        static_cast<void>(loaded);
    }
}

QPalette ModernistPalette(const Qt::ColorScheme scheme)
{
    const ModernistTones tones = TonesOf(scheme);

    QPalette palette;
    palette.setColor(QPalette::Window, tones.window);
    palette.setColor(QPalette::WindowText, tones.text);
    palette.setColor(QPalette::Base, tones.window);
    palette.setColor(QPalette::AlternateBase, tones.raised);
    palette.setColor(QPalette::Text, tones.text);
    palette.setColor(QPalette::PlaceholderText, tones.secondary);
    palette.setColor(QPalette::Button, tones.raised);
    palette.setColor(QPalette::ButtonText, tones.text);
    palette.setColor(QPalette::BrightText, tones.accentInk);
    palette.setColor(QPalette::Highlight, tones.raised);
    palette.setColor(QPalette::HighlightedText, tones.text);
    palette.setColor(QPalette::Accent, tones.accent);
    palette.setColor(QPalette::Link, tones.accentInk);
    palette.setColor(QPalette::LinkVisited, tones.accentInk);
    palette.setColor(QPalette::Light, tones.raised);
    palette.setColor(QPalette::Midlight, tones.edge);
    palette.setColor(QPalette::Mid, tones.divider);
    palette.setColor(QPalette::Dark, tones.chrome);
    palette.setColor(QPalette::Shadow, tones.chrome);
    palette.setColor(QPalette::ToolTipBase, tones.raised);
    palette.setColor(QPalette::ToolTipText, tones.text);

    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
    {
        palette.setColor(QPalette::Disabled, role, tones.disabled);
    }
    palette.setColor(QPalette::Disabled, QPalette::Base, tones.chrome);
    palette.setColor(QPalette::Disabled, QPalette::Button, tones.chrome);

    return palette;
}

QString ModernistStyleSheet(const Qt::ColorScheme scheme)
{
    const ModernistTones tones = TonesOf(scheme);

    return QStringLiteral(R"(
QMainWindow, QDialog { background: %window%; }
QStatusBar { background: %chrome%; border-top: 1px solid %divider%; }
QStatusBar::item { border: none; }
#TabStrip { background: %window%; border-bottom: 1px solid %divider%; }
#TriageStrip { background: %chrome%; border-bottom: 1px solid %divider%; }
#PageToolbar { border-bottom: 1px solid %divider%; }
QToolButton#Gear {
    background: transparent; border: 1px solid %edge%; color: %secondary%; padding: 4px 6px;
}
QToolButton#Gear:hover { color: %text%; border-color: %secondary%; }
QComboBox#ProfilePicker { font-weight: 600; padding: 3px 10px; }
QToolButton#FilterChip {
    background: transparent; border: 1px solid %edge%; color: %text%; padding: 3px 10px;
}
QToolButton#FilterChip:hover { border-color: %secondary%; }
QToolButton#FilterChip:checked {
    background: %accent%; border: 1px solid %accent%; color: %onAccent%; font-weight: 600;
}
QToolButton#FilterChip[population="none"] { border-color: %divider%; color: %faint%; }
QLabel[tag="filled"] { background: %accent%; color: %onAccent%; font-weight: 600; padding: 2px 7px; }
QLabel[tag="outlined"] { border: 1px solid %accent%; color: %accentInk%; font-weight: 600; padding: 1px 6px; }
QLabel[tag="muted"] { background: %raised%; color: %secondary%; padding: 2px 7px; }
QLabel#TriageQuiet { color: %secondary%; }
QFrame#TriageSeparator { background: %divider%; }
QLabel#FooterRestart { color: %accentInk%; font-weight: 600; }
QLabel#FooterSummary { color: %secondary%; }
QLabel#FooterAside { color: %secondary%; }
QProgressBar#FooterMeter { background: %raised%; border: none; }
QProgressBar#FooterMeter::chunk { background: %accent%; }
#PanelHeader { background: %chrome%; border-left: 1px solid %divider%; border-bottom: 1px solid %divider%; }
#PanelBody { background: %chrome%; border-left: 1px solid %divider%; }
#PanelRail { background: %chrome%; border-left: 1px solid %divider%; }
QToolButton#PanelExpand {
    background: transparent; border: none; color: %secondary%; padding: 0;
}
QToolButton#PanelExpand:hover { color: %text%; background: %raised%; }
QLabel#PanelTitle { color: %text%; }
QLabel#PanelSubHeading { color: %secondary%; font-weight: 700; }
QLabel#PanelPromise { color: %secondary%; }
QLabel#DetailFieldName { color: %secondary%; }
QTextEdit#UncutText {
    background: transparent; border: none;
    selection-background-color: %accent%; selection-color: %onAccent%;
}
QToolButton#PanelToggle, QToolButton#PanelClose {
    background: transparent; border: none; color: %secondary%; padding: 2px 6px;
}
QToolButton#PanelToggle:hover, QToolButton#PanelClose:hover { color: %text%; background: %raised%; }
QListWidget#SectionRail { background: %chrome%; border-right: 1px solid %divider%; outline: none; }
QListWidget#SectionRail::item { padding: 8px 16px; color: %secondary%; border-left: 3px solid transparent; }
QListWidget#SectionRail::item:selected, QListWidget#SectionRail::item:selected:active {
    background: %raised%; color: %text%; font-weight: 600; border-left: 3px solid %accent%;
}
QListWidget#SectionRail::item:disabled { color: %disabled%; }
QFrame#OptionsBox { background: %window%; border: 1px solid %divider%; }
#OptionsRow[follows="true"] { border-top: 1px solid %raised%; }
#OptionsChoice[follows="true"] { border-top: 1px solid %divider%; }
QLabel#OptionsGroupName {
    color: %secondary%; font-weight: 700; font-size: 11px; letter-spacing: 1px;
}
QLabel#OptionsChoiceName { font-weight: 600; }
QLabel#AboutVersion { font-weight: 700; }
QLabel#EmptyHeadline { font-weight: 800; }
QLabel#EmptyBody { color: %secondary%; }
QLabel#ModeExplained { color: %secondary%; }
QToolTip { background: %raised%; color: %text%; border: 1px solid %edge%; }
QSplitter::handle { background: %divider%; }
QHeaderView::section {
    background: %window%; color: %secondary%;
    border: none; border-bottom: 2px solid %divider%;
    padding: 6px 8px;
}
QTableView, QTreeView { border: none; }
QTableView { gridline-color: transparent; }
QLineEdit, QComboBox, QSpinBox {
    background: %raised%; border: 1px solid %edge%; border-radius: 0; padding: 3px 8px;
    selection-background-color: %accent%; selection-color: %onAccent%;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid %accent%; }
QPushButton {
    background: transparent; border: 1px solid %edge%; border-radius: 0;
    padding: 5px 12px; font-weight: 600;
}
QPushButton:hover { border-color: %secondary%; background: %raised%; }
QPushButton:default { background: %accent%; color: %onAccent%; border: 1px solid %accent%; }
QPushButton:default:hover { background: %accentWarm%; border-color: %accentWarm%; }
QPushButton:disabled { color: %disabled%; border-color: %divider%; background: transparent; }
QPushButton:default:disabled { background: transparent; color: %disabled%; border: 1px solid %divider%; }
QPushButton[role="primary"] { background: %accent%; color: %onAccent%; border: 1px solid %accent%; }
QPushButton[role="primary"]:hover { background: %accentWarm%; border-color: %accentWarm%; }
QPushButton[role="primary"]:disabled { background: transparent; color: %disabled%; border: 1px solid %divider%; }
QPushButton[role="destructive"] {
    background: transparent; border: 1px solid %accent%; color: %accentInk%;
}
QPushButton[role="destructive"]:hover { background: %raised%; }
QPushButton[role="destructive"]:disabled { border: 1px solid %divider%; color: %disabled%; }
QPushButton:focus { border-color: %text%; }
QPushButton[scale="small"] { padding: 2px 9px; }
)")
        .replace(QStringLiteral("%window%"), tones.window.name())
        .replace(QStringLiteral("%chrome%"), tones.chrome.name())
        .replace(QStringLiteral("%raised%"), tones.raised.name())
        .replace(QStringLiteral("%divider%"), tones.divider.name())
        .replace(QStringLiteral("%edge%"), tones.edge.name())
        .replace(QStringLiteral("%text%"), tones.text.name())
        .replace(QStringLiteral("%secondary%"), tones.secondary.name())
        .replace(QStringLiteral("%faint%"), tones.faint.name())
        .replace(QStringLiteral("%disabled%"), tones.disabled.name())
        .replace(QStringLiteral("%accentWarm%"), tones.accentWarm.name())
        .replace(QStringLiteral("%accentInk%"), tones.accentInk.name())
        .replace(QStringLiteral("%accent%"), tones.accent.name())
        .replace(QStringLiteral("%onAccent%"), tones.onAccent.name());
}

QIcon BrandIcon()
{
    static const QIcon icon = []
    {
        QIcon built;
        for (const int size : {16, 24, 32, 48, 64, 128, 256})
        {
            built.addFile(QStringLiteral(":/icons/app-icon_%1.png").arg(size), QSize(size, size));
        }
        return built;
    }();

    return icon;
}

void ApplyModernistTheme(QApplication& app)
{
    LoadTheArchivoFamilyOnce();

    QFont archivo(QStringLiteral("Archivo"));
    archivo.setPointSizeF(10.0);
    archivo.setFeature("tnum", 1);
    QApplication::setFont(archivo);

    RefreshModernistTheme(app);
}

void RefreshModernistTheme(QApplication& app)
{
    const Qt::ColorScheme scheme = CurrentColorScheme();

    QApplication::setStyle(new ModernistStyle(scheme));
    QApplication::setPalette(ModernistPalette(scheme));
    app.setStyleSheet(ModernistStyleSheet(scheme));
}
