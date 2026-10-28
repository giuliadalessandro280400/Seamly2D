/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolscale.cpp
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

#include "vtoolscale.h"

#include <limits.h>
#include <QColor>
#include <QDomNode>
#include <QDomNodeList>
#include <QPoint>
#include <QSharedPointer>
#include <QString>
#include <QUndoStack>
#include <new>

#include "../../../dialogs/tools/dialogtool.h"
#include "../../../dialogs/tools/dialogscale.h"
#include "../../../visualization/line/operation/vistoolscale.h"
#include "../../../visualization/visualization.h"
#include "../vgeometry/vabstractcurve.h"
#include "../vgeometry/varc.h"
#include "../vgeometry/vellipticalarc.h"
#include "../vgeometry/vcubicbezier.h"
#include "../vgeometry/vcubicbezierpath.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vpointf.h"
#include "../vgeometry/vspline.h"
#include "../vgeometry/vsplinepath.h"
#include "../vpatterndb/vtranslatevars.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"
#include "../vmisc/diagnostic.h"
#include "../vmisc/logging.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vformula.h"
#include "../ifc/ifcdef.h"
#include "../ifc/exception/vexception.h"
#include "../vwidgets/vabstractsimple.h"
#include "../vwidgets/vmaingraphicsscene.h"
#include "../../vabstracttool.h"
#include "../../vdatatool.h"
#include "../vdrawtool.h"

const QString VToolScale::ToolType = QStringLiteral("scale");

//---------------------------------------------------------------------------------------------------------------------
QString VToolScale::scalePostfix()
{
    return QString(QChar(0x00D7)); // multiplication sign
}

