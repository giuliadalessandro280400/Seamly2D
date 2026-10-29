/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   proportionalhandles.h
 **  @brief  Proportional curve handles: handle lengths follow the chord
 **          length so curves keep their shape across sizes.
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

#ifndef PROPORTIONALHANDLES_H
#define PROPORTIONALHANDLES_H

#include <QLineF>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtMath>

#include "../vmisc/vabstractapplication.h"
#include "../qmuparser/qmutokenparser.h"

/**
 * Proportional handles
 *
 * Normally the length of a curve handle is a fixed value. When the pattern is graded the end points move but the
 * handles keep their length, so a curve drawn nicely in one size looks too round or too square in the others.
 *
 * With proportional handles a curve remembers its chord (the distance between the two end points of a segment) in
 * the size where it was edited. The stored handle lengths belong to that reference chord; in every other size they
 * are multiplied by currentChord / referenceChord. The handle angles do not change.
 *
 * Simple curves store the reference in the attribute "refChord", curve paths store one value per segment in
 * "refChords" (separated by ';'). Values are in pattern units.
 */
namespace ProportionalHandles
{
inline const QString &attrRefChord()
{
    static const QString attr = QStringLiteral("refChord");
    return attr;
}

inline const QString &attrRefChords()
{
    static const QString attr = QStringLiteral("refChords");
    return attr;
}

/** @brief chord returns the distance between two points in pattern units. */
inline qreal chord(const QPointF &p1, const QPointF &p2)
{
    return qApp->fromPixel(QLineF(p1, p2).length());
}

/** @brief factor returns the multiplier for the handles of a segment, 1 when the feature is off. */
inline qreal factor(qreal currentChord, qreal referenceChord)
{
    if (referenceChord <= 0 || qFuzzyIsNull(referenceChord) || currentChord <= 0)
    {
        return 1.0;
    }
    return currentChord / referenceChord;
}

/** @brief isNumber true if a formula is a single number that can be safely rewritten. */
inline bool isNumber(const QString &formula)
{
    return qmu::QmuTokenParser::IsSingle(formula);
}

/** @brief toStored converts an actual handle length (pattern units) to the value stored for the reference chord. */
inline QString toStored(qreal actualLength, qreal currentChord, qreal referenceChord)
{
    return QString::number(actualLength / factor(currentChord, referenceChord));
}

inline QVector<qreal> parseList(const QString &text)
{
    QVector<qreal> values;
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts)
    {
        bool ok = false;
        const qreal value = part.toDouble(&ok);
        values.append(ok ? value : 0);
    }
    return values;
}

inline QString formatList(const QVector<qreal> &values)
{
    QStringList parts;
    for (const qreal value : values)
    {
        parts.append(QString::number(value));
    }
    return parts.join(QLatin1Char(';'));
}
} // namespace ProportionalHandles

#endif // PROPORTIONALHANDLES_H
