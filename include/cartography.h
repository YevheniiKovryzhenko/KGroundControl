/****************************************************************************
//          Auburn University Aerospace Engineering Department
//             Aero-Astro Computational and Experimental lab
//
//     Copyright (C) 2024  Yevhenii Kovryzhenko
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the
//     GNU AFFERO GENERAL PUBLIC LICENSE Version 3
//     along with this program.  If not, see <https://www.gnu.org/licenses/>
//
****************************************************************************/

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
