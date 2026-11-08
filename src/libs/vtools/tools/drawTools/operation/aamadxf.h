/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   aamadxf.h
 **  @brief  Reader of AAMA DXF pattern files (pieces, corners, curves, notches)
 **           
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#ifndef AAMADXF_H
#define AAMADXF_H

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * A piece of an AAMA DXF file (ASTM D6673). Coordinates are in millimetres, y up as in the file.
 *   layer 1  boundary (closed polyline) and texts  layer 2  turn points (corners)
 *   layer 3  curve points                           layer 4  notches
 *   layer 7  grain line                             layer 8  internal lines
 */
struct AamaPiece
{
    QString                    name;
    QVector<QPointF>           boundary;   // vertices of the outline
    QVector<bool>              turn;       // true where the vertex is a corner
    QVector<QPointF>           notches;
    QVector<QVector<QPointF>>  internals;
    QVector<QPointF>           grain;      // empty or two points
    QStringList                texts;
};

struct AamaFile
{
    QVector<AamaPiece> pieces;
    QString            units;    // what the file says about its units ("mm", "cm", "inch", or empty)
    QString            error;    // empty if the file could be read
};

/**
 * @brief readAama reads an AAMA DXF file.
 * @param unitMm size of one drawing unit in millimetres; 0 = from the file (millimetres if it does not say)
 */
AamaFile readAama(const QString &path, double unitMm = 0);

#endif // AAMADXF_H
