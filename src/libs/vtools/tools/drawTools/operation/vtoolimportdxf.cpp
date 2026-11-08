/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolimportdxf.cpp
 **  @brief  A DXF pattern (AAMA) placed in the draft
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

#include "vtoolimportdxf.h"
#include "aamadxf.h"

#include <climits>
#include <QDomElement>
#include <QLineF>
#include <QHash>
#include <QInputDialog>
#include <QMap>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QString>
#include <QtMath>

#include <cmath>

#include "../vgeometry/vabstractcubicbezierpath.h"
#include "../vgeometry/vgobject.h"
#include "../vgeometry/vpointf.h"
#include "../vgeometry/vsplinepath.h"
#include "../vgeometry/vsplinepoint.h"
#include "../vmisc/vabstractapplication.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vformula.h"
#include "../ifc/ifcdef.h"
#include "../ifc/exception/vexception.h"
#include "../vwidgets/vmaingraphicsscene.h"
#include "../../vabstracttool.h"
#include "../vdrawtool.h"

const QString VToolImportDxf::ToolType         = QStringLiteral("dxfImport");
const QString VToolImportDxf::AttrAnchor       = QStringLiteral("anchor");
const QString VToolImportDxf::AttrDxfAnchor    = QStringLiteral("dxfAnchor");
const QString VToolImportDxf::AttrDirection    = QStringLiteral("direction");
const QString VToolImportDxf::AttrDxfDirection = QStringLiteral("dxfDirection");
const QString VToolImportDxf::TagShapes        = QStringLiteral("shapes");
const QString VToolImportDxf::AttrLinks        = QStringLiteral("links");
const QString VToolImportDxf::AttrReference    = QStringLiteral("reference");

