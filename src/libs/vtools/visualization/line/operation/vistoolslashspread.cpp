/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vistoolslashspread.cpp
 **  @brief  Preview of slash and spread
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

#include "vistoolslashspread.h"

#include <QLineF>
#include <QScopedPointer>
#include <QSharedPointer>
#include <Qt>

#include "../vgeometry/vabstractcurve.h"
#include "../vgeometry/vgeometrydef.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vpointf.h"
#include "../vpatterndb/vcontainer.h"
#include "../vmisc/vabstractapplication.h"
#include "../../../tools/drawTools/operation/vtoolslashspread.h"
#include "visoperation.h"

//---------------------------------------------------------------------------------------------------------------------
VisToolSlashSpread::VisToolSlashSpread(const VContainer *data, QGraphicsItem *parent)
    : VisOperation(data, parent)
    , m_hinge(NULL_ID)
    , m_edgeStart(NULL_ID)
    , m_edgeEnd(NULL_ID)
    , m_slashes(3)
    , m_spread(0)
    , m_percent(false)
    , m_fixed(SlashSpread::Fixed::Start)
    , m_hingePoint(nullptr)
    , m_slashLines()
{
    m_hingePoint = InitPoint(supportColor2, this);
}

//---------------------------------------------------------------------------------------------------------------------
void VisToolSlashSpread::setSpread(const QString &expression)
{
    m_spread = FindVal(expression, Visualization::data->DataVariables());
}

//---------------------------------------------------------------------------------------------------------------------
void VisToolSlashSpread::RefreshGeometry()
{
    if (objects.isEmpty())
    {
        return;
    }

    for (VScaledLine *line : m_slashLines)
    {
        line->setVisible(false);
    }

    if (m_hinge != NULL_ID)
    {
        DrawPoint(m_hingePoint, static_cast<QPointF>(*Visualization::data->GeometricObject<VPointF>(m_hinge)),
                  supportColor2);
    }

    const bool ready = m_hinge != NULL_ID && m_edgeStart != NULL_ID && m_edgeEnd != NULL_ID
                       && m_edgeStart != m_edgeEnd;
    SlashSpread geometry(QPointF(), {}, 0, 0, false, m_fixed);
    if (ready)
    {
        geometry = VToolSlashSpread::makeSpread(Visualization::data, m_hinge, m_edgeStart, m_edgeEnd, m_slashes,
                                                m_spread, m_percent, m_fixed, objects);
        const QVector<QPointF> ends = geometry.slashEnds();
        while (m_slashLines.size() < ends.size())
        {
            m_slashLines.append(InitItem<VScaledLine>(supportColor2, this));
        }
        for (int i = 0; i < ends.size(); ++i)
        {
            DrawLine(m_slashLines.at(i), QLineF(geometry.hinge(), ends.at(i)), supportColor2, lineWeight,
                     Qt::DashLine);
        }
        Visualization::toolTip = tr("Slash and spread: %1 slashes, the edge gets %2 longer")
                .arg(m_slashes).arg(qApp->fromPixel(geometry.totalSpread()), 0, 'f', 2);
    }

    int iPoint = -1;
    int iCurve = -1;
    for (const quint32 id : objects)
    {
        const QSharedPointer<VGObject> obj = Visualization::data->GetGObject(id);
        if (static_cast<GOType>(obj->getType()) == GOType::Point)
        {
            const QSharedPointer<VPointF> point = Visualization::data->GeometricObject<VPointF>(id);
            ++iPoint;
            DrawPoint(GetPoint(static_cast<quint32>(iPoint), supportColor2), static_cast<QPointF>(*point),
                      supportColor2);
            ++iPoint;
            VScaledEllipse *item = GetPoint(static_cast<quint32>(iPoint), supportColor);
            if (ready && geometry.isValid())
            {
                DrawPoint(item, geometry.map(static_cast<QPointF>(*point)), supportColor);
            }
            continue;
        }

        const QSharedPointer<VAbstractCurve> curve = Visualization::data->GeometricObject<VAbstractCurve>(id);
        ++iCurve;
        DrawPath(GetCurve(static_cast<quint32>(iCurve), supportColor2), curve->GetPath(), curve->DirectionArrows(),
                 supportColor2, Qt::SolidLine, lineWeight, Qt::RoundCap);
        ++iCurve;
        VCurvePathItem *path = GetCurve(static_cast<quint32>(iCurve), supportColor);
        if (ready && geometry.isValid())
        {
            QScopedPointer<VAbstractCurve> opened(geometry.opened(curve, QString()));
            DrawPath(path, opened->GetPath(), opened->DirectionArrows(), supportColor, Qt::SolidLine, lineWeight,
                     Qt::RoundCap);
        }
    }
}
