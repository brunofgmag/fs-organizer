#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FS Organizer"));
    QApplication::setApplicationVersion(QStringLiteral(FSORG_VERSION));
    QApplication::setOrganizationName(QStringLiteral("fs-organizer"));

    QMainWindow window;
    window.setWindowTitle(QApplication::applicationName());
    window.resize(1024, 720);
    window.show();

    return QApplication::exec();
}
