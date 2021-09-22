#include "MainWindow.h"
#include "LoginDialog.h"

#include <QApplication>

#include "utils.h"
#include <QSettings>
#include <QtWidgets>

void playground()
{

}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w; w.show();
//    B23LoginDialog dlg; dlg.show();
//    playground();
    return a.exec();
}
