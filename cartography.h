#ifndef CARTOGRAPHY_H
#define CARTOGRAPHY_H

#include <QWidget>
#include <QtLocation>
#include <QtPositioning>
// import QtLocation
    // import QtPositioning

namespace Ui {
class cartography;
}

class cartography : public QWidget
{
    Q_OBJECT

public:
    explicit cartography(QWidget *parent = nullptr);
    ~cartography();

private:
    Ui::cartography *ui;
};

#endif // CARTOGRAPHY_H
