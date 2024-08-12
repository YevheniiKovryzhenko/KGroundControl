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

#include "signal_filters.h"
#include <QtMath>

LowPassFilter::LowPassFilter(double sample_rate, double cutoff_frequency)
{
    double dt = 1.0 / sample_rate;
    double RC = 1.0 / (cutoff_frequency * 2.0 * M_PI);
    alpha_ = dt / (dt + RC);
    prev_output_ = 0.0;
}

double LowPassFilter::update(double input)
{
    double output = alpha_ * input + (1.0 - alpha_) * prev_output_;
    prev_output_ = output;
    return output;
}

