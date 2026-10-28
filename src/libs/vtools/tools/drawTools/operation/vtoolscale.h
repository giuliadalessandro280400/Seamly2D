/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolscale.h
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

#ifndef VTOOLSCALE_H
#define VTOOLSCALE_H

#include <qcompilerdetection.h>
#include <QMap>
#include <QMetaObject>
#include <QObject>
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
 * @brief The VToolScale class scales a set of objects relative to an origin point.
 *
 * Horizontal (X) and vertical (Y) factors are independent formulas, so the operation follows size changes.
 * A factor of 1 leaves that direction unchanged, 1.1 makes it 10% longer, a negative factor also mirrors.
 */
class VToolScale : public VAbstractOperation
{
    Q_OBJECT
public:
    virtual               ~VToolScale() Q_DECL_EQ_DEFAULT;
    virtual void           setDialog() override;
    static VToolScale     *Create(QSharedPointer<DialogTool> dialog, VMainGraphicsScene *scene,
                                  VAbstractPattern *doc, VContainer *data);

    static VToolScale     *Create(const quint32 _id, const quint32 &origin, QString &xScale, QString &yScale,
                                  const QString &suffix, const QVector<SourceItem> &source,
                                  const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                                  VAbstractPattern *doc, VContainer *data, const Document &parse,
                                  const Source &typeCreation);

    static const QString   ToolType;
    static QString         scalePostfix();

    virtual int            type() const override {return Type;}
    enum                   { Type = UserType + static_cast<int>(Tool::Scale)};

    QString                getOriginPointName() const;
    quint32                getOriginPointId() const;
    void                   setOriginPointId(const quint32 &value);

    VFormula               getFormulaXScale() const;
    void                   setFormulaXScale(const VFormula &value);

    VFormula               getFormulaYScale() const;
    void                   setFormulaYScale(const VFormula &value);

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
    Q_DISABLE_COPY(VToolScale)
    quint32                m_originPointId;
    QString                m_formulaXScale;
    QString                m_formulaYScale;

                           VToolScale(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 originPointId,
                                      const QString &xScale, const QString &yScale, const QString &suffix,
                                      const QVector<SourceItem> &source, const QVector<DestinationItem> &destination,
                                      const Source &typeCreation, QGraphicsItem *parent = nullptr);

    static DestinationItem createPoint(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                       const QString &suffix, VContainer *data);
    template <class Item>
    static DestinationItem createItem(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                      const QString &suffix, VContainer *data);
    template <class Item>
    static DestinationItem createArc(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                     const QString &suffix, VContainer *data);
    template <class Item>
    static DestinationItem createCurve(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                       const QString &suffix, VContainer *data);
    template <class Item>
    static DestinationItem createCurveWithSegments(quint32 idTool, quint32 idItem, const QPointF &origin,
                                                   qreal sx, qreal sy, const QString &suffix, VContainer *data);

    static void updatePoint(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                            const QString &suffix, VContainer *data, const DestinationItem &item);
    template <class Item>
    static void updateItem(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                           const QString &suffix, VContainer *data, quint32 id);
    template <class Item>
    static void updateArc(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                          const QString &suffix, VContainer *data, quint32 id);
    template <class Item>
    static void updateCurve(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                            const QString &suffix, VContainer *data, quint32 id);
    template <class Item>
    static void updateCurveWithSegments(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                        const QString &suffix, VContainer *data, quint32 id);
};

#endif // VTOOLSCALE_H
