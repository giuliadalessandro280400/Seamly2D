/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vistoolalign.h
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

#ifndef VISTOOLALIGN_H
#define VISTOOLALIGN_H

#include <qcompilerdetection.h>
#include <QGraphicsItem>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QtGlobal>

#include "../ifc/ifcdef.h"
#include "../vmisc/def.h"
#include "visoperation.h"

struct AlignTransform;

class VisToolAlign : public VisOperation
{
    Q_OBJECT
public:
    explicit       VisToolAlign(const VContainer *data, QGraphicsItem *parent = nullptr);
    virtual       ~VisToolAlign() = default;

    virtual void   RefreshGeometry() override;

    void           setSourcePointId(quint32 value);
    void           setSourceDirectionId(quint32 value);
    void           setTargetPointId(quint32 value);
    void           setTargetDirectionId(quint32 value);
    void           setAngle(const QString &expression);

    virtual int    type() const override {return Type;}
    enum           { Type = UserType + static_cast<int>(Vis::ToolAlign)};

private:
    Q_DISABLE_COPY(VisToolAlign)
    quint32         m_sourcePointId;
    quint32         m_sourceDirectionId;
    quint32         m_targetPointId;
    quint32         m_targetDirectionId;
    qreal           m_angle;
    VScaledEllipse *m_sourcePoint;
    VScaledEllipse *m_targetPoint;
    VScaledLine    *m_sourceLine;
    VScaledLine    *m_targetLine;

    template <class Item>
    int addCurve(const AlignTransform &t, bool ready, quint32 id, int i);
};

#endif // VISTOOLALIGN_H
