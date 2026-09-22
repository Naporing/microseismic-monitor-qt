#include "mainwindow.h"
#include "appversion.h"
#include "updatemanager.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("SeismicWaveformsDemo");
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName("Naporing");
    UpdateManager updates;
    MainWindow w(nullptr, &updates);
    QObject::connect(&w, &MainWindow::restartRequested, &a, &QCoreApplication::quit, Qt::QueuedConnection);
    w.show();
    QTimer::singleShot(5000, &updates, [&updates] { updates.checkForUpdates(true); });
    return QApplication::exec();
}
