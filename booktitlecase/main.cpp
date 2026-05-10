#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("local");
    QCoreApplication::setApplicationName("booktitlecase");

    MainWindow w;
    w.show();
    return app.exec();
}
