/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   piece_curve_edit.h
 **  @brief  Piece editor: edits of the curves of a piece (inner points, pins, simplify)
 **          stored on the nodes of the piece, re-applied in every size.
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

#ifndef PIECE_CURVE_EDIT_H
#define PIECE_CURVE_EDIT_H

#include <QDomElement>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVector>

class VAbstractPattern;
class VSplinePath;
class VAbstractCurve;

/**
 * Edits made in the piece editor to a curve of a piece. They are stored as attributes of the <node> of the curve, so
 * the draft does not change and the edits are re-applied to the curve of every size:
 *   simplify="-" or "0.31;0.62"   the curve is replaced by a smooth curve with fewer points; the numbers are where the
 *                                  new points are, as a fraction of the length (so every size gets the same points)
 *   curveOffsets="2:0.5,-0.3"     inner point 2 moved by (0.5, -0.3) pattern units
 *   curvePins="2;5"               inner points 2 and 5 are pinned: they hold the curve when its ends move
 * and on the <node> of a point:
 *   bend="true"                   when this point moves, the curves next to it only move their end with its handle
 *                                  (instead of following with their whole shape)
 */
namespace PieceCurveEdit
{
extern const QString AttrSimplify;
extern const QString AttrCurveOffsets;
extern const QString AttrCurvePins;
extern const QString AttrBend;

struct CurveEdit
{
    bool               simplified {false};
    QVector<qreal>     splits;          // fractions of the length, 0 < f < 1
    QMap<int, QPointF> offsets;         // pattern units
    QVector<int>       pins;

    bool isEmpty() const { return not simplified && offsets.isEmpty() && pins.isEmpty(); }
};

QDomElement    findNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId);
CurveEdit      readCurveEdit(const QMap<QString, QString> &attributes);
bool           readBend(const QMap<QString, QString> &attributes);

QString        formatSplits(bool simplified, const QVector<qreal> &splits);
QString        formatOffsets(const QMap<int, QPointF> &offsets);
QString        formatPins(const QVector<int> &pins);

/** @brief computeSplits where to put the points of the simplified curve so that it stays within the tolerance. */
QVector<qreal> computeSplits(const QVector<QPointF> &points, qreal tolerance);
/** @brief fitCurve a smooth curve through the points, one segment between two splits. */
VSplinePath    fitCurve(const QVector<QPointF> &points, const QVector<qreal> &splits);
} // namespace PieceCurveEdit

#endif // PIECE_CURVE_EDIT_H
