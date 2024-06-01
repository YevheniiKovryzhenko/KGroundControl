#include "cartography.h"
#include "ui_cartography.h"

cartography::cartography(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::cartography)
{
    ui->setupUi(this);
}

cartography::~cartography()
{
    delete ui;
}
