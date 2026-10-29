/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolalign.h
 **  @brief  Align operation: moves a group so a point lands on a target point
 **          and rotates it so a line of the group follows a target line.
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

#ifndef VTOOLALIGN_H
#define VTOOLALIGN_H

#include <qcompilerdetection.h>
#include <QMetaObject>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "vabstractoperation.h"
#include "../vgeometry/vgeometrydef.h"
#include "../vmisc/def.h"
#include "../ifc/xml/vabstractpattern.h"

template <class T> class QSharedPointer;
class VFormula;

/**
 * @brief The AlignTransform struct describes the rigid movement of the Align tool: a move of length/angle followed
 * by a rotation around the target point.
 */
struct AlignTransform
{
    qreal   length {0};    // in pixels
    qreal   angle {0};     // direction of the move, degrees
    qreal   rotation {0};  // rotation after the move, degrees
    QPointF origin;        // rotation origin (the target point)
};

/**
 * @brief The VToolAlign class moves a group of objects so that point P lands on point T, then rotates the group
 * around T so that the direction P->A follows the direction T->B (plus an optional extra angle).
 * The group is not deformed.
 */
class VToolAlign : public VAbstractOperation
{
    Q_OBJECT
public:
    virtual               ~VToolAlign() Q_DECL_EQ_DEFAULT;
    virtual void           setDialog() override;
    static VToolAlign     *Create(QSharedPointer<DialogTool> dialog, VMainGraphicsScene *scene,
                                  VAbstractPattern *doc, VContainer *data);

    static VToolAlign     *Create(const quint32 _id, quint32 sourcePoint, quint32 sourceDirection,
                                  quint32 targetPoint, quint32 targetDirection, QString &angle,
                                  const QString &suffix, const QVector<SourceItem> &source,
                                  const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                                  VAbstractPattern *doc, VContainer *data, const Document &parse,
                                  const Source &typeCreation);

    static const QString   ToolType;
    static const QString   AttrSourcePoint;
    static const QString   AttrSourceDirection;
    static const QString   AttrTargetPoint;
    static const QString   AttrTargetDirection;

    static AlignTransform  computeTransform(const QPointF &p, const QPointF &a, const QPointF &t, const QPointF &b,
                                            qreal extraAngle);

    virtual int            type() const override {return Type;}
    enum                   { Type = UserType + static_cast<int>(Tool::Align)};

    quint32                getSourcePointId() const;
    void                   setSourcePointId(quint32 value);
    quint32                getSourceDirectionId() const;
    void                   setSourceDirectionId(quint32 value);
    quint32                getTargetPointId() const;
    void                   setTargetPointId(quint32 value);
    quint32                getTargetDirectionId() const;
    void                   setTargetDirectionId(quint32 value);

    QString                pointName(quint32 id) const;

    VFormula               getFormulaAngle() const;
    void                   setFormulaAngle(const VFormula &value);

    virtual void           ShowVisualization(bool show) override;

protected slots:
    virtual void           showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id=NULL_ID) override;

protected:
    virtual void           SetVisualization() override;
    virtual void           SaveDialog(QDomElement &domElement) override;
    virtual void           ReadToolAttributes(const QDomElement &domElement) override;
    virtual void           SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj) override;
    virtual QString        makeToolTip() const override;

private:
    Q_DISABLE_COPY(VToolAlign)
    quint32                m_sourcePointId;
    quint32                m_sourceDirectionId;
    quint32                m_targetPointId;
    quint32                m_targetDirectionId;
    QString                m_formulaAngle;

                           VToolAlign(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 sourcePoint,
                                      quint32 sourceDirection, quint32 targetPoint, quint32 targetDirection,
                                      const QString &angle, const QString &suffix, const QVector<SourceItem> &source,
                                      const QVector<DestinationItem> &destination, const Source &typeCreation,
                                      QGraphicsItem *parent = nullptr);

    void                   setPointOption(quint32 &member, quint32 value);

    static DestinationItem createPoint(quint32 idTool, quint32 idItem, const AlignTransform &t,
                                       const QString &suffix, VContainer *data);
    template <class Item>
    static DestinationItem createItem(quint32 idTool, quint32 idItem, const AlignTransform &t,
                                      const QString &suffix, VContainer *data);
    static void            updatePoint(quint32 idTool, quint32 idItem, const AlignTransform &t,
                                       const QString &suffix, VContainer *data, const DestinationItem &item);
    template <class Item>
    static void            updateItem(quint32 idTool, quint32 idItem, const AlignTransform &t,
                                      const QString &suffix, VContainer *data, quint32 id);
    template <class Item>
    static void            registerItem(VContainer *data, quint32 id);
};

#endif // VTOOLALIGN_H
