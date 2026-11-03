/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vistoolslashspread.h
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

#ifndef VISTOOLSLASHSPREAD_H
#define VISTOOLSLASHSPREAD_H

#include <qcompilerdetection.h>
#include <QGraphicsItem>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "../ifc/ifcdef.h"
#include "../vmisc/def.h"
#include "visoperation.h"
#include "../../../tools/drawTools/operation/slashspread.h"

class VisToolSlashSpread : public VisOperation
{
    Q_OBJECT
public:
    explicit       VisToolSlashSpread(const VContainer *data, QGraphicsItem *parent = nullptr);
    virtual       ~VisToolSlashSpread() = default;

    virtual void   RefreshGeometry() override;

    void           setHingeId(quint32 value)     { m_hinge = value; }
    void           setEdgeStartId(quint32 value) { m_edgeStart = value; }
    void           setEdgeEndId(quint32 value)   { m_edgeEnd = value; }
    void           setSlashes(int value)         { m_slashes = value; }
    void           setPercent(bool value)        { m_percent = value; }
    void           setFixed(SlashSpread::Fixed value) { m_fixed = value; }
    void           setSpread(const QString &expression);

    virtual int    type() const override {return Type;}
    enum           { Type = UserType + static_cast<int>(Vis::ToolSlashSpread)};

private:
    Q_DISABLE_COPY(VisToolSlashSpread)
    quint32                 m_hinge;
    quint32                 m_edgeStart;
    quint32                 m_edgeEnd;
    int                     m_slashes;
    qreal                   m_spread;
    bool                    m_percent;
    SlashSpread::Fixed      m_fixed;
    VScaledEllipse         *m_hingePoint;
    QVector<VScaledLine *>  m_slashLines;
};

#endif // VISTOOLSLASHSPREAD_H
