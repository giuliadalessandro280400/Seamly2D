/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   piece_curve_edit.cpp
 **  @brief  Piece editor: edits of the curves of a piece (inner points, pins, simplify)
 **          stored on the nodes of the piece, re-applied in every size.
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

#include "piece_curve_edit.h"

#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/ifcdef.h"
#include "../vgeometry/vpointf.h"
#include "../vgeometry/vsplinepath.h"
#include "../vgeometry/vsplinepoint.h"
#include "../vmisc/vabstractapplication.h"

#include <QLineF>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace PieceCurveEdit
{
const QString AttrSimplify     = QStringLiteral("simplify");
const QString AttrCurveOffsets = QStringLiteral("curveOffsets");
const QString AttrCurvePins    = QStringLiteral("curvePins");
const QString AttrBend         = QStringLiteral("bend");

//---------------------------------------------------------------------------------------------------------------------
QDomElement findNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId)
{
    if (doc == nullptr)
    {
        return QDomElement();
    }
    const QDomElement piece = doc->elementById(pieceId, VAbstractPattern::TagPiece);
    if (not piece.isElement())
    {
        return QDomElement();
    }
    QDomElement node = piece.firstChildElement(VAbstractPattern::TagNodes).firstChildElement(VAbstractPattern::TagNode);
    while (not node.isNull())
    {
        if (node.attribute(AttrIdObject).toUInt() == nodeId)
        {
            return node;
        }
        node = node.nextSiblingElement(VAbstractPattern::TagNode);
    }
    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
CurveEdit readCurveEdit(const QMap<QString, QString> &node)
{
    CurveEdit edit;
    const QString simplify = node.value(AttrSimplify);
    if (not simplify.isEmpty())
    {
        edit.simplified = true;
        if (simplify != QLatin1String("-"))
        {
            for (const QString &part : simplify.split(QLatin1Char(';'), Qt::SkipEmptyParts))
            {
                bool ok = false;
                const qreal f = part.toDouble(&ok);
                if (ok && f > 0 && f < 1)
                {
                    edit.splits.append(f);
                }
            }
            std::sort(edit.splits.begin(), edit.splits.end());
        }
    }

    for (const QString &part : node.value(AttrCurveOffsets).split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        const QStringList indexAndValue = part.split(QLatin1Char(':'));
        if (indexAndValue.size() != 2)
        {
            continue;
        }
        const QStringList xy = indexAndValue.at(1).split(QLatin1Char(','));
        bool ok1 = false, ok2 = false, ok3 = false;
        const int index = indexAndValue.at(0).toInt(&ok1);
        const qreal x = xy.value(0).toDouble(&ok2);
        const qreal y = xy.value(1).toDouble(&ok3);
        if (ok1 && ok2 && ok3)
        {
            edit.offsets.insert(index, QPointF(x, y));
        }
    }

    for (const QString &part : node.value(AttrCurvePins).split(QLatin1Char(';'), Qt::SkipEmptyParts))
    {
        bool ok = false;
        const int index = part.toInt(&ok);
        if (ok && not edit.pins.contains(index))
        {
            edit.pins.append(index);
        }
    }
    std::sort(edit.pins.begin(), edit.pins.end());
    return edit;
}

//---------------------------------------------------------------------------------------------------------------------
bool readBend(const QMap<QString, QString> &node)
{
    return node.value(AttrBend) == QLatin1String("true");
}

//---------------------------------------------------------------------------------------------------------------------
QString formatSplits(bool simplified, const QVector<qreal> &splits)
{
    if (not simplified)
    {
        return QString();
    }
    if (splits.isEmpty())
    {
        return QStringLiteral("-");
    }
    QStringList parts;
    for (const qreal f : splits)
    {
        parts.append(QString::number(f, 'f', 5));
    }
    return parts.join(QLatin1Char(';'));
}

//---------------------------------------------------------------------------------------------------------------------
QString formatOffsets(const QMap<int, QPointF> &offsets)
{
    QStringList parts;
    for (auto i = offsets.cbegin(); i != offsets.cend(); ++i)
    {
        if (qAbs(i.value().x()) < 1e-4 && qAbs(i.value().y()) < 1e-4)
        {
            continue;
        }
        parts.append(QStringLiteral("%1:%2,%3").arg(i.key()).arg(i.value().x(), 0, 'f', 4)
                     .arg(i.value().y(), 0, 'f', 4));
    }
    return parts.join(QLatin1Char(';'));
}

//---------------------------------------------------------------------------------------------------------------------
QString formatPins(const QVector<int> &pins)
{
    QStringList parts;
    for (const int i : pins)
    {
        parts.append(QString::number(i));
    }
    return parts.join(QLatin1Char(';'));
}

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// A polyline measured by its length
struct Polyline
{
    QVector<QPointF> points;
    QVector<qreal>   lengths; // cumulative
    qreal            total {0};

    explicit Polyline(const QVector<QPointF> &p) : points(p)
    {
        lengths.reserve(points.size());
        qreal sum = 0;
        for (int i = 0; i < points.size(); ++i)
        {
            if (i > 0)
            {
                sum += QLineF(points.at(i - 1), points.at(i)).length();
            }
            lengths.append(sum);
        }
        total = sum;
    }

    QPointF at(qreal fraction) const
    {
        if (points.isEmpty())
        {
            return QPointF();
        }
        const qreal s = qBound(0.0, fraction, 1.0) * total;
        auto it = std::lower_bound(lengths.cbegin(), lengths.cend(), s);
        int i = static_cast<int>(it - lengths.cbegin());
        if (i <= 0)
        {
            return points.first();
        }
        if (i >= points.size())
        {
            return points.last();
        }
        const qreal segment = lengths.at(i) - lengths.at(i - 1);
        const qreal k = segment > 0 ? (s - lengths.at(i - 1)) / segment : 0;
        return points.at(i - 1) + (points.at(i) - points.at(i - 1)) * k;
    }

    QPointF tangent(qreal fraction) const
    {
        const qreal h = total > 0 ? qMin(0.02, 3.0 / total) : 0.01;
        QPointF a = at(qMax(0.0, fraction - h));
        QPointF b = at(qMin(1.0, fraction + h));
        QPointF d = b - a;
        qreal len = std::hypot(d.x(), d.y());
        if (len < 1e-9)
        {
            d = points.last() - points.first();
            len = std::hypot(d.x(), d.y());
        }
        return len > 1e-9 ? d / len : QPointF(1, 0);
    }
};

//---------------------------------------------------------------------------------------------------------------------
qreal dot(const QPointF &a, const QPointF &b)
{
    return a.x() * b.x() + a.y() * b.y();
}

//---------------------------------------------------------------------------------------------------------------------
QPointF bezier(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, qreal u)
{
    const qreal v = 1 - u;
    return p0 * (v * v * v) + p1 * (3 * v * v * u) + p2 * (3 * v * u * u) + p3 * (u * u * u);
}

//---------------------------------------------------------------------------------------------------------------------
struct SegmentFit
{
    qreal   alpha1 {0};
    qreal   alpha2 {0};
    qreal   maxError {0};
    qreal   maxErrorFraction {0};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief fitSegment one cubic curve from fraction f0 to f1 with given end directions, least squares (P. Schneider,
 * "An algorithm for automatically fitting digitized curves", Graphics Gems 1990).
 */
SegmentFit fitSegment(const Polyline &line, qreal f0, qreal f1, const QPointF &t0, const QPointF &t1)
{
    SegmentFit fit;
    const QPointF p0 = line.at(f0);
    const QPointF p3 = line.at(f1);
    const QPointF tHat1 = t0;
    const QPointF tHat2 = -t1;
    const qreal chord = QLineF(p0, p3).length();
    const qreal segmentLength = (f1 - f0) * line.total;

    const int n = qBound(16, static_cast<int>(segmentLength / 2.0), 200);
    QVector<QPointF> samples(n);
    QVector<qreal> fractions(n);
    QVector<qreal> u(n);
    for (int i = 0; i < n; ++i)
    {
        fractions[i] = f0 + (f1 - f0) * i / (n - 1);
        samples[i] = line.at(fractions[i]);
        u[i] = static_cast<qreal>(i) / (n - 1);
    }

    auto solve = [&]()
    {
        qreal c11 = 0, c12 = 0, c22 = 0, x1 = 0, x2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const qreal v = 1 - u[i];
            const qreal b0 = v * v * v, b1 = 3 * v * v * u[i], b2 = 3 * v * u[i] * u[i], b3 = u[i] * u[i] * u[i];
            const QPointF a1 = tHat1 * b1;
            const QPointF a2 = tHat2 * b2;
            c11 += dot(a1, a1);
            c12 += dot(a1, a2);
            c22 += dot(a2, a2);
            const QPointF tmp = samples.at(i) - (p0 * (b0 + b1) + p3 * (b2 + b3));
            x1 += dot(a1, tmp);
            x2 += dot(a2, tmp);
        }
        const qreal det = c11 * c22 - c12 * c12;
        qreal alpha1 = 0, alpha2 = 0;
        if (qAbs(det) > 1e-12)
        {
            alpha1 = (x1 * c22 - c12 * x2) / det;
            alpha2 = (c11 * x2 - c12 * x1) / det;
        }
        const qreal eps = 1e-6 * qMax(chord, 1.0);
        if (alpha1 < eps || alpha2 < eps)
        {
            alpha1 = alpha2 = chord / 3.0;
        }
        fit.alpha1 = alpha1;
        fit.alpha2 = alpha2;
    };

    auto control = [&](QPointF &p1, QPointF &p2)
    {
        p1 = p0 + tHat1 * fit.alpha1;
        p2 = p3 + tHat2 * fit.alpha2;
    };

    solve();
    // A few Newton steps move the parameters closer to the real nearest points, then fit again
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        QPointF p1, p2;
        control(p1, p2);
        for (int i = 1; i < n - 1; ++i)
        {
            const qreal t = u[i];
            const qreal v = 1 - t;
            const QPointF q = bezier(p0, p1, p2, p3, t);
            const QPointF q1 = (p1 - p0) * (3 * v * v) + (p2 - p1) * (6 * v * t) + (p3 - p2) * (3 * t * t);
            const QPointF q2 = (p2 - p1 * 2 + p0) * (6 * v) + (p3 - p2 * 2 + p1) * (6 * t);
            const QPointF diff = q - samples.at(i);
            const qreal numerator = dot(diff, q1);
            const qreal denominator = dot(q1, q1) + dot(diff, q2);
            if (qAbs(denominator) > 1e-12)
            {
                u[i] = qBound(0.0, t - numerator / denominator, 1.0);
            }
        }
        solve();
    }

    QPointF p1, p2;
    control(p1, p2);
    // Real distance of each sample from the new curve (nearest point of a fine polyline of it)
    const int m = 96;
    QVector<QPointF> curve(m + 1);
    for (int k = 0; k <= m; ++k)
    {
        curve[k] = bezier(p0, p1, p2, p3, static_cast<qreal>(k) / m);
    }
    for (int i = 0; i < n; ++i)
    {
        const QPointF &q = samples.at(i);
        qreal error = (std::numeric_limits<qreal>::max)();
        for (int k = 0; k < m; ++k)
        {
            const QPointF ab = curve.at(k + 1) - curve.at(k);
            const qreal len2 = dot(ab, ab);
            const qreal t = len2 > 0 ? qBound(0.0, dot(q - curve.at(k), ab) / len2, 1.0) : 0;
            error = qMin(error, QLineF(q, curve.at(k) + ab * t).length());
        }
        if (error > fit.maxError)
        {
            fit.maxError = error;
            fit.maxErrorFraction = fractions.at(i);
        }
    }
    return fit;
}

