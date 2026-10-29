/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vistoolalign.cpp
 **  @brief  Preview of the Align operation.
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

#include "vistoolalign.h"

#include <QLineF>
#include <QPainterPath>
#include <QSharedPointer>
#include <Qt>

#include "../vmisc/diagnostic.h"
#include "../vgeometry/vabstractcurve.h"
#include "../vgeometry/varc.h"
#include "../vgeometry/vcubicbezier.h"
#include "../vgeometry/vcubicbezierpath.h"
#include "../vgeometry/vellipticalarc.h"
#include "../vgeometry/vgeometrydef.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vpointf.h"
#include "../vgeometry/vspline.h"
#include "../vgeometry/vsplinepath.h"
#include "../vpatterndb/vcontainer.h"
#include "../../../tools/drawTools/operation/vtoolalign.h"
#include "visoperation.h"

//---------------------------------------------------------------------------------------------------------------------
VisToolAlign::VisToolAlign(const VContainer *data, QGraphicsItem *parent)
    : VisOperation(data, parent)
    , m_sourcePointId(NULL_ID)
    , m_sourceDirectionId(NULL_ID)
    , m_targetPointId(NULL_ID)
    , m_targetDirectionId(NULL_ID)
    , m_angle(0)
    , m_sourcePoint(nullptr)
    , m_targetPoint(nullptr)
    , m_sourceLine(nullptr)
    , m_targetLine(nullptr)
{
    m_sourcePoint = InitPoint(supportColor2, this);
    m_targetPoint = InitPoint(supportColor2, this);
    m_sourceLine = InitItem<VScaledLine>(supportColor2, this);
    m_targetLine = InitItem<VScaledLine>(supportColor2, this);
}

//---------------------------------------------------------------------------------------------------------------------
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wswitch-default")

void VisToolAlign::RefreshGeometry()
{
    if (objects.isEmpty())
    {
        return;
    }

    auto pointAt = [this](quint32 id) {
        return static_cast<QPointF>(*Visualization::data->GeometricObject<VPointF>(id));
    };

    QPointF p, a, t, b;
    if (m_sourcePointId != NULL_ID)
    {
        p = pointAt(m_sourcePointId);
        DrawPoint(m_sourcePoint, p, supportColor2);
    }
    if (m_sourceDirectionId != NULL_ID)
    {
        a = pointAt(m_sourceDirectionId);
        DrawLine(m_sourceLine, QLineF(p, a), supportColor2, lineWeight, Qt::DashLine);
    }
    if (m_targetPointId != NULL_ID)
    {
        t = pointAt(m_targetPointId);
        DrawPoint(m_targetPoint, t, supportColor2);

        // While the last point is not chosen yet the mouse gives the direction.
        b = m_targetDirectionId != NULL_ID ? pointAt(m_targetDirectionId) : Visualization::scenePos;
        DrawLine(m_targetLine, QLineF(t, b), supportColor2, lineWeight, Qt::DashLine);
    }

    const bool ready = m_sourcePointId != NULL_ID && m_sourceDirectionId != NULL_ID && m_targetPointId != NULL_ID;
    const AlignTransform transform = VToolAlign::computeTransform(p, a, t, b, m_angle);

    if (ready)
    {
        Visualization::toolTip = tr("Align: rotation %1°").arg(transform.rotation, 0, 'f', 2);
    }

    int iPoint = -1;
    int iCurve = -1;
    for (int i = 0; i < objects.size(); ++i)
    {
        const quint32 id = objects.at(i);
        const QSharedPointer<VGObject> obj = Visualization::data->GetGObject(id);

        // This check helps to find missed objects in the switch
        Q_STATIC_ASSERT_X(static_cast<int>(GOType::Unknown) == 7, "Not all objects was handled.");

        switch(static_cast<GOType>(obj->getType()))
        {
            case GOType::Point:
            {
                const QSharedPointer<VPointF> point = Visualization::data->GeometricObject<VPointF>(id);

                ++iPoint;
                VScaledEllipse *item = GetPoint(static_cast<quint32>(iPoint), supportColor2);
                DrawPoint(item, static_cast<QPointF>(*point), supportColor2);

                ++iPoint;
                item = GetPoint(static_cast<quint32>(iPoint), supportColor);
                if (ready)
                {
                    const VPointF moved = point->Move(transform.length, transform.angle)
                                               .Rotate(transform.origin, transform.rotation);
                    DrawPoint(item, static_cast<QPointF>(moved), supportColor);
                }
                break;
            }
            case GOType::Arc:
                iCurve = addCurve<VArc>(transform, ready, id, iCurve);
                break;
            case GOType::EllipticalArc:
                iCurve = addCurve<VEllipticalArc>(transform, ready, id, iCurve);
                break;
            case GOType::Spline:
                iCurve = addCurve<VSpline>(transform, ready, id, iCurve);
                break;
            case GOType::SplinePath:
                iCurve = addCurve<VSplinePath>(transform, ready, id, iCurve);
                break;
            case GOType::CubicBezier:
                iCurve = addCurve<VCubicBezier>(transform, ready, id, iCurve);
                break;
            case GOType::CubicBezierPath:
                iCurve = addCurve<VCubicBezierPath>(transform, ready, id, iCurve);
                break;
            case GOType::Unknown:
            case GOType::Curve:
            case GOType::Path:
            case GOType::AllCurves:
            default:
                break;
        }
    }
}
QT_WARNING_POP

//---------------------------------------------------------------------------------------------------------------------
void VisToolAlign::setSourcePointId(quint32 value)     { m_sourcePointId = value; }
void VisToolAlign::setSourceDirectionId(quint32 value) { m_sourceDirectionId = value; }
void VisToolAlign::setTargetPointId(quint32 value)     { m_targetPointId = value; }
void VisToolAlign::setTargetDirectionId(quint32 value) { m_targetDirectionId = value; }

//---------------------------------------------------------------------------------------------------------------------
void VisToolAlign::setAngle(const QString &expression)
{
    m_angle = FindVal(expression, Visualization::data->DataVariables());
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
int VisToolAlign::addCurve(const AlignTransform &t, bool ready, quint32 id, int i)
{
    const QSharedPointer<Item> curve = Visualization::data->template GeometricObject<Item>(id);

    ++i;
    VCurvePathItem *path = GetCurve(static_cast<quint32>(i), supportColor2);
    DrawPath(path, curve->GetPath(), curve->DirectionArrows(), supportColor2, Qt::SolidLine,
             lineWeight, Qt::RoundCap);

    ++i;
    path = GetCurve(static_cast<quint32>(i), supportColor);
    if (ready)
    {
        const Item moved = curve->Move(t.length, t.angle).Rotate(t.origin, t.rotation);
        DrawPath(path, moved.GetPath(), moved.DirectionArrows(), supportColor, Qt::SolidLine,
                 lineWeight, Qt::RoundCap);
    }

    return i;
}
