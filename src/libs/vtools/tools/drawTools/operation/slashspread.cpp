/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   slashspread.cpp
 **  @brief  Slash and spread: the geometry shared by the tool and its preview
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

#include "slashspread.h"

#include <QLineF>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "../../piece_curve_edit.h"
#include "../vgeometry/vabstractcurve.h"
#include "../vgeometry/varc.h"
#include "../vgeometry/vcubicbezier.h"
#include "../vgeometry/vcubicbezierpath.h"
#include "../vgeometry/vellipticalarc.h"
#include "../vgeometry/vpointf.h"
#include "../vgeometry/vspline.h"
#include "../vgeometry/vsplinepath.h"
#include "../vmisc/def.h"
#include "../vpatterndb/vcontainer.h"

namespace
{
qreal normalized(qreal angle) // -180 .. 180
{
    angle = std::fmod(angle, 360.0);
    if (angle > 180)
    {
        angle -= 360;
    }
    if (angle <= -180)
    {
        angle += 360;
    }
    return angle;
}

QVector<QPointF> polylineAt(const QVector<QPointF> &line, const QVector<qreal> &fractions)
{
    QVector<qreal> lengths {0};
    for (int i = 1; i < line.size(); ++i)
    {
        lengths.append(lengths.last() + QLineF(line.at(i - 1), line.at(i)).length());
    }
    const qreal total = lengths.last();
    QVector<QPointF> points;
    for (const qreal f : fractions)
    {
        const qreal s = f * total;
        int i = 1;
        while (i < lengths.size() - 1 && lengths.at(i) < s)
        {
            ++i;
        }
        const qreal segment = lengths.at(i) - lengths.at(i - 1);
        const qreal k = segment > 0 ? (s - lengths.at(i - 1)) / segment : 0;
        points.append(line.at(i - 1) + (line.at(i) - line.at(i - 1)) * k);
    }
    return points;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
SlashSpread::SlashSpread(const QPointF &hinge, const QVector<QPointF> &edge, int slashes, qreal spread, bool percent,
                         Fixed fixed)
    : m_hinge(hinge)
{
    if (edge.size() < 2 || slashes < 1)
    {
        return;
    }

    qreal edgeLength = 0;
    for (int i = 1; i < edge.size(); ++i)
    {
        edgeLength += QLineF(edge.at(i - 1), edge.at(i)).length();
    }
    m_total = percent ? edgeLength * spread / 100.0 : spread;

    // Slashes evenly spaced along the edge
    QVector<qreal> fractions;
    for (int i = 1; i <= slashes; ++i)
    {
        fractions.append(static_cast<qreal>(i) / (slashes + 1));
    }
    m_slashEnds = polylineAt(edge, fractions);

    m_startAngle = QLineF(hinge, edge.first()).angle();
    const qreal endPosition = normalized(QLineF(hinge, edge.last()).angle() - m_startAngle);
    if (qFuzzyIsNull(endPosition))
    {
        return; // the edge is in line with the hinge: nothing to open
    }
    m_sweep = endPosition > 0 ? 1 : -1;

    for (const QPointF &end : m_slashEnds)
    {
        m_slashPositions.append(fanPosition(end));
    }

    // Each slash opens so that the edge gets longer by the same amount there
    const qreal gap = m_total / slashes;
    QVector<qreal> cumulative {0};
    for (const QPointF &end : m_slashEnds)
    {
        const qreal r = QLineF(hinge, end).length();
        qreal angle = 0;
        if (r > 0)
        {
            angle = qRadiansToDegrees(2 * qAsin(qBound(-1.0, gap / (2 * r), 1.0)));
        }
        cumulative.append(cumulative.last() + m_sweep * angle);
    }

    qreal fixedRotation = 0;
    switch (fixed)
    {
        case Fixed::End:
            fixedRotation = cumulative.last();
            break;
        case Fixed::Both:
        {
            const int low = slashes / 2;
            const int high = (slashes + 1) / 2;
            fixedRotation = (cumulative.at(low) + cumulative.at(high)) / 2;
            break;
        }
        case Fixed::Start:
        default:
            break;
    }
    for (const qreal r : cumulative)
    {
        m_rotations.append(r - fixedRotation);
    }
    m_valid = true;
}

//---------------------------------------------------------------------------------------------------------------------
qreal SlashSpread::fanPosition(const QPointF &p) const
{
    return m_sweep * normalized(QLineF(m_hinge, p).angle() - m_startAngle);
}

//---------------------------------------------------------------------------------------------------------------------
int SlashSpread::section(const QPointF &p) const
{
    if (QLineF(m_hinge, p).length() < 1e-6)
    {
        return -1; // the hinge itself does not move
    }
    const qreal position = fanPosition(p);
    int section = 0;
    for (const qreal slash : m_slashPositions)
    {
        if (position > slash + 1e-9)
        {
            ++section;
        }
    }
    return section;
}

//---------------------------------------------------------------------------------------------------------------------
qreal SlashSpread::rotation(int section) const
{
    if (not m_valid || section < 0 || section >= m_rotations.size())
    {
        return 0;
    }
    return m_rotations.at(section);
}

//---------------------------------------------------------------------------------------------------------------------
QPointF SlashSpread::map(const QPointF &p) const
{
    const qreal angle = rotation(section(p));
    if (qFuzzyIsNull(angle))
    {
        return p;
    }
    QLineF line(m_hinge, p);
    line.setAngle(line.angle() + angle);
    return line.p2();
}

//---------------------------------------------------------------------------------------------------------------------
VAbstractCurve *SlashSpread::opened(const QSharedPointer<VAbstractCurve> &curve, const QString &suffix) const
{
    const QVector<QPointF> points = curve->getPoints();
    int first = -2;
    bool oneSection = true;
    for (const QPointF &p : points)
    {
        const int s = section(p);
        if (s == -1)
        {
            continue; // on the hinge
        }
        if (first == -2)
        {
            first = s;
        }
        else if (s != first)
        {
            oneSection = false;
            break;
        }
    }

    if (oneSection)
    {
        const qreal angle = rotation(qMax(first, 0));
        switch (static_cast<GOType>(curve->getType()))
        {
            case GOType::Arc:
                return (new VArc(qSharedPointerDynamicCast<VArc>(curve)->Rotate(m_hinge, angle, suffix)));
            case GOType::EllipticalArc:
                return (new VEllipticalArc(
                    qSharedPointerDynamicCast<VEllipticalArc>(curve)->Rotate(m_hinge, angle, suffix)));
            case GOType::Spline:
                return (new VSpline(qSharedPointerDynamicCast<VSpline>(curve)->Rotate(m_hinge, angle, suffix)));
            case GOType::SplinePath:
                return (new VSplinePath(qSharedPointerDynamicCast<VSplinePath>(curve)->Rotate(m_hinge, angle, suffix)));
            case GOType::CubicBezier:
                return (new VCubicBezier(
                    qSharedPointerDynamicCast<VCubicBezier>(curve)->Rotate(m_hinge, angle, suffix)));
            case GOType::CubicBezierPath:
                return (new VCubicBezierPath(
                    qSharedPointerDynamicCast<VCubicBezierPath>(curve)->Rotate(m_hinge, angle, suffix)));
            default:
                break;
        }
    }

    // Cut by a slash: the opened pieces joined again by one smooth curve
    QVector<QPointF> moved;
    moved.reserve(points.size());
    for (const QPointF &p : points)
    {
        moved.append(map(p));
    }
    const QVector<qreal> splits = PieceCurveEdit::computeSplits(moved, ToPixel(0.3, Unit::Mm));
    VSplinePath fitted = PieceCurveEdit::fitCurve(moved, splits);
    const QVector<VSplinePoint> splinePoints = fitted.GetSplinePath();
    auto *result = new VSplinePath(splinePoints);
    result->setName(curve->name() + suffix);
    result->setLineColor(curve->getLineColor());
    result->SetPenStyle(curve->GetPenStyle());
    result->setLineWeight(curve->getLineWeight());
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QPointF> SlashSpread::edgeOf(const VContainer *data, quint32 startId, quint32 endId,
                                     const QVector<quint32> &objects)
{
    const QPointF start = static_cast<QPointF>(*data->GeometricObject<VPointF>(startId));
    const QPointF end = static_cast<QPointF>(*data->GeometricObject<VPointF>(endId));
    for (const quint32 id : objects)
    {
        const QSharedPointer<VGObject> obj = data->GetGObject(id);
        if (static_cast<GOType>(obj->getType()) == GOType::Point)
        {
            continue;
        }
        const QSharedPointer<VAbstractCurve> curve = data->GeometricObject<VAbstractCurve>(id);
        QVector<QPointF> points = curve->getPoints();
        if (points.size() < 2)
        {
            continue;
        }
        const qreal tolerance = 0.5; // pixels
        if (QLineF(points.first(), start).length() < tolerance && QLineF(points.last(), end).length() < tolerance)
        {
            return points;
        }
        if (QLineF(points.first(), end).length() < tolerance && QLineF(points.last(), start).length() < tolerance)
        {
            std::reverse(points.begin(), points.end());
            return points;
        }
    }
    return {start, end};
}

//---------------------------------------------------------------------------------------------------------------------
SlashSpread::Fixed SlashSpread::fixedFromString(const QString &value)
{
    if (value == QLatin1String("end"))
    {
        return Fixed::End;
    }
    if (value == QLatin1String("both"))
    {
        return Fixed::Both;
    }
    return Fixed::Start;
}

//---------------------------------------------------------------------------------------------------------------------
QString SlashSpread::fixedToString(Fixed value)
{
    switch (value)
    {
        case Fixed::End:
            return QStringLiteral("end");
        case Fixed::Both:
            return QStringLiteral("both");
        case Fixed::Start:
        default:
            return QStringLiteral("start");
    }
}
