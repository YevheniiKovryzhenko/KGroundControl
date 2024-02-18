#include "kgroundcontrol.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    KGroundControl w;
    w.show();
    return a.exec();
}