namespace
{
QPointF mmToPixel(const QPointF &p)
{
    return QPointF(ToPixel(p.x(), Unit::Mm), ToPixel(p.y(), Unit::Mm));
}

QPointF unitsToPixel(const QPointF &p)
{
    return QPointF(qApp->toPixel(p.x()), qApp->toPixel(p.y()));
}

QPointF pixelToUnits(const QPointF &p)
{
    return QPointF(qApp->fromPixel(p.x()), qApp->fromPixel(p.y()));
}

QPointF rotated(const QPointF &v, qreal degrees)
{
    QLineF line(QPointF(), v);
    line.setAngle(line.angle() + degrees);
    return line.p2();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief The Deformation class moves any point as the tied points moved: moving least squares (Schaefer, McPhail,
 * Warren 2006). One tie: the whole moves. Two: it moves, turns and grows. Three or more: it also changes shape,
 * smoothly, each point following mostly the ties near it.
 */
class Deformation
{
public:
    Deformation(const QVector<QPointF> &from, const QVector<QPointF> &to) : m_from(from), m_to(to) {}

    QPointF map(const QPointF &v) const
    {
        const int n = m_from.size();
        if (n == 0)
        {
            return v;
        }
        if (n == 1)
        {
            return v + (m_to.first() - m_from.first());
        }

        QVector<qreal> w(n);
        qreal sum = 0;
        QPointF pStar, qStar;
        for (int i = 0; i < n; ++i)
        {
            const QPointF d = m_from.at(i) - v;
            const qreal d2 = d.x() * d.x() + d.y() * d.y();
            if (d2 < 1e-9)
            {
                return m_to.at(i);
            }
            w[i] = 1.0 / d2;
            sum += w.at(i);
            pStar += m_from.at(i) * w.at(i);
            qStar += m_to.at(i) * w.at(i);
        }
        pStar /= sum;
        qStar /= sum;

        if (n >= 3)
        {
            // affine: M = (sum w p^T p)^-1 (sum w p^T q), row vectors
            qreal a11 = 0, a12 = 0, a22 = 0, b11 = 0, b12 = 0, b21 = 0, b22 = 0;
            for (int i = 0; i < n; ++i)
            {
                const QPointF p = m_from.at(i) - pStar;
                const QPointF q = m_to.at(i) - qStar;
                a11 += w.at(i) * p.x() * p.x();
                a12 += w.at(i) * p.x() * p.y();
                a22 += w.at(i) * p.y() * p.y();
                b11 += w.at(i) * p.x() * q.x();
                b12 += w.at(i) * p.x() * q.y();
                b21 += w.at(i) * p.y() * q.x();
                b22 += w.at(i) * p.y() * q.y();
            }
            const qreal det = a11 * a22 - a12 * a12;
            if (qAbs(det) > 1e-9 * qMax(1.0, a11 * a22))
            {
                const qreal i11 = a22 / det, i12 = -a12 / det, i22 = a11 / det;
                const qreal m11 = i11 * b11 + i12 * b21;
                const qreal m12 = i11 * b12 + i12 * b22;
                const qreal m21 = i12 * b11 + i22 * b21;
                const qreal m22 = i12 * b12 + i22 * b22;
                const QPointF r = v - pStar;
                return QPointF(r.x() * m11 + r.y() * m21, r.x() * m12 + r.y() * m22) + qStar;
            }
        }

        // similarity (two ties, or ties in a line): a complex factor a = sum w conj(p) q / sum w |p|^2
        qreal re = 0, im = 0, norm = 0;
        for (int i = 0; i < n; ++i)
        {
            const QPointF p = m_from.at(i) - pStar;
            const QPointF q = m_to.at(i) - qStar;
            re += w.at(i) * (p.x() * q.x() + p.y() * q.y());
            im += w.at(i) * (p.x() * q.y() - p.y() * q.x());
            norm += w.at(i) * (p.x() * p.x() + p.y() * p.y());
        }
        if (norm < 1e-12)
        {
            return v + (qStar - pStar);
        }
        re /= norm;
        im /= norm;
        const QPointF r = v - pStar;
        return QPointF(r.x() * re - r.y() * im, r.x() * im + r.y() * re) + qStar;
    }

private:
    QVector<QPointF> m_from;
    QVector<QPointF> m_to;
};

//---------------------------------------------------------------------------------------------------------------------
struct Placement
{
    QPointF     anchorRef;
    QPointF     dxfAnchor;
    qreal       angle {0};
    Deformation deformation {{}, {}};

    QPointF map(const QPointF &mm) const
    {
        return deformation.map(anchorRef + rotated(mmToPixel(mm) - dxfAnchor, angle));
    }
};

//---------------------------------------------------------------------------------------------------------------------
Placement makePlacement(const VContainer *data, quint32 anchor, int dxfAnchor, quint32 direction, int dxfDirection,
                        qreal angle, const QVector<DxfShape> &shapes, const QPointF &anchorReference,
                        const QPointF &directionReference, const QVector<DxfLink> &links)
{
    Placement placement;
    QVector<QPointF> from, to;

    const QPointF anchorNow = static_cast<QPointF>(*data->GeometricObject<VPointF>(anchor));
    placement.anchorRef = unitsToPixel(anchorReference);
    from.append(placement.anchorRef);
    to.append(anchorNow);

    if (dxfAnchor >= 0 && dxfAnchor < shapes.size() && not shapes.at(dxfAnchor).points.isEmpty())
    {
        placement.dxfAnchor = mmToPixel(shapes.at(dxfAnchor).points.first());
    }

    qreal turn = 0;
    QPointF directionRef;
    QPointF directionNow;
    QPointF dxfDirectionPoint;
    const bool hasDirection = direction != NULL_ID && dxfDirection >= 0 && dxfDirection < shapes.size()
                              && not shapes.at(dxfDirection).points.isEmpty();
    if (hasDirection)
    {
        directionRef = unitsToPixel(directionReference);
        directionNow = static_cast<QPointF>(*data->GeometricObject<VPointF>(direction));
        dxfDirectionPoint = mmToPixel(shapes.at(dxfDirection).points.first());
        turn = QLineF(placement.anchorRef, directionRef).angle()
               - QLineF(placement.dxfAnchor, dxfDirectionPoint).angle();
    }
    placement.angle = turn + angle;

    // Each tie moves its DXF point exactly as much as its draft point moved; the free points follow in proportion.
    const auto placed = [&placement](const QPointF &mm)
    {
        return placement.anchorRef + rotated(mmToPixel(mm) - placement.dxfAnchor, placement.angle);
    };
    if (hasDirection)
    {
        const QPointF p = placed(shapes.at(dxfDirection).points.first());
        from.append(p);
        to.append(p + (directionNow - directionRef));
    }
    for (const DxfLink &link : links)
    {
        if (link.shape < 0 || link.shape >= shapes.size() || shapes.at(link.shape).points.isEmpty())
        {
            continue;
        }
        try
        {
            const QPointF now = static_cast<QPointF>(*data->GeometricObject<VPointF>(link.point));
            const QPointF p = placed(shapes.at(link.shape).points.first());
            from.append(p);
            to.append(p + (now - unitsToPixel(link.reference)));
        }
        catch (const VException &)
        {
            // the draft point was removed: the tie is ignored
        }
    }
    placement.deformation = Deformation(from, to);
    return placement;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief throughPoints a smooth curve through the points (Catmull-Rom), straight if there are only two.
 */
VSplinePath *throughPoints(const QVector<QPointF> &points, const QString &name)
{
    const int n = points.size();
    QVector<VSplinePoint> splinePoints;
    for (int i = 0; i < n; ++i)
    {
        const QPointF p = points.at(i);
        QPointF tangent;
        if (n == 2)
        {
            tangent = points.at(1) - points.at(0);
        }
        else if (i == 0)
        {
            tangent = points.at(1) - points.at(0);
        }
        else if (i == n - 1)
        {
            tangent = points.at(n - 1) - points.at(n - 2);
        }
        else
        {
            tangent = (points.at(i + 1) - points.at(i - 1)) / 2;
        }
        const qreal angle2 = QLineF(QPointF(), tangent).angle();
        const qreal angle1 = std::fmod(angle2 + 180, 360);
        const qreal before = i > 0 ? QLineF(points.at(i - 1), p).length() / 3 : 0;
        const qreal after = i < n - 1 ? QLineF(p, points.at(i + 1)).length() / 3 : 0;
        splinePoints.append(VSplinePoint(VPointF(p), angle1, QString::number(angle1), angle2, QString::number(angle2),
                                         before, QString::number(qApp->fromPixel(before)), after,
                                         QString::number(qApp->fromPixel(after))));
    }
    auto *path = new VSplinePath(splinePoints);
    path->setName(name);
    return path;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
VToolImportDxf::VToolImportDxf(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 anchor, int dxfAnchor,
                               quint32 direction, int dxfDirection, const QString &angle,
                               const QVector<DxfShape> &shapes, const QPointF &anchorReference,
                               const QPointF &directionReference, const QVector<DxfLink> &links,
                               const QVector<DestinationItem> &destination, const Source &typeCreation,
                               QGraphicsItem *parent)
    : VAbstractOperation(doc, data, id, QString(), QVector<SourceItem>(), destination, parent)
    , m_anchor(anchor)
    , m_dxfAnchor(dxfAnchor)
    , m_direction(direction)
    , m_dxfDirection(dxfDirection)
    , m_formulaAngle(angle)
    , m_shapes(shapes)
    , m_anchorReference(anchorReference)
    , m_directionReference(directionReference)
    , m_links(links)
{
    InitOperatedObjects();
    ToolCreation(typeCreation);
}

//---------------------------------------------------------------------------------------------------------------------
VToolImportDxf *VToolImportDxf::Create(const quint32 _id, quint32 anchor, int dxfAnchor, quint32 direction,
                                       int dxfDirection, QString &angle, const QVector<DxfShape> &shapes,
                                       const QPointF &anchorReference, const QPointF &directionReference,
                                       const QVector<DxfLink> &links, const QVector<DestinationItem> &destination,
                                       VMainGraphicsScene *scene, VAbstractPattern *doc, VContainer *data,
                                       const Document &parse, const Source &typeCreation)
{
    const qreal angleValue = CheckFormula(_id, angle, data);
    const Placement placement = makePlacement(data, anchor, dxfAnchor, direction, dxfDirection, angleValue, shapes,
                                              anchorReference, directionReference, links);

    QVector<DestinationItem> dest = destination;
    quint32 id = _id;
    const bool fromGui = typeCreation == Source::FromGui;
    if (fromGui)
    {
        dest.clear();
        id = VContainer::getNextId();
    }

    for (int i = 0; i < shapes.size(); ++i)
    {
        const DxfShape &shape = shapes.at(i);
        if (shape.point)
        {
            const QPointF p = placement.map(shape.points.value(0));
            VPointF point(p.x(), p.y(), shape.name, qApp->toPixel(0.13), qApp->toPixel(0.26));
            point.setIdObject(id);
            if (fromGui)
            {
                DestinationItem item;
                item.mx = point.mx();
                item.my = point.my();
                item.showPointName = true;
                item.id = data->AddGObject(new VPointF(point));
                dest.append(item);
            }
            else if (i < dest.size())
            {
                const DestinationItem &item = dest.at(i);
                point.setMx(item.mx);
                point.setMy(item.my);
                point.setShowPointName(item.showPointName);
                data->UpdateGObject(item.id, new VPointF(point));
            }
            continue;
        }

        QVector<QPointF> mapped;
        for (const QPointF &mm : shape.points)
        {
            mapped.append(placement.map(mm));
        }
        VSplinePath *path = throughPoints(mapped, shape.name);
        path->setIdObject(id);
        quint32 curveId = NULL_ID;
        if (fromGui)
        {
            DestinationItem item;
            item.mx = INT_MAX;
            item.my = INT_MAX;
            item.id = data->AddGObject(path);
            curveId = item.id;
            dest.append(item);
        }
        else if (i < dest.size())
        {
            curveId = dest.at(i).id;
            data->UpdateGObject(curveId, path);
        }
        else
        {
            delete path;
        }
        if (curveId != NULL_ID)
        {
            data->AddCurveWithSegments(data->GeometricObject<VAbstractCubicBezierPath>(curveId), curveId);
        }
    }

    if (not fromGui && parse != Document::FullParse)
    {
        doc->UpdateToolData(id, data);
    }

    if (parse == Document::FullParse)
    {
        VDrawTool::AddRecord(id, Tool::ImportDxf, doc);
        VToolImportDxf *tool = new VToolImportDxf(doc, data, id, anchor, dxfAnchor, direction, dxfDirection, angle,
                                                  shapes, anchorReference, directionReference, links, dest,
                                                  typeCreation);
        scene->addItem(tool);
        initOperationToolConnections(scene, tool);
        VAbstractPattern::AddTool(id, tool);
        doc->IncrementReferens(data->GeometricObject<VPointF>(anchor)->getIdTool());
        if (direction != NULL_ID)
        {
            doc->IncrementReferens(data->GeometricObject<VPointF>(direction)->getIdTool());
        }
        for (const DxfLink &link : links)
        {
            try
            {
                doc->IncrementReferens(data->GeometricObject<VPointF>(link.point)->getIdTool());
            }
            catch (const VException &)
            {
            }
        }
        return tool;
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<DxfShape> VToolImportDxf::shapesFromFile(const AamaFile &file, const QVector<int> &pieces,
                                                 const VContainer *data)
{
    QVector<DxfShape> points;
    QVector<DxfShape> curves;
    QStringList taken;

    const auto unique = [&taken](const QString &name)
    {
        return VContainer::IsUnique(name) && not taken.contains(name);
    };
    const auto flip = [](const QPointF &p) { return QPointF(p.x(), -p.y()); };
    Q_UNUSED(data)

    for (const int index : pieces)
    {
        if (index < 0 || index >= file.pieces.size())
        {
            continue;
        }
        const AamaPiece &piece = file.pieces.at(index);

        // a short name made of letters and digits, not used yet in the pattern
        QString base = piece.name;
        base.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{Nd}]")));
        if (base.isEmpty() || base.at(0).isDigit())
        {
            base.prepend(QLatin1Char('P'));
        }
        QString prefix = base;
        for (int k = 2; not unique(prefix + QStringLiteral("_1")); ++k)
        {
            prefix = base + QString::number(k);
        }

        // corners
        QVector<int> corners;
        QVector<QString> cornerNames;
        for (int i = 0; i < piece.boundary.size(); ++i)
        {
            if (piece.turn.at(i))
            {
                corners.append(i);
                const QString name = prefix + QLatin1Char('_') + QString::number(corners.size());
                cornerNames.append(name);
                taken.append(name);
                points.append({true, name, {flip(piece.boundary.at(i))}});
            }
        }
        // notches
        for (int i = 0; i < piece.notches.size(); ++i)
        {
            const QString name = prefix + QStringLiteral("_N") + QString::number(i + 1);
            taken.append(name);
            points.append({true, name, {flip(piece.notches.at(i))}});
        }
        // the outline, from corner to corner
        const int n = piece.boundary.size();
        for (int c = 0; c < corners.size(); ++c)
        {
            const int from = corners.at(c);
            const int to = corners.at((c + 1) % corners.size());
            QVector<QPointF> line;
            int i = from;
            do
            {
                line.append(flip(piece.boundary.at(i)));
                i = (i + 1) % n;
            } while (i != to);
            line.append(flip(piece.boundary.at(to)));
            if (corners.size() == 1 && line.size() < 3)
            {
                continue;
            }
            const QString name = QStringLiteral("Spl_") + cornerNames.at(c) + QLatin1Char('_')
                                 + cornerNames.at((c + 1) % corners.size());
            curves.append({false, name, line});
        }
        // internal lines (darts...): a point at each end and at each corner, straight or curved between them
        for (int l = 0; l < piece.internals.size(); ++l)
        {
            const QString lineName = prefix + QStringLiteral("_L") + QString::number(l + 1);
            QVector<QPointF> line;
            for (const QPointF &p : piece.internals.at(l))
            {
                line.append(flip(p));
            }
            QVector<int> breaks {0};
            for (int i = 1; i + 1 < line.size(); ++i)
            {
                const qreal a1 = QLineF(line.at(i - 1), line.at(i)).angle();
                const qreal a2 = QLineF(line.at(i), line.at(i + 1)).angle();
                qreal change = std::fmod(qAbs(a2 - a1), 360.0);
                change = qMin(change, 360.0 - change);
                if (change > 30)
                {
                    breaks.append(i);
                }
            }
            breaks.append(line.size() - 1);

            QStringList names;
            for (int b = 0; b < breaks.size(); ++b)
            {
                QString name;
                if (b == 0)
                {
                    name = lineName + QLatin1Char('a');
                }
                else if (b == breaks.size() - 1)
                {
                    name = lineName + QLatin1Char('b');
                }
                else
                {
                    name = lineName + QLatin1Char('_') + QString::number(b);
                }
                names.append(name);
                taken.append(name);
                points.append({true, name, {line.at(breaks.at(b))}});
            }
            for (int b = 0; b + 1 < breaks.size(); ++b)
            {
                curves.append({false, QStringLiteral("Spl_") + names.at(b) + QLatin1Char('_') + names.at(b + 1),
                               line.mid(breaks.at(b), breaks.at(b + 1) - breaks.at(b) + 1)});
            }
        }
        // grain line
        if (piece.grain.size() == 2)
        {
            curves.append({false, QStringLiteral("Spl_") + prefix + QStringLiteral("_Grain"),
                           {flip(piece.grain.at(0)), flip(piece.grain.at(1))}});
        }
    }
    return points + curves;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<DxfShape> VToolImportDxf::readShapes(const QDomElement &domElement)
{
    QVector<DxfShape> shapes;
    const QDomElement tag = domElement.firstChildElement(TagShapes);
    for (QDomElement e = tag.firstChildElement(); not e.isNull(); e = e.nextSiblingElement())
    {
        DxfShape shape;
        shape.point = e.tagName() == QLatin1String("point");
        shape.name = e.attribute(QStringLiteral("name"));
        for (const QString &pair : e.attribute(QStringLiteral("xy")).split(QLatin1Char(' '), Qt::SkipEmptyParts))
        {
            const QStringList xy = pair.split(QLatin1Char(','));
            if (xy.size() == 2)
            {
                shape.points.append(QPointF(xy.at(0).toDouble(), xy.at(1).toDouble()));
            }
        }
        if (not shape.points.isEmpty())
        {
            shapes.append(shape);
        }
    }
    return shapes;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<DxfLink> VToolImportDxf::readLinks(const QString &value)
{
    QVector<DxfLink> links;
    for (const QString &part : value.split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        const QStringList fields = part.split(QLatin1Char(':'));
        if (fields.size() != 3)
        {
            continue;
        }
        const QStringList xy = fields.at(2).split(QLatin1Char(','));
        if (xy.size() != 2)
        {
            continue;
        }
        DxfLink link;
        link.shape = fields.at(0).toInt();
        link.point = fields.at(1).toUInt();
        link.reference = QPointF(xy.at(0).toDouble(), xy.at(1).toDouble());
        links.append(link);
    }
    return links;
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolImportDxf::formatLinks(const QVector<DxfLink> &links)
{
    QStringList parts;
    for (const DxfLink &link : links)
    {
        parts.append(QStringLiteral("%1:%2:%3,%4").arg(link.shape).arg(link.point)
                     .arg(link.reference.x(), 0, 'f', 4).arg(link.reference.y(), 0, 'f', 4));
    }
    return parts.join(QLatin1Char(';'));
}

//---------------------------------------------------------------------------------------------------------------------
int VToolImportDxf::shapeOfDestination(quint32 destinationId) const
{
    for (int i = 0; i < destination.size(); ++i)
    {
        if (destination.at(i).id == destinationId)
        {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::linkPoint(quint32 destinationId, quint32 draftPoint)
{
    const int shape = shapeOfDestination(destinationId);
    if (shape < 0)
    {
        return;
    }
    for (int i = m_links.size() - 1; i >= 0; --i)
    {
        if (m_links.at(i).shape == shape)
        {
            m_links.removeAt(i);
        }
    }
    if (draftPoint != NULL_ID)
    {
        DxfLink link;
        link.shape = shape;
        link.point = draftPoint;
        link.reference = pixelToUnits(
            static_cast<QPointF>(*VAbstractTool::data.GeometricObject<VPointF>(draftPoint)));
        m_links.append(link);
    }
    saveChange();
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::saveChange()
{
    QSharedPointer<VGObject> obj = VAbstractTool::data.GetFakeGObject(m_id);
    SaveOption(obj);
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::setAnchorId(quint32 value)
{
    if (value != NULL_ID)
    {
        m_anchor = value;
        m_anchorReference = pixelToUnits(static_cast<QPointF>(*VAbstractTool::data.GeometricObject<VPointF>(value)));
        saveChange();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::setDirectionId(quint32 value)
{
    if (value != NULL_ID)
    {
        m_direction = value;
        m_directionReference =
            pixelToUnits(static_cast<QPointF>(*VAbstractTool::data.GeometricObject<VPointF>(value)));
        saveChange();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolImportDxf::pointName(quint32 id) const
{
    if (id == NULL_ID)
    {
        return QString();
    }
    return VAbstractTool::data.GetGObject(id)->name();
}

//---------------------------------------------------------------------------------------------------------------------
VFormula VToolImportDxf::getFormulaAngle() const
{
    VFormula formula(m_formulaAngle, getData());
    formula.setCheckZero(false);
    formula.setToolId(m_id);
    formula.setPostfix(degreeSymbol);
    return formula;
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::setFormulaAngle(const VFormula &value)
{
    if (value.error() == false)
    {
        m_formulaAngle = value.GetFormula(FormulaType::FromUser);
        saveChange();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::ReadToolAttributes(const QDomElement &domElement)
{
    m_anchor       = doc->GetParametrUInt(domElement, AttrAnchor, NULL_ID_STR);
    m_dxfAnchor    = domElement.attribute(AttrDxfAnchor, QStringLiteral("0")).toInt();
    m_direction    = doc->GetParametrUInt(domElement, AttrDirection, NULL_ID_STR);
    m_dxfDirection = domElement.attribute(AttrDxfDirection, QStringLiteral("-1")).toInt();
    m_formulaAngle = doc->GetParametrString(domElement, AttrAngle, QStringLiteral("0"));
    m_shapes       = readShapes(domElement);
    m_links        = readLinks(domElement.attribute(AttrLinks));
    const QStringList reference = domElement.attribute(AttrReference).split(QLatin1Char(','));
    if (reference.size() == 4)
    {
        m_anchorReference = QPointF(reference.at(0).toDouble(), reference.at(1).toDouble());
        m_directionReference = QPointF(reference.at(2).toDouble(), reference.at(3).toDouble());
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj)
{
    VDrawTool::SaveOptions(tag, obj);

    doc->SetAttribute(tag, AttrType, ToolType);
    doc->SetAttribute(tag, AttrAnchor, QString().setNum(m_anchor));
    doc->SetAttribute(tag, AttrDxfAnchor, m_dxfAnchor);
    if (m_direction != NULL_ID)
    {
        doc->SetAttribute(tag, AttrDirection, QString().setNum(m_direction));
        doc->SetAttribute(tag, AttrDxfDirection, m_dxfDirection);
    }
    else
    {
        tag.removeAttribute(AttrDirection);
        tag.removeAttribute(AttrDxfDirection);
    }
    doc->SetAttribute(tag, AttrAngle, m_formulaAngle);
    doc->SetAttribute(tag, AttrReference, QStringLiteral("%1,%2,%3,%4")
                      .arg(m_anchorReference.x(), 0, 'f', 4).arg(m_anchorReference.y(), 0, 'f', 4)
                      .arg(m_directionReference.x(), 0, 'f', 4).arg(m_directionReference.y(), 0, 'f', 4));
    if (m_links.isEmpty())
    {
        tag.removeAttribute(AttrLinks);
    }
    else
    {
        doc->SetAttribute(tag, AttrLinks, formatLinks(m_links));
    }

    SaveSourceDestination(tag); // removes the children

    QDomElement shapesTag = doc->createElement(TagShapes);
    for (const DxfShape &shape : m_shapes)
    {
        QDomElement e = doc->createElement(shape.point ? QStringLiteral("point") : QStringLiteral("curve"));
        e.setAttribute(QStringLiteral("name"), shape.name);
        QStringList xy;
        for (const QPointF &p : shape.points)
        {
            xy.append(QStringLiteral("%1,%2").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3));
        }
        e.setAttribute(QStringLiteral("xy"), xy.join(QLatin1Char(' ')));
        shapesTag.appendChild(e);
    }
    tag.appendChild(shapesTag);
}

//---------------------------------------------------------------------------------------------------------------------
QString VToolImportDxf::makeToolTip() const
{
    return QStringLiteral("<tr> <td><b>%1:</b> %2</td> </tr><tr> <td><b>%3:</b> %4</td> </tr>")
            .arg(tr("DXF placed on"), pointName(m_anchor), tr("Tied points"))
            .arg(m_links.size() + 1 + (m_direction != NULL_ID ? 1 : 0));
}

//---------------------------------------------------------------------------------------------------------------------
void VToolImportDxf::showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id)
{
    QMenu menu;
    QAction *tieAll = menu.addAction(tr("Tie Points to the Draft..."));
    QAction *tie = nullptr;
    QAction *untie = nullptr;
    const int shape = shapeOfDestination(id);
    const bool isPoint = shape >= 0 && shape < m_shapes.size() && m_shapes.at(shape).point;
    if (isPoint)
    {
        tie = menu.addAction(tr("Tie %1 to a Point of the Draft...").arg(m_shapes.at(shape).name));
        bool tied = false;
        for (const DxfLink &link : qAsConst(m_links))
        {
            tied = tied || link.shape == shape;
        }
        untie = menu.addAction(tr("Untie %1").arg(m_shapes.at(shape).name));
        untie->setEnabled(tied);
    }
    menu.addSeparator();
    QAction *remove = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Delete DXF"));
    remove->setEnabled(referens() <= 1);

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == nullptr)
    {
        return;
    }
    if (chosen == remove)
    {
        deleteTool(); // do not catch exception here
        return;
    }
    if (chosen == untie)
    {
        linkPoint(id, NULL_ID);
        return;
    }

    // the points of the draft (not the ones of this DXF), by name
    QMap<QString, quint32> candidates;
    QVector<quint32> own;
    for (const DestinationItem &item : qAsConst(destination))
    {
        own.append(item.id);
    }
    const QHash<quint32, QSharedPointer<VGObject>> *objects = VAbstractTool::data.DataGObjects();
    for (auto i = objects->cbegin(); i != objects->cend(); ++i)
    {
        if (static_cast<GOType>(i.value()->getType()) == GOType::Point && i.value()->getMode() == Draw::Calculation
            && not own.contains(i.key()) && not i.value()->name().isEmpty())
        {
            candidates.insert(i.value()->name(), i.key());
        }
    }
    if (candidates.isEmpty())
    {
        return;
    }

    if (chosen == tie)
    {
        bool ok = false;
        const QString name = QInputDialog::getItem(qApp->getMainWindow(), tr("Tie DXF Point"),
                                                   tr("Point of the draft that %1 follows:")
                                                       .arg(m_shapes.at(shape).name),
                                                   candidates.keys(), 0, false, &ok);
        if (ok && candidates.contains(name))
        {
            linkPoint(id, candidates.value(name));
        }
        return;
    }

    if (chosen == tieAll)
    {
        // one row per DXF point: which draft point it follows
        QDialog dialog(qApp->getMainWindow());
        dialog.setWindowTitle(tr("Tie DXF Points"));
        auto *layout = new QVBoxLayout(&dialog);
        auto *text = new QLabel(tr("Choose the point of the draft that each DXF point follows. The points you leave "
                                   "free move in proportion to the tied ones. The point the DXF is placed on is "
                                   "already tied."), &dialog);
        text->setWordWrap(true);
        layout->addWidget(text);
        auto *table = new QTableWidget(&dialog);
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels({tr("DXF point"), tr("Follows the draft point")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        QVector<int> rows;
        for (int i = 0; i < m_shapes.size(); ++i)
        {
            if (m_shapes.at(i).point && i != m_dxfAnchor)
            {
                rows.append(i);
            }
        }
        table->setRowCount(rows.size());
        QVector<QComboBox *> boxes;
        for (int r = 0; r < rows.size(); ++r)
        {
            auto *name = new QTableWidgetItem(m_shapes.at(rows.at(r)).name);
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            table->setItem(r, 0, name);
            auto *box = new QComboBox(table);
            box->addItem(tr("(free)"), static_cast<uint>(NULL_ID));
            for (auto c = candidates.cbegin(); c != candidates.cend(); ++c)
            {
                box->addItem(c.key(), static_cast<uint>(c.value()));
            }
            for (const DxfLink &link : qAsConst(m_links))
            {
                if (link.shape == rows.at(r))
                {
                    box->setCurrentIndex(qMax(0, box->findData(static_cast<uint>(link.point))));
                }
            }
            table->setCellWidget(r, 1, box);
            boxes.append(box);
        }
        table->resizeColumnsToContents();
        layout->addWidget(table);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        dialog.resize(460, 420);
        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }

        QVector<DxfLink> links;
        for (int r = 0; r < rows.size(); ++r)
        {
            const quint32 point = boxes.at(r)->currentData().toUInt();
            if (point == NULL_ID)
            {
                continue;
            }
            DxfLink link;
            link.shape = rows.at(r);
            link.point = point;
            // keep the reference of an unchanged tie, take the current position for a new one
            link.reference = pixelToUnits(
                static_cast<QPointF>(*VAbstractTool::data.GeometricObject<VPointF>(point)));
            for (const DxfLink &old : qAsConst(m_links))
            {
                if (old.shape == link.shape && old.point == link.point)
                {
                    link.reference = old.reference;
                }
            }
            links.append(link);
        }
        m_links = links;
        saveChange();
    }
}
