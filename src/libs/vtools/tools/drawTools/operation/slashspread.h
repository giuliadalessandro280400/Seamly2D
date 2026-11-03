/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   slashspread.h
 **  @brief  Slash and spread: the geometry shared by the tool and its preview
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

#ifndef SLASHSPREAD_H
#define SLASHSPREAD_H

#include <QPointF>
#include <QSharedPointer>
#include <QString>
#include <QVector>

class VContainer;
class VAbstractCurve;
struct SourceItem;

/**
 * @brief The SlashSpread class opens a part of a pattern like slashing the paper from the edge to a hinge point and
 * spreading it.
 *
 * The slashes go from the hinge to points evenly spaced along the edge to open. Each object is in one of the sections
 * between the slashes and turns around the hinge with its section: every slash opens so that the edge gets longer by
 * the same amount. One part stays where it is: the one at the first point of the edge, the one at the last point, or
 * the middle (both sides open). A curve cut by a slash is redrawn as one smooth curve through its opened pieces.
 */
class SlashSpread
{
public:
    enum class Fixed { Start = 0, End = 1, Both = 2 };

    SlashSpread(const QPointF &hinge, const QVector<QPointF> &edge, int slashes, qreal spread, bool percent,
                Fixed fixed);

    bool             isValid() const { return m_valid; }
    QPointF          hinge() const { return m_hinge; }
    QVector<QPointF> slashEnds() const { return m_slashEnds; }
    qreal            totalSpread() const { return m_total; }   // pixels

    int              section(const QPointF &p) const;
    qreal            rotation(int section) const;              // degrees, as QLineF angles
    QPointF          map(const QPointF &p) const;

    /** @brief opened returns a new curve (the caller owns it) after the spread: turned whole if it is in one section,
     * else redrawn as one smooth curve. */
    VAbstractCurve  *opened(const QSharedPointer<VAbstractCurve> &curve, const QString &suffix) const;

    /** @brief edgeOf the polyline of the edge: the curve among the objects that joins the two points, or the line. */
    static QVector<QPointF> edgeOf(const VContainer *data, quint32 startId, quint32 endId,
                                   const QVector<quint32> &objects);

    static Fixed     fixedFromString(const QString &value);
    static QString   fixedToString(Fixed value);

private:
    QPointF          m_hinge;
    QVector<QPointF> m_slashEnds;
    QVector<qreal>   m_slashPositions;   // position of each slash around the hinge (degrees from the edge start)
    QVector<qreal>   m_rotations;        // rotation of each section (degrees)
    qreal            m_startAngle {0};
    qreal            m_sweep {1};        // +1 / -1: which way the edge goes around the hinge
    qreal            m_total {0};
    bool             m_valid {false};

    qreal            fanPosition(const QPointF &p) const;
};

#endif // SLASHSPREAD_H