//---------------------------------------------------------------------------------------------------------------------
QPointF tangentAt(const Polyline &line, qreal f)
{
    return line.tangent(f);
}

//---------------------------------------------------------------------------------------------------------------------
void split(const Polyline &line, qreal f0, qreal f1, qreal tolerance, int depth, QVector<qreal> &splits)
{
    const SegmentFit fit = fitSegment(line, f0, f1, tangentAt(line, f0), tangentAt(line, f1));
    if (fit.maxError <= tolerance || depth >= 8 || (f1 - f0) * line.total < 4 * tolerance)
    {
        return;
    }
    const qreal margin = (f1 - f0) * 0.15;
    const qreal at = qBound(f0 + margin, fit.maxErrorFraction, f1 - margin);
    split(line, f0, at, tolerance, depth + 1, splits);
    splits.append(at);
    split(line, at, f1, tolerance, depth + 1, splits);
}

//---------------------------------------------------------------------------------------------------------------------
qreal normalizedAngle(qreal angle)
{
    angle = std::fmod(angle, 360.0);
    return angle < 0 ? angle + 360.0 : angle;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
QVector<qreal> computeSplits(const QVector<QPointF> &points, qreal tolerance)
{
    QVector<qreal> splits;
    if (points.size() < 3)
    {
        return splits;
    }
    const Polyline line(points);
    if (line.total <= 0)
    {
        return splits;
    }
    const qreal tol = qMax(tolerance, 0.01);
    split(line, 0, 1, tol, 0, splits);

    // Move each point along the curve to where the curves around it fit best
    const auto optimise = [&]()
    {
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < splits.size(); ++i)
            {
                const qreal f0 = i == 0 ? 0 : splits.at(i - 1);
                const qreal f1 = i == splits.size() - 1 ? 1 : splits.at(i + 1);
                const auto errorAt = [&](qreal f)
                {
                    return qMax(fitSegment(line, f0, f, tangentAt(line, f0), tangentAt(line, f)).maxError,
                                fitSegment(line, f, f1, tangentAt(line, f), tangentAt(line, f1)).maxError);
                };
                qreal bestF = splits.at(i);
                qreal bestError = errorAt(bestF);
                for (int k = 1; k < 24; ++k)
                {
                    const qreal f = f0 + (f1 - f0) * k / 24.0;
                    const qreal error = errorAt(f);
                    if (error < bestError)
                    {
                        bestError = error;
                        bestF = f;
                    }
                }
                splits[i] = bestF;
            }
        }
    };

    // Drop every point that is not needed: try to join the two curves around it
    const auto reduce = [&]()
    {
        bool removed = true;
        while (removed && not splits.isEmpty())
        {
            removed = false;
            int best = -1;
            qreal bestError = tol;
            for (int i = 0; i < splits.size(); ++i)
            {
                const qreal f0 = i == 0 ? 0 : splits.at(i - 1);
                const qreal f1 = i == splits.size() - 1 ? 1 : splits.at(i + 1);
                const qreal error = fitSegment(line, f0, f1, tangentAt(line, f0), tangentAt(line, f1)).maxError;
                if (error <= bestError)
                {
                    bestError = error;
                    best = i;
                }
            }
            if (best >= 0)
            {
                splits.removeAt(best);
                removed = true;
            }
        }
    };

    optimise();
    reduce();
    // With the points in better places, one fewer may be enough
    for (int attempt = 0; attempt < 3 && splits.size() > 0; ++attempt)
    {
        const QVector<qreal> before = splits;
        // try without each point, placing the others at best, and keep it if within the tolerance
        bool improved = false;
        for (int i = 0; i < before.size() && not improved; ++i)
        {
            splits = before;
            splits.removeAt(i);
            optimise();
            qreal worst = 0;
            for (int k = 0; k <= splits.size(); ++k)
            {
                const qreal f0 = k == 0 ? 0 : splits.at(k - 1);
                const qreal f1 = k == splits.size() ? 1 : splits.at(k);
                worst = qMax(worst, fitSegment(line, f0, f1, tangentAt(line, f0), tangentAt(line, f1)).maxError);
            }
            improved = worst <= tol;
        }
        if (not improved)
        {
            splits = before;
            break;
        }
    }
    optimise();
    return splits;
}

