/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vistoolscale.h
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

#ifndef VISTOOLSCALE_H
#define VISTOOLSCALE_H

#include <qcompilerdetection.h>
#include <QGraphicsItem>
#include <QMetaObject>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QtGlobal>

#include "../ifc/ifcdef.h"
#include "../vmisc/def.h"
#include "visoperation.h"

class VisToolScale : public VisOperation
{
    Q_OBJECT
public:
    explicit       VisToolScale(const VContainer *data, QGraphicsItem *parent = nullptr);
    virtual       ~VisToolScale() = default;

    virtual void   RefreshGeometry() override;

    void           setOriginPointId(quint32 value);
    void           setXScale(const QString &expression);
    void           setYScale(const QString &expression);

    virtual int    type() const override {return Type;}
    enum           { Type = UserType + static_cast<int>(Vis::ToolScale)};

private:
    Q_DISABLE_COPY(VisToolScale)
    qreal           m_xScale;
    qreal           m_yScale;
    VScaledEllipse *m_point;

    template <class Item>
    int addCurve(const QPointF &origin, quint32 id, int i);
};

#endif // VISTOOLSCALE_H
