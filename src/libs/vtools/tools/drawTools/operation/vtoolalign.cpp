/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolalign.cpp
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

#include "vtoolalign.h"

#include <limits.h>
#include <type_traits>
#include <QDomElement>
#include <QLineF>
#include <QSharedPointer>
#include <QString>

#include "../../../dialogs/tools/dialogtool.h"
#include "../../../dialogs/tools/dialogalign.h"
#include "../../../visualization/line/operation/vistoolalign.h"
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
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vformula.h"
#include "../ifc/ifcdef.h"
#include "../ifc/exception/vexception.h"
#include "../vwidgets/vmaingraphicsscene.h"
#include "../../vabstracttool.h"
#include "../../vdatatool.h"
#include "../vdrawtool.h"

const QString VToolAlign::ToolType            = QStringLiteral("align");
const QString VToolAlign::AttrSourcePoint     = QStringLiteral("sourcePoint");
const QString VToolAlign::AttrSourceDirection = QStringLiteral("sourceDirection");
const QString VToolAlign::AttrTargetPoint     = QStringLiteral("targetPoint");
const QString VToolAlign::AttrTargetDirection = QStringLiteral("targetDirection");

//---------------------------------------------------------------------------------------------------------------------
AlignTransform VToolAlign::computeTransform(const QPointF &p, const QPointF &a, const QPointF &t, const QPointF &b,
                                            qreal extraAngle)
{
    AlignTransform transform;
    const QLineF move(p, t);
    transform.length = move.length();
    transform.angle = move.angle();
    transform.origin = t;

    const QLineF sourceLine(p, a);
    const QLineF targetLine(t, b);
    qreal rotation = extraAngle;
    if (not qFuzzyIsNull(sourceLine.length()) && not qFuzzyIsNull(targetLine.length()))
    {
        rotation += targetLine.angle() - sourceLine.angle();
    }
    transform.rotation = rotation;
    return transform;
}

