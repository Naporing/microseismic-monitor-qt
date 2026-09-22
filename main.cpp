#include "mainwindow.h"
#include "appversion.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("SeismicWaveformsDemo");
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName("Naporing");
    MainWindow w;
    w.show();
    return QApplication::exec();
}
