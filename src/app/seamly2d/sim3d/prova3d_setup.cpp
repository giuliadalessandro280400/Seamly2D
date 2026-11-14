/******************************************************************************
 **  @file   prova3d_setup.cpp
 **  @brief  Prova 3D: pieces of the pattern on the form, and their seams.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#include "prova3d_setup.h"

#include "../ifc/xml/vabstractpattern.h"
#include "../vgeometry/vpointf.h"
#include "../vmisc/def.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vpatterndb/vpiecepath.h"
#include "../vpatterndb/floatItemData/vgrainlinedata.h"

#include <QCoreApplication>
#include <QDomElement>
#include <QLineF>
#include <QMap>

#include <cmath>

namespace prova3d
{

namespace
{
const QString tagRoot = QStringLiteral("prova3d");

QString modeKey(Mode m)
{
    switch (m)
    {
        case Mode::Pair: return QStringLiteral("pair");
        case Mode::Single: return QStringLiteral("single");
        default: return QStringLiteral("fold");
    }
}
Mode modeFromKey(const QString &k)
{
    if (k == QLatin1String("pair")) return Mode::Pair;
    if (k == QLatin1String("single")) return Mode::Single;
    return Mode::Fold;
}
QString positionKey(Position p)
{
    switch (p)
    {
        case Position::Back: return QStringLiteral("back");
        case Position::FrontWaist: return QStringLiteral("frontWaist");
        case Position::BackWaist: return QStringLiteral("backWaist");
        default: return QStringLiteral("front");
    }
}
Position positionFromKey(const QString &k)
{
    if (k == QLatin1String("back")) return Position::Back;
    if (k == QLatin1String("frontWaist")) return Position::FrontWaist;
    if (k == QLatin1String("backWaist")) return Position::BackWaist;
    return Position::Front;
}
QString sidesKey(Sides s)
{
    switch (s)
    {
        case Sides::Right: return QStringLiteral("right");
        case Sides::Left: return QStringLiteral("left");
        default: return QStringLiteral("both");
    }
}
Sides sidesFromKey(const QString &k)
{
    if (k == QLatin1String("right")) return Sides::Right;
    if (k == QLatin1String("left")) return Sides::Left;
    return Sides::Both;
}

sim::P2 reflect(const sim::P2 &p, const sim::P2 &a, const sim::P2 &b)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double l = std::hypot(dx, dy);
    if (l < 1e-9) return {2 * a.x - p.x, p.y};
    const double ux = dx / l, uy = dy / l;
    const double vx = p.x - a.x, vy = p.y - a.y;
    const double d = vx * ux + vy * uy;
    return {a.x + 2 * d * ux - vx, a.y + 2 * d * uy - vy};
}

double distanceToLine(const sim::P2 &p, const sim::P2 &a, const sim::P2 &b)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double l = std::hypot(dx, dy);
    if (l < 1e-9) return std::hypot(p.x - a.x, p.y - a.y);
    return std::fabs((p.x - a.x) * dy - (p.y - a.y) * dx) / l;
}

// one copy of a piece in the simulation: its index there and where each outline point of the half went
struct Copy
{
    int index{-1};
    std::vector<int> map;
};
} // namespace

QString modeName(Mode m)
{
    switch (m)
    {
        case Mode::Pair: return QCoreApplication::translate("Prova3dSetup", "Cut 2");
        case Mode::Single: return QCoreApplication::translate("Prova3dSetup", "Whole");
        default: return QCoreApplication::translate("Prova3dSetup", "On the fold");
    }
}

QString positionName(Position p)
{
    switch (p)
    {
        case Position::Back: return QCoreApplication::translate("Prova3dSetup", "Back, from the shoulders");
        case Position::FrontWaist: return QCoreApplication::translate("Prova3dSetup", "Front, from the waist");
        case Position::BackWaist: return QCoreApplication::translate("Prova3dSetup", "Back, from the waist");
        default: return QCoreApplication::translate("Prova3dSetup", "Front, from the shoulders");
    }
}

QString sidesName(Sides s)
{
    switch (s)
    {
        case Sides::Right: return QCoreApplication::translate("Prova3dSetup", "Right half only");
        case Sides::Left: return QCoreApplication::translate("Prova3dSetup", "Left half only");
        default: return QCoreApplication::translate("Prova3dSetup", "Both halves");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
Setup Setup::load(const VAbstractPattern *doc)
{
    Setup s;
    if (doc == nullptr) return s;
    const QDomElement root = doc->documentElement().firstChildElement(tagRoot);
    for (QDomElement e = root.firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
    {
        if (e.tagName() == QLatin1String("piece"))
        {
            PieceSetup p;
            p.id = e.attribute(QStringLiteral("piece")).toUInt();   // not "id": ids are unique in the whole file
            p.use = e.attribute(QStringLiteral("use"), QStringLiteral("true")) != QLatin1String("false");
            p.mode = modeFromKey(e.attribute(QStringLiteral("mode")));
            p.center1 = e.attribute(QStringLiteral("center1")).toUInt();
            p.center2 = e.attribute(QStringLiteral("center2")).toUInt();
            p.position = positionFromKey(e.attribute(QStringLiteral("position")));
            s.pieces.append(p);
        }
        else if (e.tagName() == QLatin1String("seam"))
        {
            SeamSetup m;
            m.pieceA = e.attribute(QStringLiteral("pieceA")).toUInt();
            m.a1 = e.attribute(QStringLiteral("a1")).toUInt();
            m.a2 = e.attribute(QStringLiteral("a2")).toUInt();
            m.pieceB = e.attribute(QStringLiteral("pieceB")).toUInt();
            m.b1 = e.attribute(QStringLiteral("b1")).toUInt();
            m.b2 = e.attribute(QStringLiteral("b2")).toUInt();
            m.reverse = e.attribute(QStringLiteral("reverse")) == QLatin1String("true");
            m.sides = sidesFromKey(e.attribute(QStringLiteral("sides")));
            s.seams.append(m);
        }
    }
    return s;
}

void Setup::save(VAbstractPattern *doc) const
{
    if (doc == nullptr) return;
    QDomElement root = doc->documentElement();
    QDomElement old = root.firstChildElement(tagRoot);
    if (!old.isNull()) root.removeChild(old);
    QDomElement el = doc->createElement(tagRoot);
    for (const PieceSetup &p : pieces)
    {
        QDomElement e = doc->createElement(QStringLiteral("piece"));
        e.setAttribute(QStringLiteral("piece"), p.id);
        e.setAttribute(QStringLiteral("use"), p.use ? QStringLiteral("true") : QStringLiteral("false"));
        e.setAttribute(QStringLiteral("mode"), modeKey(p.mode));
        if (p.center1 != 0) e.setAttribute(QStringLiteral("center1"), p.center1);
        if (p.center2 != 0) e.setAttribute(QStringLiteral("center2"), p.center2);
        e.setAttribute(QStringLiteral("position"), positionKey(p.position));
        el.appendChild(e);
    }
    for (const SeamSetup &m : seams)
    {
        QDomElement e = doc->createElement(QStringLiteral("seam"));
        e.setAttribute(QStringLiteral("pieceA"), m.pieceA);
        e.setAttribute(QStringLiteral("a1"), m.a1);
        e.setAttribute(QStringLiteral("a2"), m.a2);
        e.setAttribute(QStringLiteral("pieceB"), m.pieceB);
        e.setAttribute(QStringLiteral("b1"), m.b1);
        e.setAttribute(QStringLiteral("b2"), m.b2);
        if (m.reverse) e.setAttribute(QStringLiteral("reverse"), QStringLiteral("true"));
        e.setAttribute(QStringLiteral("sides"), sidesKey(m.sides));
        el.appendChild(e);
    }
    root.appendChild(el);
    emit doc->patternChanged(false);
}

const PieceSetup *Setup::piece(quint32 id) const
{
    for (const PieceSetup &p : pieces)
        if (p.id == id) return &p;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
QVector<NodeInfo> pieceNodes(const VContainer *data, quint32 pieceId)
{
    QVector<NodeInfo> out;
    try
    {
        const VPiece piece = data->GetPiece(pieceId);
        for (const VPieceNode &node : piece.GetPath().getNodes())
        {
            if (node.GetTypeTool() != Tool::NodePoint || node.isExcluded()) continue;
            const QSharedPointer<VPointF> p = data->GeometricObject<VPointF>(node.GetId());
            out.append({node.GetId(), p->name()});
        }
    }
    catch (...)
    {
    }
    return out;
}

QString pieceName(const VContainer *data, quint32 pieceId)
{
    try
    {
        return data->GetPiece(pieceId).GetName();
    }
    catch (...)
    {
        return QString::number(pieceId);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
bool buildInput(const Setup &setup, const VContainer *data, Input &out, QString *error)
{
    out = Input();
    QMap<quint32, QVector<Copy>> copies;     // piece id -> its copies in the simulation
    QMap<quint32, QMap<quint32, int>> nodeIndex;   // piece id -> node id -> index in the outline of the half

    for (const PieceSetup &ps : setup.pieces)
    {
        if (!ps.use) continue;
        VPiece piece;
        try
        {
            piece = data->GetPiece(ps.id);
        }
        catch (...)
        {
            continue;
        }

        // the seam line, in cm
        const QVector<QPointF> path = piece.mainPathPoints(data);
        std::vector<sim::P2> pts;
        std::vector<QPointF> px;
        for (const QPointF &p : path)
        {
            if (!px.empty() && QLineF(px.back(), p).length() < 1e-3) continue;
            px.push_back(p);
            pts.push_back({FromPixel(p.x(), Unit::Cm), FromPixel(p.y(), Unit::Cm)});
        }
        if (px.size() > 2 && QLineF(px.front(), px.back()).length() < 1e-3)
        {
            px.pop_back();
            pts.pop_back();
        }
        if (pts.size() < 3) continue;

        QMap<quint32, int> &nodes = nodeIndex[ps.id];
        for (const VPieceNode &node : piece.GetPath().getNodes())
        {
            if (node.GetTypeTool() != Tool::NodePoint || node.isExcluded()) continue;
            const QPointF at = data->GeometricObject<VPointF>(node.GetId())->toQPointF();
            int best = -1;
            double bd = 1e18;
            for (size_t k = 0; k < px.size(); ++k)
            {
                const double d = QLineF(px[k], at).length();
                if (d < bd) { bd = d; best = int(k); }
            }
            if (best >= 0 && bd < 1.0) nodes.insert(node.GetId(), best);
        }

        // grain: the grainline of the piece, vertical when it has none
        double grain = 90;
        {
            bool ok = false;
            const double r = piece.GetGrainlineGeometry().getRotation().toDouble(&ok);
            if (ok && piece.GetGrainlineGeometry().IsVisible()) grain = -r;
        }

        const int n = int(pts.size());
        const bool hasCenter = nodes.contains(ps.center1) && nodes.contains(ps.center2) && ps.center1 != ps.center2;
        sim::P2 c1{0, 0}, c2{0, 0};
        double centroidX = 0, minY = 1e18;
        for (const sim::P2 &p : pts) { centroidX += p.x / n; minY = std::min(minY, p.y); }
        if (hasCenter)
        {
            c1 = pts[size_t(nodes.value(ps.center1))];
            c2 = pts[size_t(nodes.value(ps.center2))];
        }
        else if (ps.mode != Mode::Single)
        {
            out.warnings << QCoreApplication::translate("Prova3dSetup", "%1: no centre line, taken as whole")
                                .arg(piece.GetName());
        }
        const Mode mode = hasCenter ? ps.mode : Mode::Single;
        const double centerX = hasCenter ? (c1.x + c2.x) / 2 : centroidX;
        const double side = centroidX >= centerX ? 1.0 : -1.0;   // which way the half goes from the centre line
        const bool isBack = ps.position == Position::Back || ps.position == Position::BackWaist;
        const bool fromWaist = ps.position == Position::FrontWaist || ps.position == Position::BackWaist;

        auto makeInput = [&](const std::vector<sim::P2> &outline, const QString &suffix)
        {
            sim::PieceInput in;
            in.name = (piece.GetName() + suffix).toStdString();
            in.outline = outline;
            in.grainDeg = grain;
            in.centerX = centerX;
            in.angleDeg = isBack ? 180 : 0;
            // the half of copy 0 always goes to the same side of the body, front and back alike
            in.mirrored = isBack ? (side > 0) : (side < 0);
            in.topY = minY;
            in.height = fromWaist ? 102.0 : 143.0;
            return in;
        };

        QVector<Copy> list;
        if (mode == Mode::Fold)
        {
            const int i1 = nodes.value(ps.center1), i2 = nodes.value(ps.center2);
            auto walkStraight = [&](int from, int to)
            {
                for (int k = (from + 1) % n; k != to; k = (k + 1) % n)
                    if (distanceToLine(pts[size_t(k)], c1, c2) > 0.2) return false;
                return true;
            };
            // the fold is the straight stretch between the two nodes; the rest is the half to mirror
            int e0, e1;
            if (walkStraight(i1, i2)) { e0 = i2; e1 = i1; }
            else { e0 = i1; e1 = i2; }
            std::vector<int> half;
            for (int k = e0;; k = (k + 1) % n) { half.push_back(k); if (k == e1) break; }
            const int h = int(half.size());
            std::vector<sim::P2> full;
            for (int k : half) full.push_back(pts[size_t(k)]);
            for (int j = h - 2; j >= 1; --j) full.push_back(reflect(pts[size_t(half[size_t(j)])], c1, c2));
            Copy a, b;
            a.map.assign(size_t(n), -1);
            b.map.assign(size_t(n), -1);
            for (int q = 0; q < h; ++q)
            {
                a.map[size_t(half[size_t(q)])] = q;
                b.map[size_t(half[size_t(q)])] = (q == 0 || q == h - 1) ? q : h + (h - 2 - q);
            }
            a.index = b.index = int(out.pieces.size());
            out.pieces.push_back(makeInput(full, QString()));
            list << a << b;
        }
        else if (mode == Mode::Pair)
        {
            Copy a, b;
            a.map.resize(size_t(n));
            b.map.resize(size_t(n));
            std::vector<sim::P2> mirrored;
            for (int k = n - 1; k >= 0; --k) mirrored.push_back(reflect(pts[size_t(k)], c1, c2));
            for (int k = 0; k < n; ++k) { a.map[size_t(k)] = k; b.map[size_t(k)] = n - 1 - k; }
            a.index = int(out.pieces.size());
            out.pieces.push_back(makeInput(pts, QStringLiteral(" dx")));
            b.index = int(out.pieces.size());
            out.pieces.push_back(makeInput(mirrored, QStringLiteral(" sx")));
            list << a << b;
        }
        else
        {
            Copy a;
            a.map.resize(size_t(n));
            for (int k = 0; k < n; ++k) a.map[size_t(k)] = k;
            a.index = int(out.pieces.size());
            out.pieces.push_back(makeInput(pts, QString()));
            list << a;
        }
        copies.insert(ps.id, list);

        // pins on the centre line, a little inside the piece
        if (hasCenter && !fromWaist)
        {
            const double dx = c2.x - c1.x, dy = c2.y - c1.y;
            const double l = std::hypot(dx, dy);
            double nx = -dy / l, ny = dx / l;
            if ((centroidX - c1.x) * nx < 0) { nx = -nx; ny = -ny; }
            const double inset = mode == Mode::Fold ? 0.0 : 1.5;
            for (double t : {0.15, 0.85})
            {
                const sim::P2 p{c1.x + dx * t + nx * inset, c1.y + dy * t + ny * inset};
                out.pins.push_back({list.first().index, p});
                if (mode == Mode::Pair) out.pins.push_back({list.at(1).index, reflect(p, c1, c2)});
            }
        }
        else if (hasCenter)
        {
            const double dx = c2.x - c1.x, dy = c2.y - c1.y;
            const double l = std::hypot(dx, dy);
            double nx = -dy / l, ny = dx / l;
            if ((centroidX - c1.x) * nx < 0) { nx = -nx; ny = -ny; }
            const double inset = mode == Mode::Fold ? 0.0 : 1.5;
            const sim::P2 top = c1.y < c2.y ? c1 : c2;
            const sim::P2 p{top.x + nx * inset, top.y + 3.0 + ny * inset};
            out.pins.push_back({list.first().index, p});
            if (mode == Mode::Pair) out.pins.push_back({list.at(1).index, reflect(p, c1, c2)});
        }
    }

    if (out.pieces.empty())
    {
        if (error) *error = QCoreApplication::translate("Prova3dSetup", "No piece is set up for the 3D fitting");
        return false;
    }

    // seams: once on the half, on both halves unless separated
    for (const SeamSetup &m : setup.seams)
    {
        if (!copies.contains(m.pieceA) || !copies.contains(m.pieceB)) continue;
        const QVector<Copy> &ca = copies.value(m.pieceA), &cb = copies.value(m.pieceB);
        const QMap<quint32, int> &na = nodeIndex.value(m.pieceA), &nb = nodeIndex.value(m.pieceB);
        if (!na.contains(m.a1) || !na.contains(m.a2) || !nb.contains(m.b1) || !nb.contains(m.b2)) continue;
        const QString name = QStringLiteral("%1 · %2").arg(pieceName(data, m.pieceA), pieceName(data, m.pieceB));

        auto add = [&](int k, const QString &suffix)
        {
            const int ka = (k == 1 && ca.size() > 1) ? 1 : 0;
            const int kb = (k == 1 && cb.size() > 1) ? 1 : 0;
            const Copy &A = ca.at(ka);
            const Copy &B = cb.at(kb);
            const bool swapA = ka == 1, swapB = kb == 1;   // a mirrored half runs the other way round
            const quint32 aStart = swapA ? m.a2 : m.a1, aEnd = swapA ? m.a1 : m.a2;
            const quint32 bStart = swapA ? m.b2 : m.b1, bEnd = swapA ? m.b1 : m.b2;
            sim::SeamInput s;
            s.pieceA = A.index;
            s.pieceB = B.index;
            s.fromA = A.map[size_t(na.value(aStart))];
            s.toA = A.map[size_t(na.value(aEnd))];
            s.fromB = B.map[size_t(nb.value(bStart))];
            s.toB = B.map[size_t(nb.value(bEnd))];
            s.reverseB = m.reverse != swapA;
            if (swapB) s.reverseB = !s.reverseB;
            if (s.fromA < 0 || s.toA < 0 || s.fromB < 0 || s.toB < 0) return;
            s.name = (name + suffix).toStdString();
            s.autoDirection = true;
            out.seams.push_back(s);
        };

        const bool bothMirrored = ca.size() > 1 && cb.size() > 1;
        const bool anyMirrored = ca.size() > 1 || cb.size() > 1;
        switch (m.sides)
        {
            case Sides::Right:
                add(0, anyMirrored ? QStringLiteral(" dx") : QString());
                break;
            case Sides::Left:
                add(1, anyMirrored ? QStringLiteral(" sx") : QString());
                break;
            default:
                add(0, bothMirrored ? QStringLiteral(" dx") : QString());
                if (bothMirrored) add(1, QStringLiteral(" sx"));
                break;
        }
    }
    return true;
}

} // namespace prova3d