//---------------------------------------------------------------------------------------------------------------------
VToolScale::VToolScale(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 originPointId,
                       const QString &xScale, const QString &yScale, const QString &suffix,
                       const QVector<SourceItem> &source, const QVector<DestinationItem> &destination,
                       const Source &typeCreation, QGraphicsItem *parent)
    : VAbstractOperation(doc, data, id, suffix, source, destination, parent)
    , m_originPointId(originPointId)
    , m_formulaXScale(xScale)
    , m_formulaYScale(yScale)
{
    InitOperatedObjects();
    ToolCreation(typeCreation);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::setDialog()
{
    SCASSERT(not m_dialog.isNull())
    QSharedPointer<DialogScale> dialogTool = m_dialog.objectCast<DialogScale>();
    SCASSERT(not dialogTool.isNull())
    dialogTool->setOriginPointId(m_originPointId);
    dialogTool->setXScale(m_formulaXScale);
    dialogTool->setYScale(m_formulaYScale);
    dialogTool->setSuffix(suffix);
}

//---------------------------------------------------------------------------------------------------------------------
VToolScale *VToolScale::Create(QSharedPointer<DialogTool> dialog, VMainGraphicsScene *scene,
                               VAbstractPattern *doc, VContainer *data)
{
    SCASSERT(not dialog.isNull())
    QSharedPointer<DialogScale> dialogTool = dialog.objectCast<DialogScale>();
    SCASSERT(not dialogTool.isNull())
    const quint32 originPointId = dialogTool->getOriginPointId();
    QString xScale = dialogTool->getXScale();
    QString yScale = dialogTool->getYScale();
    const QString suffix = dialogTool->getSuffix();
    const QVector<SourceItem> source = dialogTool->getSourceObjects();

    VToolScale* operation = Create(0, originPointId, xScale, yScale, suffix, source, QVector<DestinationItem>(),
                                   scene, doc, data, Document::FullParse, Source::FromGui);
    if (operation != nullptr)
    {
        operation->m_dialog = dialogTool;
    }
    return operation;
}

//---------------------------------------------------------------------------------------------------------------------
VToolScale *VToolScale::Create(const quint32 _id, const quint32 &origin, QString &xScale, QString &yScale,
                               const QString &suffix, const QVector<SourceItem> &source,
                               const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                               VAbstractPattern *doc, VContainer *data, const Document &parse,
                               const Source &typeCreation)
{
    const qreal sx = CheckFormula(_id, xScale, data);
    const qreal sy = CheckFormula(_id, yScale, data);

    const auto originPoint = *data->GeometricObject<VPointF>(origin);
    const QPointF oPoint = static_cast<QPointF>(originPoint);

    QVector<DestinationItem> dest = destination;

    quint32 id = _id;
    if (typeCreation == Source::FromGui)
    {
        dest.clear();// Try to avoid mistake, value must be empty

        id = VContainer::getNextId();//Just reserve id for tool

        for (int i = 0; i < source.size(); ++i)
        {
            const SourceItem item = source.at(i);
            const quint32 objectId = item.id;
            const QSharedPointer<VGObject> obj = data->GetGObject(objectId);

            // This check helps to find missed objects in the switch
            Q_STATIC_ASSERT_X(static_cast<int>(GOType::Unknown) == 7, "Not all objects were handled.");

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wswitch-default")
            switch(static_cast<GOType>(obj->getType()))
            {
                case GOType::Point:
                    dest.append(createPoint(id, objectId, oPoint, sx, sy, suffix, data));
                    break;
                case GOType::Arc:
                    dest.append(createArc<VArc>(id, objectId, oPoint, sx, sy, suffix, data));
                    break;
                case GOType::EllipticalArc:
                    dest.append(createArc<VEllipticalArc>(id, objectId, oPoint, sx, sy, suffix, data));
                    break;
                case GOType::Spline:
                    dest.append(createCurve<VSpline>(id, objectId, oPoint, sx, sy, suffix, data));
                    break;
                case GOType::SplinePath:
                    dest.append(createCurveWithSegments<VSplinePath>(id, objectId, oPoint, sx, sy, suffix, data));
                    break;
                case GOType::CubicBezier:
                    dest.append(createCurve<VCubicBezier>(id, objectId, oPoint, sx, sy, suffix, data));
                    break;
                case GOType::CubicBezierPath:
                    dest.append(createCurveWithSegments<VCubicBezierPath>(id, objectId, oPoint, sx, sy, suffix,
                                                                          data));
                    break;
                case GOType::Unknown:
                case GOType::Curve:
                case GOType::Path:
                case GOType::AllCurves:
                default:
                    break;
            }
QT_WARNING_POP
        }
    }
    else
    {
        for (int i = 0; i < source.size(); ++i)
        {
            const SourceItem item = source.at(i);
            const quint32 objectId = item.id;
            const QSharedPointer<VGObject> obj = data->GetGObject(objectId);

            // This check helps to find missed objects in the switch
            Q_STATIC_ASSERT_X(static_cast<int>(GOType::Unknown) == 7, "Not all objects were handled.");

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wswitch-default")
            switch(static_cast<GOType>(obj->getType()))
            {
                case GOType::Point:
                    updatePoint(id, objectId, oPoint, sx, sy, suffix, data, dest.at(i));
                    break;
                case GOType::Arc:
                    updateArc<VArc>(id, objectId, oPoint, sx, sy, suffix, data, dest.at(i).id);
                    break;
                case GOType::EllipticalArc:
                    updateArc<VEllipticalArc>(id, objectId, oPoint, sx, sy, suffix, data, dest.at(i).id);
                    break;
                case GOType::Spline:
                    updateCurve<VSpline>(id, objectId, oPoint, sx, sy, suffix, data, dest.at(i).id);
                    break;
                case GOType::SplinePath:
                    updateCurveWithSegments<VSplinePath>(id, objectId, oPoint, sx, sy, suffix, data,
                                                         dest.at(i).id);
                    break;
                case GOType::CubicBezier:
                    updateCurve<VCubicBezier>(id, objectId, oPoint, sx, sy, suffix, data, dest.at(i).id);
                    break;
                case GOType::CubicBezierPath:
                    updateCurveWithSegments<VCubicBezierPath>(id, objectId, oPoint, sx, sy, suffix, data,
                                                              dest.at(i).id);
                    break;
                case GOType::Unknown:
                case GOType::Curve:
                case GOType::Path:
                case GOType::AllCurves:
                    break;
            }
QT_WARNING_POP
        }
        if (parse != Document::FullParse)
        {
            doc->UpdateToolData(id, data);
        }
    }

    if (parse == Document::FullParse)
    {
        VDrawTool::AddRecord(id, Tool::Scale, doc);
        VToolScale *tool = new VToolScale(doc, data, id, origin, xScale, yScale, suffix, source, dest, typeCreation);
        scene->addItem(tool);
        initOperationToolConnections(scene, tool);
        VAbstractPattern::AddTool(id, tool);
        doc->IncrementReferens(originPoint.getIdTool());
        for (int i = 0; i < source.size(); ++i)
        {
            const SourceItem item = source.at(i);
            doc->IncrementReferens(data->GetGObject(item.id)->getIdTool());
        }
        return tool;
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolScale::getOriginPointName() const
{
    return VAbstractTool::data.GetGObject(m_originPointId)->name();
}

//---------------------------------------------------------------------------------------------------------------------
quint32 VToolScale::getOriginPointId() const
{
    return m_originPointId;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::setOriginPointId(const quint32 &value)
{
    if (value != NULL_ID)
    {
        m_originPointId = value;

        QSharedPointer<VGObject> obj = VAbstractTool::data.GetFakeGObject(m_id);
        SaveOption(obj);
    }
}

//---------------------------------------------------------------------------------------------------------------------
VFormula VToolScale::getFormulaXScale() const
{
    VFormula formula(m_formulaXScale, getData());
    formula.setCheckZero(true);
    formula.setToolId(m_id);
    formula.setPostfix(scalePostfix());
    return formula;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::setFormulaXScale(const VFormula &value)
{
    if (value.error() == false)
    {
        m_formulaXScale = value.GetFormula(FormulaType::FromUser);

        QSharedPointer<VGObject> obj = VContainer::GetFakeGObject(m_id);
        SaveOption(obj);
    }
}

//---------------------------------------------------------------------------------------------------------------------
VFormula VToolScale::getFormulaYScale() const
{
    VFormula formula(m_formulaYScale, getData());
    formula.setCheckZero(true);
    formula.setToolId(m_id);
    formula.setPostfix(scalePostfix());
    return formula;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::setFormulaYScale(const VFormula &value)
{
    if (value.error() == false)
    {
        m_formulaYScale = value.GetFormula(FormulaType::FromUser);

        QSharedPointer<VGObject> obj = VContainer::GetFakeGObject(m_id);
        SaveOption(obj);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::ShowVisualization(bool show)
{
    ShowToolVisualization<VisToolScale>(show);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::SetVisualization()
{
    if (not vis.isNull())
    {
        VisToolScale *visual = qobject_cast<VisToolScale *>(vis);
        SCASSERT(visual != nullptr)

        const bool separator = qApp->Settings()->getOsSeparator();
        visual->setObjects(sourceToObjects(source));
        visual->setOriginPointId(m_originPointId);
        visual->setXScale(qApp->translateVariables()->FormulaToUser(m_formulaXScale, separator));
        visual->setYScale(qApp->translateVariables()->FormulaToUser(m_formulaYScale, separator));
        visual->RefreshGeometry();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::SaveDialog(QDomElement &domElement)
{
    SCASSERT(not m_dialog.isNull())
    QSharedPointer<DialogScale> dialogTool = m_dialog.objectCast<DialogScale>();
    SCASSERT(not dialogTool.isNull())

    doc->SetAttribute(domElement, AttrCenter, QString().setNum(dialogTool->getOriginPointId()));
    doc->SetAttribute(domElement, VAbstractPattern::AttrXScale, dialogTool->getXScale());
    doc->SetAttribute(domElement, VAbstractPattern::AttrYScale, dialogTool->getYScale());
    doc->SetAttribute(domElement, AttrSuffix, dialogTool->getSuffix());
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::ReadToolAttributes(const QDomElement &domElement)
{
    m_originPointId = doc->GetParametrUInt(domElement, AttrCenter, NULL_ID_STR);
    m_formulaXScale = doc->GetParametrString(domElement, VAbstractPattern::AttrXScale, "1");
    m_formulaYScale = doc->GetParametrString(domElement, VAbstractPattern::AttrYScale, "1");
    suffix = doc->GetParametrString(domElement, AttrSuffix);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj)
{
    VDrawTool::SaveOptions(tag, obj);

    doc->SetAttribute(tag, AttrType, ToolType);
    doc->SetAttribute(tag, AttrCenter, QString().setNum(m_originPointId));
    doc->SetAttribute(tag, VAbstractPattern::AttrXScale, m_formulaXScale);
    doc->SetAttribute(tag, VAbstractPattern::AttrYScale, m_formulaYScale);
    doc->SetAttribute(tag, AttrSuffix, suffix);

    SaveSourceDestination(tag);
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolScale::makeToolTip() const
{
    const QString toolTipStr = QString("<tr> <td><b>%1:</b> %2</td> </tr>"
                                       "<tr> <td><b>%3:</b> %4%7</td> </tr>"
                                       "<tr> <td><b>%5:</b> %6%7</td> </tr>")
                                       .arg(tr("  Origin point"))
                                       .arg(getOriginPointName())
                                       .arg(tr("Horizontal scale"))
                                       .arg(getFormulaXScale().getDoubleValue())
                                       .arg(tr("Vertical scale"))
                                       .arg(getFormulaYScale().getDoubleValue())
                                       .arg(scalePostfix());
    return toolTipStr;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id)
{
    try
    {
        ContextMenu<DialogScale>(event, id);
    }
    catch(const VExceptionToolWasDeleted &error)
    {
        Q_UNUSED(error)
        return;//Leave this method immediately!!!
    }
}

//---------------------------------------------------------------------------------------------------------------------
DestinationItem VToolScale::createPoint(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                        const QString &suffix, VContainer *data)
{
    const QSharedPointer<VPointF> point = data->GeometricObject<VPointF>(idItem);
    VPointF scaled = point->Scale(origin, sx, sy, suffix);
    scaled.setIdObject(idTool);

    DestinationItem item;
    item.mx = scaled.mx();
    item.my = scaled.my();
    item.showPointName = scaled.isShowPointName();
    item.id = data->AddGObject(new VPointF(scaled));
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
DestinationItem VToolScale::createItem(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                       const QString &suffix, VContainer *data)
{
    const QSharedPointer<Item> i = data->GeometricObject<Item>(idItem);
    Item scaled = i->Scale(origin, sx, sy, suffix);
    scaled.setIdObject(idTool);

    DestinationItem item;
    item.mx = INT_MAX;
    item.my = INT_MAX;
    item.id = data->AddGObject(new Item(scaled));
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
DestinationItem VToolScale::createArc(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                      const QString &suffix, VContainer *data)
{
    const DestinationItem item = createItem<Item>(idTool, idItem, origin, sx, sy, suffix, data);
    data->AddArc(data->GeometricObject<Item>(item.id), item.id);
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
DestinationItem VToolScale::createCurve(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                        const QString &suffix, VContainer *data)
{
    const DestinationItem item = createItem<Item>(idTool, idItem, origin, sx, sy, suffix, data);
    data->AddSpline(data->GeometricObject<Item>(item.id), item.id);
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
DestinationItem VToolScale::createCurveWithSegments(quint32 idTool, quint32 idItem, const QPointF &origin,
                                                    qreal sx, qreal sy, const QString &suffix, VContainer *data)
{
    const DestinationItem item = createItem<Item>(idTool, idItem, origin, sx, sy, suffix, data);
    data->AddCurveWithSegments(data->GeometricObject<Item>(item.id), item.id);
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolScale::updatePoint(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                             const QString &suffix, VContainer *data, const DestinationItem &item)
{
    const QSharedPointer<VPointF> point = data->GeometricObject<VPointF>(idItem);
    VPointF scaled = point->Scale(origin, sx, sy, suffix);
    scaled.setIdObject(idTool);
    scaled.setMx(item.mx);
    scaled.setMy(item.my);
    scaled.setShowPointName(item.showPointName);
    data->UpdateGObject(item.id, new VPointF(scaled));
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
void VToolScale::updateItem(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                            const QString &suffix, VContainer *data, quint32 id)
{
    const QSharedPointer<Item> i = data->GeometricObject<Item>(idItem);
    Item scaled = i->Scale(origin, sx, sy, suffix);
    scaled.setIdObject(idTool);
    data->UpdateGObject(id, new Item(scaled));
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
void VToolScale::updateArc(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                           const QString &suffix, VContainer *data, quint32 id)
{
    updateItem<Item>(idTool, idItem, origin, sx, sy, suffix, data, id);
    data->AddArc(data->GeometricObject<Item>(id), id);
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
void VToolScale::updateCurve(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                             const QString &suffix, VContainer *data, quint32 id)
{
    updateItem<Item>(idTool, idItem, origin, sx, sy, suffix, data, id);
    data->AddSpline(data->GeometricObject<Item>(id), id);
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
void VToolScale::updateCurveWithSegments(quint32 idTool, quint32 idItem, const QPointF &origin, qreal sx, qreal sy,
                                         const QString &suffix, VContainer *data, quint32 id)
{
    updateItem<Item>(idTool, idItem, origin, sx, sy, suffix, data, id);
    data->AddCurveWithSegments(data->GeometricObject<Item>(id), id);
}
