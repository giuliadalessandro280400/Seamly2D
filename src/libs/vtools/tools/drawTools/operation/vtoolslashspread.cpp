/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolslashspread.cpp
 **  @brief  Slash and spread: opens a part of the pattern around a hinge point
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

#include "vtoolslashspread.h"

#include <limits.h>
#include <QDomElement>
#include <QSharedPointer>
#include <QString>

#include "../../../dialogs/tools/dialogtool.h"
#include "../../../dialogs/tools/dialogslashspread.h"
#include "../../../visualization/line/operation/vistoolslashspread.h"
#include "../../../visualization/visualization.h"
#include "../vgeometry/vabstractcurve.h"
#include "../vgeometry/vabstractcubicbezierpath.h"
#include "../vgeometry/vabstractbezier.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vpointf.h"
#include "../vpatterndb/vtranslatevars.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vformula.h"
#include "../ifc/ifcdef.h"
#include "../ifc/exception/vexception.h"
#include "../vwidgets/vmaingraphicsscene.h"
#include "../../vabstracttool.h"
#include "../../vdatatool.h"
#include "../vdrawtool.h"

const QString VToolSlashSpread::ToolType       = QStringLiteral("slashSpread");
const QString VToolSlashSpread::AttrHinge      = QStringLiteral("hinge");
const QString VToolSlashSpread::AttrEdgeStart  = QStringLiteral("edgeStart");
const QString VToolSlashSpread::AttrEdgeEnd    = QStringLiteral("edgeEnd");
const QString VToolSlashSpread::AttrSlashes    = QStringLiteral("slashes");
const QString VToolSlashSpread::AttrSpread     = QStringLiteral("spread");
const QString VToolSlashSpread::AttrSpreadType = QStringLiteral("spreadType");
const QString VToolSlashSpread::AttrFixed      = QStringLiteral("fixedSide");

//---------------------------------------------------------------------------------------------------------------------
VToolSlashSpread::VToolSlashSpread(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 hinge,
                                   quint32 edgeStart, quint32 edgeEnd, int slashes, const QString &spread,
                                   bool percent, SlashSpread::Fixed fixed, const QString &suffix,
                                   const QVector<SourceItem> &source, const QVector<DestinationItem> &destination,
                                   const Source &typeCreation, QGraphicsItem *parent)
    : VAbstractOperation(doc, data, id, suffix, source, destination, parent)
    , m_hinge(hinge)
    , m_edgeStart(edgeStart)
    , m_edgeEnd(edgeEnd)
    , m_slashes(slashes)
    , m_formulaSpread(spread)
    , m_percent(percent)
    , m_fixed(fixed)
{
    InitOperatedObjects();
    ToolCreation(typeCreation);
}

