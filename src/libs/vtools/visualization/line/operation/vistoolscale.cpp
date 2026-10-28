/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vistoolscale.cpp
 **  @brief  Scale operation: scales selected objects relative to an origin
 **          point by independent X and Y factors. Based on the Rotation tool.
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

#include "vistoolscale.h"

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
#include "../../../tools/drawTools/operation/vtoolscale.h"
#include "visoperation.h"

//---------------------------------------------------------------------------------------------------------------------
VisToolScale::VisToolScale(const VContainer *data, QGraphicsItem *parent)
    : VisOperation(data, parent)
    , m_xScale(1.0)
    , m_yScale(1.0)
    , m_point(nullptr)
{
    m_point = InitPoint(supportColor2, this);
}

//---------------------------------------------------------------------------------------------------------------------
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wswitch-default")

void VisToolScale::RefreshGeometry()
{
    if (objects.isEmpty())
    {
        return;
    }

    QPointF origin;

    if (object1Id != NULL_ID)
    {
        origin = static_cast<QPointF>(*Visualization::data->GeometricObject<VPointF>(object1Id));
        DrawPoint(m_point, origin, supportColor2);

        Visualization::toolTip = tr("Scale X = %1%3, Y = %2%3")
                                    .arg(m_xScale).arg(m_yScale).arg(VToolScale::scalePostfix());
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
                const QSharedPointer<VPointF> p = Visualization::data->GeometricObject<VPointF>(id);

                ++iPoint;
                VScaledEllipse *point = GetPoint(static_cast<quint32>(iPoint), supportColor2);
                DrawPoint(point, static_cast<QPointF>(*p), supportColor2);

                ++iPoint;
                point = GetPoint(static_cast<quint32>(iPoint), supportColor);

                if (object1Id != NULL_ID)
                {
                    DrawPoint(point, VPointF::ScalePF(origin, static_cast<QPointF>(*p), m_xScale, m_yScale),
                              supportColor);
                }
                break;
            }
            case GOType::Arc:
                iCurve = addCurve<VArc>(origin, id, iCurve);
                break;
            case GOType::EllipticalArc:
                iCurve = addCurve<VEllipticalArc>(origin, id, iCurve);
                break;
            case GOType::Spline:
                iCurve = addCurve<VSpline>(origin, id, iCurve);
                break;
            case GOType::SplinePath:
                iCurve = addCurve<VSplinePath>(origin, id, iCurve);
                break;
            case GOType::CubicBezier:
                iCurve = addCurve<VCubicBezier>(origin, id, iCurve);
                break;
            case GOType::CubicBezierPath:
                iCurve = addCurve<VCubicBezierPath>(origin, id, iCurve);
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
void VisToolScale::setOriginPointId(quint32 value)
{
    object1Id = value;
}

//---------------------------------------------------------------------------------------------------------------------
void VisToolScale::setXScale(const QString &expression)
{
    m_xScale = FindVal(expression, Visualization::data->DataVariables());
}

//---------------------------------------------------------------------------------------------------------------------
void VisToolScale::setYScale(const QString &expression)
{
    m_yScale = FindVal(expression, Visualization::data->DataVariables());
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
int VisToolScale::addCurve(const QPointF &origin, quint32 id, int i)
{
    const QSharedPointer<Item> curve = Visualization::data->template GeometricObject<Item>(id);

    ++i;
    VCurvePathItem *path = GetCurve(static_cast<quint32>(i), supportColor2);
    DrawPath(path, curve->GetPath(), curve->DirectionArrows(), supportColor2, Qt::SolidLine,
             lineWeight, Qt::RoundCap);

    ++i;
    path = GetCurve(static_cast<quint32>(i), supportColor);
    if (object1Id != NULL_ID)
    {
        const Item scaled = curve->Scale(origin, m_xScale, m_yScale);
        DrawPath(path, scaled.GetPath(), scaled.DirectionArrows(), supportColor, Qt::SolidLine,
                 lineWeight, Qt::RoundCap);
    }

    return i;
}
