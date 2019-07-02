// Copyright Alan (AJ) Pryor, Jr. 2017
// Transcribed from MATLAB code by Colin Ophus
// PRISM is distributed under the GNU General Public License (GPL)
// If you use PRISM, we ask that you cite the following papers:

#define PRISM_BUILDING_GUI 1
#include <QApplication>
#include "prismmainwindow.h"
#include <QFontDatabase>
#include <QFont>

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    QFontDatabase database;
    int id1 = database.addApplicationFont(":/fonts/Roboto-Regular.ttf");
    int id2 = database.addApplicationFont(":/fonts/Roboto-Black.ttf");
    int id3 = database.addApplicationFont(":/fonts/Roboto-Light.ttf");
    int id4 = database.addApplicationFont(":/fonts/Roboto-Medium.ttf");
    //dumb comment times two threee
    QFont font = QFont("Roboto");
    a.setFont(font);
    PRISMMainWindow w;
    w.show();
    w.setGeometry(100,100,850,700);
    return a.exec();
}