//---------------------------------------------------------------------------------------------------------------------
VSplinePath fitCurve(const QVector<QPointF> &points, const QVector<qreal> &splits)
{
    const Polyline line(points);
    QVector<qreal> fractions;
    fractions.append(0);
    for (const qreal f : splits)
    {
        if (f > fractions.last() + 1e-6 && f < 1 - 1e-6)
        {
            fractions.append(f);
        }
    }
    fractions.append(1);

    QVector<QPointF> tangents;
    for (const qreal f : fractions)
    {
        tangents.append(tangentAt(line, f));
    }

    QVector<qreal> incoming(fractions.size(), 0); // length of the handle before each point
    QVector<qreal> outgoing(fractions.size(), 0); // length of the handle after each point
    for (int i = 0; i + 1 < fractions.size(); ++i)
    {
        const SegmentFit fit = fitSegment(line, fractions.at(i), fractions.at(i + 1), tangents.at(i),
                                          tangents.at(i + 1));
        outgoing[i] = fit.alpha1;
        incoming[i + 1] = fit.alpha2;
    }

    QVector<VSplinePoint> splinePoints;
    for (int i = 0; i < fractions.size(); ++i)
    {
        const QPointF p = line.at(fractions.at(i));
        const qreal angle2 = normalizedAngle(QLineF(p, p + tangents.at(i)).angle());
        const qreal angle1 = normalizedAngle(angle2 + 180);
        splinePoints.append(VSplinePoint(VPointF(p), angle1, QString::number(angle1), angle2,
                                         QString::number(angle2), incoming.at(i),
                                         QString::number(qApp->fromPixel(incoming.at(i))), outgoing.at(i),
                                         QString::number(qApp->fromPixel(outgoing.at(i)))));
    }
    return VSplinePath(splinePoints);
}
} // namespace PieceCurveEdit