//---------------------------------------------------------------------------------------------------------------------
SlashSpread VToolSlashSpread::makeSpread(const VContainer *data, quint32 hinge, quint32 edgeStart, quint32 edgeEnd,
                                         int slashes, qreal spread, bool percent, SlashSpread::Fixed fixed,
                                         const QVector<quint32> &objects)
{
    const QPointF h = static_cast<QPointF>(*data->GeometricObject<VPointF>(hinge));
    const QVector<QPointF> edge = SlashSpread::edgeOf(data, edgeStart, edgeEnd, objects);
    return SlashSpread(h, edge, slashes, percent ? spread : qApp->toPixel(spread), percent, fixed);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::setDialog()
{
    SCASSERT(not m_dialog.isNull())
    QSharedPointer<DialogSlashSpread> dialogTool = m_dialog.objectCast<DialogSlashSpread>();
    SCASSERT(not dialogTool.isNull())
    dialogTool->setHingeId(m_hinge);
    dialogTool->setEdgeStartId(m_edgeStart);
    dialogTool->setEdgeEndId(m_edgeEnd);
    dialogTool->setSlashes(m_slashes);
    dialogTool->setSpread(m_formulaSpread);
    dialogTool->setPercent(m_percent);
    dialogTool->setFixed(m_fixed);
    dialogTool->setSuffix(suffix);
}

//---------------------------------------------------------------------------------------------------------------------
VToolSlashSpread *VToolSlashSpread::Create(QSharedPointer<DialogTool> dialog, VMainGraphicsScene *scene,
                                           VAbstractPattern *doc, VContainer *data)
{
    SCASSERT(not dialog.isNull())
    QSharedPointer<DialogSlashSpread> dialogTool = dialog.objectCast<DialogSlashSpread>();
    SCASSERT(not dialogTool.isNull())
    QString spread = dialogTool->getSpread();

    VToolSlashSpread *operation = Create(0, dialogTool->getHingeId(), dialogTool->getEdgeStartId(),
                                         dialogTool->getEdgeEndId(), dialogTool->getSlashes(), spread,
                                         dialogTool->isPercent(), dialogTool->getFixed(), dialogTool->getSuffix(),
                                         dialogTool->getSourceObjects(), QVector<DestinationItem>(), scene, doc,
                                         data, Document::FullParse, Source::FromGui);
    if (operation != nullptr)
    {
        operation->m_dialog = dialogTool;
    }
    return operation;
}

//---------------------------------------------------------------------------------------------------------------------
VToolSlashSpread *VToolSlashSpread::Create(const quint32 _id, quint32 hinge, quint32 edgeStart, quint32 edgeEnd,
                                           int slashes, QString &spread, bool percent, SlashSpread::Fixed fixed,
                                           const QString &suffix, const QVector<SourceItem> &source,
                                           const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                                           VAbstractPattern *doc, VContainer *data, const Document &parse,
                                           const Source &typeCreation)
{
    const qreal spreadValue = CheckFormula(_id, spread, data);

    QVector<quint32> objects;
    for (const SourceItem &item : source)
    {
        objects.append(item.id);
    }
    const SlashSpread geometry = makeSpread(data, hinge, edgeStart, edgeEnd, slashes, spreadValue, percent, fixed,
                                            objects);

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

        if (static_cast<GOType>(obj->getType()) == GOType::Point)
        {
            VPointF moved = openedPoint(geometry, objectId, suffix, data);
            moved.setIdObject(id);
            if (fromGui)
            {
                DestinationItem item;
                item.mx = moved.mx();
                item.my = moved.my();
                item.showPointName = moved.isShowPointName();
                item.id = data->AddGObject(new VPointF(moved));
                dest.append(item);
            }
            else
            {
                const DestinationItem &item = dest.at(i);
                moved.setMx(item.mx);
                moved.setMy(item.my);
                moved.setShowPointName(item.showPointName);
                data->UpdateGObject(item.id, new VPointF(moved));
            }
            continue;
        }

        const QSharedPointer<VAbstractCurve> curve = data->GeometricObject<VAbstractCurve>(objectId);
        VAbstractCurve *opened = geometry.opened(curve, suffix); // the container takes it
        opened->setIdObject(id);
        if (fromGui)
        {
            DestinationItem item;
            item.mx = INT_MAX;
            item.my = INT_MAX;
            item.id = data->AddGObject(opened);
            registerCurve(data, data->GeometricObject<VAbstractCurve>(item.id), item.id);
            dest.append(item);
        }
        else
        {
            const quint32 destId = dest.at(i).id;
            data->UpdateGObject(destId, opened);
            registerCurve(data, data->GeometricObject<VAbstractCurve>(destId), destId);
        }
    }

    if (not fromGui && parse != Document::FullParse)
    {
        doc->UpdateToolData(id, data);
    }

    if (parse == Document::FullParse)
    {
        VDrawTool::AddRecord(id, Tool::SlashSpread, doc);
        VToolSlashSpread *tool = new VToolSlashSpread(doc, data, id, hinge, edgeStart, edgeEnd, slashes, spread,
                                                      percent, fixed, suffix, source, dest, typeCreation);
        scene->addItem(tool);
        initOperationToolConnections(scene, tool);
        VAbstractPattern::AddTool(id, tool);
        doc->IncrementReferens(data->GeometricObject<VPointF>(hinge)->getIdTool());
        doc->IncrementReferens(data->GeometricObject<VPointF>(edgeStart)->getIdTool());
        doc->IncrementReferens(data->GeometricObject<VPointF>(edgeEnd)->getIdTool());
        for (int i = 0; i < source.size(); ++i)
        {
            doc->IncrementReferens(data->GetGObject(source.at(i).id)->getIdTool());
        }
        return tool;
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
VPointF VToolSlashSpread::openedPoint(const SlashSpread &spread, quint32 idItem, const QString &suffix,
                                      VContainer *data)
{
    const QSharedPointer<VPointF> point = data->GeometricObject<VPointF>(idItem);
    const qreal angle = spread.isValid() ? spread.rotation(spread.section(static_cast<QPointF>(*point))) : 0;
    return point->Rotate(spread.hinge(), angle, suffix);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::registerCurve(VContainer *data, const QSharedPointer<VAbstractCurve> &curve, quint32 id)
{
    switch (static_cast<GOType>(curve->getType()))
    {
        case GOType::Arc:
        case GOType::EllipticalArc:
            data->AddArc(curve, id);
            break;
        case GOType::Spline:
        case GOType::CubicBezier:
            data->AddSpline(qSharedPointerDynamicCast<VAbstractBezier>(curve), id);
            break;
        case GOType::SplinePath:
        case GOType::CubicBezierPath:
            data->AddCurveWithSegments(qSharedPointerDynamicCast<VAbstractCubicBezierPath>(curve), id);
            break;
        default:
            break;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::saveChange()
{
    QSharedPointer<VGObject> obj = VAbstractTool::data.GetFakeGObject(m_id);
    SaveOption(obj);
}

void VToolSlashSpread::setHingeId(quint32 value)     { if (value != NULL_ID) { m_hinge = value; saveChange(); } }
void VToolSlashSpread::setEdgeStartId(quint32 value) { if (value != NULL_ID) { m_edgeStart = value; saveChange(); } }
void VToolSlashSpread::setEdgeEndId(quint32 value)   { if (value != NULL_ID) { m_edgeEnd = value; saveChange(); } }
void VToolSlashSpread::setSlashes(int value)         { if (value > 0) { m_slashes = value; saveChange(); } }
void VToolSlashSpread::setPercent(bool value)        { m_percent = value; saveChange(); }
void VToolSlashSpread::setFixed(SlashSpread::Fixed value) { m_fixed = value; saveChange(); }

//---------------------------------------------------------------------------------------------------------------------
QString VToolSlashSpread::pointName(quint32 id) const
{
    return VAbstractTool::data.GetGObject(id)->name();
}

//---------------------------------------------------------------------------------------------------------------------
VFormula VToolSlashSpread::getFormulaSpread() const
{
    VFormula formula(m_formulaSpread, getData());
    formula.setCheckZero(false);
    formula.setToolId(m_id);
    formula.setPostfix(m_percent ? QStringLiteral("%") : UnitsToStr(qApp->patternUnit()));
    return formula;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::setFormulaSpread(const VFormula &value)
{
    if (value.error() == false)
    {
        m_formulaSpread = value.GetFormula(FormulaType::FromUser);
        saveChange();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::ShowVisualization(bool show)
{
    ShowToolVisualization<VisToolSlashSpread>(show);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::SetVisualization()
{
    if (not vis.isNull())
    {
        VisToolSlashSpread *visual = qobject_cast<VisToolSlashSpread *>(vis);
        SCASSERT(visual != nullptr)

        visual->setObjects(sourceToObjects(source));
        visual->setHingeId(m_hinge);
        visual->setEdgeStartId(m_edgeStart);
        visual->setEdgeEndId(m_edgeEnd);
        visual->setSlashes(m_slashes);
        visual->setPercent(m_percent);
        visual->setFixed(m_fixed);
        visual->setSpread(qApp->translateVariables()->FormulaToUser(m_formulaSpread,
                                                                    qApp->Settings()->getOsSeparator()));
        visual->RefreshGeometry();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::SaveDialog(QDomElement &domElement)
{
    SCASSERT(not m_dialog.isNull())
    QSharedPointer<DialogSlashSpread> dialogTool = m_dialog.objectCast<DialogSlashSpread>();
    SCASSERT(not dialogTool.isNull())

    doc->SetAttribute(domElement, AttrHinge, QString().setNum(dialogTool->getHingeId()));
    doc->SetAttribute(domElement, AttrEdgeStart, QString().setNum(dialogTool->getEdgeStartId()));
    doc->SetAttribute(domElement, AttrEdgeEnd, QString().setNum(dialogTool->getEdgeEndId()));
    doc->SetAttribute(domElement, AttrSlashes, dialogTool->getSlashes());
    doc->SetAttribute(domElement, AttrSpread, dialogTool->getSpread());
    doc->SetAttribute(domElement, AttrSpreadType,
                      dialogTool->isPercent() ? QStringLiteral("percent") : QStringLiteral("length"));
    doc->SetAttribute(domElement, AttrFixed, SlashSpread::fixedToString(dialogTool->getFixed()));
    doc->SetAttribute(domElement, AttrSuffix, dialogTool->getSuffix());
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::ReadToolAttributes(const QDomElement &domElement)
{
    m_hinge         = doc->GetParametrUInt(domElement, AttrHinge, NULL_ID_STR);
    m_edgeStart     = doc->GetParametrUInt(domElement, AttrEdgeStart, NULL_ID_STR);
    m_edgeEnd       = doc->GetParametrUInt(domElement, AttrEdgeEnd, NULL_ID_STR);
    m_slashes       = qMax(1, static_cast<int>(doc->GetParametrUInt(domElement, AttrSlashes, QStringLiteral("3"))));
    m_formulaSpread = doc->GetParametrString(domElement, AttrSpread, QStringLiteral("0"));
    m_percent       = doc->GetParametrString(domElement, AttrSpreadType, QStringLiteral("length"))
                      == QLatin1String("percent");
    m_fixed         = SlashSpread::fixedFromString(doc->GetParametrString(domElement, AttrFixed,
                                                                          QStringLiteral("start")));
    suffix          = doc->GetParametrString(domElement, AttrSuffix);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj)
{
    VDrawTool::SaveOptions(tag, obj);

    doc->SetAttribute(tag, AttrType, ToolType);
    doc->SetAttribute(tag, AttrHinge, QString().setNum(m_hinge));
    doc->SetAttribute(tag, AttrEdgeStart, QString().setNum(m_edgeStart));
    doc->SetAttribute(tag, AttrEdgeEnd, QString().setNum(m_edgeEnd));
    doc->SetAttribute(tag, AttrSlashes, m_slashes);
    doc->SetAttribute(tag, AttrSpread, m_formulaSpread);
    doc->SetAttribute(tag, AttrSpreadType, m_percent ? QStringLiteral("percent") : QStringLiteral("length"));
    doc->SetAttribute(tag, AttrFixed, SlashSpread::fixedToString(m_fixed));
    doc->SetAttribute(tag, AttrSuffix, suffix);

    SaveSourceDestination(tag);
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolSlashSpread::makeToolTip() const
{
    return QString("<tr> <td><b>%1:</b> %2</td> </tr>"
                   "<tr> <td><b>%3:</b> %4 - %5</td> </tr>"
                   "<tr> <td><b>%6:</b> %7</td> </tr>"
                   "<tr> <td><b>%8:</b> %9 %10</td> </tr>")
            .arg(tr("Hinge"), pointName(m_hinge), tr("Edge"), pointName(m_edgeStart), pointName(m_edgeEnd),
                 tr("Slashes"), QString::number(m_slashes), tr("Spread"))
            .arg(getFormulaSpread().getDoubleValue())
            .arg(m_percent ? QStringLiteral("%") : UnitsToStr(qApp->patternUnit(), true));
}

//---------------------------------------------------------------------------------------------------------------------
void VToolSlashSpread::showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id)
{
    try
    {
        ContextMenu<DialogSlashSpread>(event, id);
    }
    catch(const VExceptionToolWasDeleted &error)
    {
        Q_UNUSED(error)
        return;//Leave this method immediately!!!
    }
}
