/******************************************************************************
 **  @file   prova3d_grid.h
 **  @brief  Prova 3D: the tapes of the dress form and the fine numbered grid between them.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#ifndef PROVA3D_GRID_H
#define PROVA3D_GRID_H

#include <QString>
#include <QVector>

#include "prova3d_engine.h"

namespace prova3d
{

struct Tape
{
    QString name;
    double  value;   // height (cm) for a level, angle (rad) for an upright tape
};

/// horizontal tapes: neck base, bust, under bust, waist, high hip, hip
QVector<Tape> levels();
/// upright tapes: centre front, princess lines, sides, centre back
QVector<Tape> meridians();
/// single points that are not crossings of tapes
QVector<std::pair<QString, std::pair<double, double>>> landmarks();   // name, (theta, y)

const double gridStep = 2.0;   // cm between the lines of the fine grid

/// "waist +4 · princess right −2 cm"
QString gridLabel(const sim::DressForm &form, double theta, double y);

/// moves a point to the nearest crossing of the grid (tapes first) when it is close enough
void snapToGrid(const sim::DressForm &form, double &theta, double &y);

/// polylines to draw on the form: tapes (bold) and the fine grid
struct GridLine { std::vector<sim::V3> points; bool tape; };
QVector<GridLine> gridLines(const sim::DressForm &form);

} // namespace prova3d

#endif // PROVA3D_GRID_H