//---------------------------------------------------------------------------------------------------------------------
VToolAlign::VToolAlign(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 sourcePoint,
                       quint32 sourceDirection, quint32 targetPoint, quint32 targetDirection, const QString &angle,
                       const QString &suffix, const QVector<SourceItem> &source,
                       const QVector<DestinationItem> &destination, const Source &typeCreation, QGraphicsItem *parent)
    : VAbstractOperation(doc, data, id, suffix, source, destination, parent)
    , m_sourcePointId(sourcePoint)
    , m_sourceDirectionId(sourceDirection)
    , m_targetPointId(targetPoint)
    , m_targetDirectionId(targetDirection)
    , m_formulaAngle(angle)
{
    InitOperatedObjects();
    ToolCreation(typeCreation);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::setDialog()
{
    SCASSERT(not m_dialog.isNull())
    QSharedPointer<DialogAlign> dialogTool = m_dialog.objectCast<DialogAlign>();
    SCASSERT(not dialogTool.isNull())
    dialogTool->setSourcePointId(m_sourcePointId);
    dialogTool->setSourceDirectionId(m_sourceDirectionId);
    dialogTool->setTargetPointId(m_targetPointId);
    dialogTool->setTargetDirectionId(m_targetDirectionId);
    dialogTool->setAngle(m_formulaAngle);
    dialogTool->setSuffix(suffix);
}

//---------------------------------------------------------------------------------------------------------------------
VToolAlign *VToolAlign::Create(QSharedPointer<DialogTool> dialog, VMainGraphicsScene *scene,
                               VAbstractPattern *doc, VContainer *data)
{
    SCASSERT(not dialog.isNull())
    QSharedPointer<DialogAlign> dialogTool = dialog.objectCast<DialogAlign>();
    SCASSERT(not dialogTool.isNull())
    QString angle = dialogTool->getAngle();

    VToolAlign* operation = Create(0, dialogTool->getSourcePointId(), dialogTool->getSourceDirectionId(),
                                   dialogTool->getTargetPointId(), dialogTool->getTargetDirectionId(), angle,
                                   dialogTool->getSuffix(), dialogTool->getSourceObjects(),
                                   QVector<DestinationItem>(), scene, doc, data, Document::FullParse,
                                   Source::FromGui);
    if (operation != nullptr)
    {
        operation->m_dialog = dialogTool;
    }
    return operation;
}

//---------------------------------------------------------------------------------------------------------------------
VToolAlign *VToolAlign::Create(const quint32 _id, quint32 sourcePoint, quint32 sourceDirection,
                               quint32 targetPoint, quint32 targetDirection, QString &angle,
                               const QString &suffix, const QVector<SourceItem> &source,
                               const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                               VAbstractPattern *doc, VContainer *data, const Document &parse,
                               const Source &typeCreation)
{
    const qreal extraAngle = CheckFormula(_id, angle, data);

    const QSharedPointer<VPointF> p = data->GeometricObject<VPointF>(sourcePoint);
    const QSharedPointer<VPointF> a = data->GeometricObject<VPointF>(sourceDirection);
    const QSharedPointer<VPointF> t = data->GeometricObject<VPointF>(targetPoint);
    const QSharedPointer<VPointF> b = data->GeometricObject<VPointF>(targetDirection);

    const AlignTransform transform = computeTransform(static_cast<QPointF>(*p), static_cast<QPointF>(*a),
                                                      static_cast<QPointF>(*t), static_cast<QPointF>(*b),
                                                      extraAngle);

    QVector<DestinationItem> dest = destination;

    quint32 id = _id;
    const bool fromGui = typeCreation == Source::FromGui;
    if (fromGui)
    {
        dest.clear();// Try to avoid mistake, value must be empty
        id = VContainer::getNextId();//Just reserve id for tool
    }

    for (int i = 0; i < source.size(); ++i)
    {
        const quint32 objectId = source.at(i).id;
        const QSharedPointer<VGObject> obj = data->GetGObject(objectId);

        // This check helps to find missed objects in the switch
        Q_STATIC_ASSERT_X(static_cast<int>(GOType::Unknown) == 7, "Not all objects were handled.");

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wswitch-default")
        switch(static_cast<GOType>(obj->getType()))
        {
            case GOType::Point:
                if (fromGui)
                {
                    dest.append(createPoint(id, objectId, transform, suffix, data));
                }
                else
                {
                    updatePoint(id, objectId, transform, suffix, data, dest.at(i));
                }
                break;
            case GOType::Arc:
                fromGui ? dest.append(createItem<VArc>(id, objectId, transform, suffix, data))
                        : updateItem<VArc>(id, objectId, transform, suffix, data, dest.at(i).id);
                break;
            case GOType::EllipticalArc:
                fromGui ? dest.append(createItem<VEllipticalArc>(id, objectId, transform, suffix, data))
                        : updateItem<VEllipticalArc>(id, objectId, transform, suffix, data, dest.at(i).id);
                break;
            case GOType::Spline:
                fromGui ? dest.append(createItem<VSpline>(id, objectId, transform, suffix, data))
                        : updateItem<VSpline>(id, objectId, transform, suffix, data, dest.at(i).id);
                break;
            case GOType::SplinePath:
                fromGui ? dest.append(createItem<VSplinePath>(id, objectId, transform, suffix, data))
                        : updateItem<VSplinePath>(id, objectId, transform, suffix, data, dest.at(i).id);
                break;
            case GOType::CubicBezier:
                fromGui ? dest.append(createItem<VCubicBezier>(id, objectId, transform, suffix, data))
                        : updateItem<VCubicBezier>(id, objectId, transform, suffix, data, dest.at(i).id);
                break;
            case GOType::CubicBezierPath:
                fromGui ? dest.append(createItem<VCubicBezierPath>(id, objectId, transform, suffix, data))
                        : updateItem<VCubicBezierPath>(id, objectId, transform, suffix, data, dest.at(i).id);
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

    if (not fromGui && parse != Document::FullParse)
    {
        doc->UpdateToolData(id, data);
    }

    if (parse == Document::FullParse)
    {
        VDrawTool::AddRecord(id, Tool::Align, doc);
        VToolAlign *tool = new VToolAlign(doc, data, id, sourcePoint, sourceDirection, targetPoint, targetDirection,
                                          angle, suffix, source, dest, typeCreation);
        scene->addItem(tool);
        initOperationToolConnections(scene, tool);
        VAbstractPattern::AddTool(id, tool);
        doc->IncrementReferens(p->getIdTool());
        doc->IncrementReferens(a->getIdTool());
        doc->IncrementReferens(t->getIdTool());
        doc->IncrementReferens(b->getIdTool());
        for (int i = 0; i < source.size(); ++i)
        {
            doc->IncrementReferens(data->GetGObject(source.at(i).id)->getIdTool());
        }
        return tool;
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 VToolAlign::getSourcePointId() const     { return m_sourcePointId; }
quint32 VToolAlign::getSourceDirectionId() const { return m_sourceDirectionId; }
quint32 VToolAlign::getTargetPointId() const     { return m_targetPointId; }
quint32 VToolAlign::getTargetDirectionId() const { return m_targetDirectionId; }

void VToolAlign::setSourcePointId(quint32 value)     { setPointOption(m_sourcePointId, value); }
void VToolAlign::setSourceDirectionId(quint32 value) { setPointOption(m_sourceDirectionId, value); }
void VToolAlign::setTargetPointId(quint32 value)     { setPointOption(m_targetPointId, value); }
void VToolAlign::setTargetDirectionId(quint32 value) { setPointOption(m_targetDirectionId, value); }

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::setPointOption(quint32 &member, quint32 value)
{
    if (value != NULL_ID)
    {
        member = value;
        QSharedPointer<VGObject> obj = VAbstractTool::data.GetFakeGObject(m_id);
        SaveOption(obj);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolAlign::pointName(quint32 id) const
{
    return VAbstractTool::data.GetGObject(id)->name();
}

//---------------------------------------------------------------------------------------------------------------------
VFormula VToolAlign::getFormulaAngle() const
{
    VFormula formula(m_formulaAngle, getData());
    formula.setCheckZero(false);
    formula.setToolId(m_id);
    formula.setPostfix(degreeSymbol);
    return formula;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::setFormulaAngle(const VFormula &value)
{
    if (value.error() == false)
    {
        m_formulaAngle = value.GetFormula(FormulaType::FromUser);

        QSharedPointer<VGObject> obj = VContainer::GetFakeGObject(m_id);
        SaveOption(obj);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::ShowVisualization(bool show)
{
    ShowToolVisualization<VisToolAlign>(show);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::SetVisualization()
{
    if (not vis.isNull())
    {
        VisToolAlign *visual = qobject_cast<VisToolAlign *>(vis);
        SCASSERT(visual != nullptr)

        visual->setObjects(sourceToObjects(source));
        visual->setSourcePointId(m_sourcePointId);
        visual->setSourceDirectionId(m_sourceDirectionId);
        visual->setTargetPointId(m_targetPointId);
        visual->setTargetDirectionId(m_targetDirectionId);
        visual->setAngle(qApp->translateVariables()->FormulaToUser(m_formulaAngle,
                                                                   qApp->Settings()->getOsSeparator()));
        visual->RefreshGeometry();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::SaveDialog(QDomElement &domElement)
{
    SCASSERT(not m_dialog.isNull())
    QSharedPointer<DialogAlign> dialogTool = m_dialog.objectCast<DialogAlign>();
    SCASSERT(not dialogTool.isNull())

    doc->SetAttribute(domElement, AttrSourcePoint, QString().setNum(dialogTool->getSourcePointId()));
    doc->SetAttribute(domElement, AttrSourceDirection, QString().setNum(dialogTool->getSourceDirectionId()));
    doc->SetAttribute(domElement, AttrTargetPoint, QString().setNum(dialogTool->getTargetPointId()));
    doc->SetAttribute(domElement, AttrTargetDirection, QString().setNum(dialogTool->getTargetDirectionId()));
    doc->SetAttribute(domElement, AttrAngle, dialogTool->getAngle());
    doc->SetAttribute(domElement, AttrSuffix, dialogTool->getSuffix());
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::ReadToolAttributes(const QDomElement &domElement)
{
    m_sourcePointId     = doc->GetParametrUInt(domElement, AttrSourcePoint, NULL_ID_STR);
    m_sourceDirectionId = doc->GetParametrUInt(domElement, AttrSourceDirection, NULL_ID_STR);
    m_targetPointId     = doc->GetParametrUInt(domElement, AttrTargetPoint, NULL_ID_STR);
    m_targetDirectionId = doc->GetParametrUInt(domElement, AttrTargetDirection, NULL_ID_STR);
    m_formulaAngle      = doc->GetParametrString(domElement, AttrAngle, "0");
    suffix              = doc->GetParametrString(domElement, AttrSuffix);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj)
{
    VDrawTool::SaveOptions(tag, obj);

    doc->SetAttribute(tag, AttrType, ToolType);
    doc->SetAttribute(tag, AttrSourcePoint, QString().setNum(m_sourcePointId));
    doc->SetAttribute(tag, AttrSourceDirection, QString().setNum(m_sourceDirectionId));
    doc->SetAttribute(tag, AttrTargetPoint, QString().setNum(m_targetPointId));
    doc->SetAttribute(tag, AttrTargetDirection, QString().setNum(m_targetDirectionId));
    doc->SetAttribute(tag, AttrAngle, m_formulaAngle);
    doc->SetAttribute(tag, AttrSuffix, suffix);

    SaveSourceDestination(tag);
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolAlign::makeToolTip() const
{
    return QString("<tr> <td><b>%1:</b> %2 &rarr; %3</td> </tr>"
                   "<tr> <td><b>%4:</b> %5-%6 &rarr; %7-%8</td> </tr>"
                   "<tr> <td><b>%9:</b> %10°</td> </tr>")
            .arg(tr("Point"), pointName(m_sourcePointId), pointName(m_targetPointId),
                 tr("Line"), pointName(m_sourcePointId), pointName(m_sourceDirectionId),
                 pointName(m_targetPointId), pointName(m_targetDirectionId), tr("Extra angle"))
            .arg(getFormulaAngle().getDoubleValue());
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id)
{
    try
    {
        ContextMenu<DialogAlign>(event, id);
    }
    catch(const VExceptionToolWasDeleted &error)
    {
        Q_UNUSED(error)
        return;//Leave this method immediately!!!
    }
}

//---------------------------------------------------------------------------------------------------------------------
DestinationItem VToolAlign::createPoint(quint32 idTool, quint32 idItem, const AlignTransform &t,
                                        const QString &suffix, VContainer *data)
{
    const QSharedPointer<VPointF> point = data->GeometricObject<VPointF>(idItem);
    VPointF moved = point->Move(t.length, t.angle, suffix).Rotate(t.origin, t.rotation);
    moved.setIdObject(idTool);

    DestinationItem item;
    item.mx = moved.mx();
    item.my = moved.my();
    item.showPointName = moved.isShowPointName();
    item.id = data->AddGObject(new VPointF(moved));
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
void VToolAlign::registerItem(VContainer *data, quint32 id)
{
    if constexpr (std::is_same_v<Item, VArc> || std::is_same_v<Item, VEllipticalArc>)
    {
        data->AddArc(data->GeometricObject<Item>(id), id);
    }
    else if constexpr (std::is_same_v<Item, VSpline> || std::is_same_v<Item, VCubicBezier>)
    {
        data->AddSpline(data->GeometricObject<Item>(id), id);
    }
    else
    {
        data->AddCurveWithSegments(data->GeometricObject<Item>(id), id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
DestinationItem VToolAlign::createItem(quint32 idTool, quint32 idItem, const AlignTransform &t,
                                       const QString &suffix, VContainer *data)
{
    const QSharedPointer<Item> i = data->GeometricObject<Item>(idItem);
    Item moved = i->Move(t.length, t.angle, suffix).Rotate(t.origin, t.rotation);
    moved.setIdObject(idTool);

    DestinationItem item;
    item.mx = INT_MAX;
    item.my = INT_MAX;
    item.id = data->AddGObject(new Item(moved));
    registerItem<Item>(data, item.id);
    return item;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolAlign::updatePoint(quint32 idTool, quint32 idItem, const AlignTransform &t, const QString &suffix,
                             VContainer *data, const DestinationItem &item)
{
    const QSharedPointer<VPointF> point = data->GeometricObject<VPointF>(idItem);
    VPointF moved = point->Move(t.length, t.angle, suffix).Rotate(t.origin, t.rotation);
    moved.setIdObject(idTool);
    moved.setMx(item.mx);
    moved.setMy(item.my);
    moved.setShowPointName(item.showPointName);
    data->UpdateGObject(item.id, new VPointF(moved));
}

//---------------------------------------------------------------------------------------------------------------------
template <class Item>
void VToolAlign::updateItem(quint32 idTool, quint32 idItem, const AlignTransform &t, const QString &suffix,
                            VContainer *data, quint32 id)
{
    const QSharedPointer<Item> i = data->GeometricObject<Item>(idItem);
    Item moved = i->Move(t.length, t.angle, suffix).Rotate(t.origin, t.rotation);
    moved.setIdObject(idTool);
    data->UpdateGObject(id, new Item(moved));
    registerItem<Item>(data, id);
}
