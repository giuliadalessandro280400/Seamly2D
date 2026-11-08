/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   aamadxf.cpp
 **  @brief  Reader of AAMA DXF pattern files (pieces, corners, curves, notches)
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

#include "aamadxf.h"

#include <QFile>
#include <QObject>
#include <QHash>
#include <QLineF>
#include <QTextStream>
#include <QtMath>

#include <cmath>

namespace
{
struct Pair
{
    int     code;
    QString value;
};

struct Entity
{
    QString          type;
    QString          layer;
    QVector<QPointF> points;   // vertices, or start/end, or position
    QString          text;
    QString          blockName; // INSERT
    double           scaleX {1};
    double           scaleY {1};
    double           rotation {0};
    bool             closed {false};
};

struct Block
{
    QString         name;
    QVector<Entity> entities;
};

//---------------------------------------------------------------------------------------------------------------------
QVector<Pair> readPairs(const QString &path, QString &error)
{
    QVector<Pair> pairs;
    QFile file(path);
    if (not file.open(QIODevice::ReadOnly))
    {
        error = QObject::tr("Can't open the file.");
        return pairs;
    }
    const QByteArray data = file.readAll();
    QString content = QString::fromUtf8(data);
    if (content.contains(QChar::ReplacementCharacter))
    {
        content = QString::fromLatin1(data); // older files use the local code page
    }
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (int i = 0; i + 1 < lines.size(); i += 2)
    {
        bool ok = false;
        const int code = lines.at(i).trimmed().toInt(&ok);
        if (not ok)
        {
            if (lines.at(i).trimmed().isEmpty())
            {
                --i; // tolerate empty lines
                continue;
            }
            error = QObject::tr("This is not a text DXF file (binary DXF is not supported).");
            pairs.clear();
            return pairs;
        }
        QString value = lines.at(i + 1);
        if (value.endsWith(QLatin1Char('\r')))
        {
            value.chop(1);
        }
        pairs.append({code, value.trimmed()});
    }
    return pairs;
}

//---------------------------------------------------------------------------------------------------------------------
// Reads the entity that starts at pairs[i] (a code 0); returns the index of the next code 0.
int readEntity(const QVector<Pair> &pairs, int i, Entity &entity)
{
    entity.type = pairs.at(i).value;
    ++i;
    double x = 0, y = 0;
    bool haveX = false;
    double x2 = 0, y2 = 0;
    bool haveSecond = false;
    while (i < pairs.size() && pairs.at(i).code != 0)
    {
        const Pair &p = pairs.at(i);
        switch (p.code)
        {
            case 8: entity.layer = p.value; break;
            case 1: entity.text = p.value; break;
            case 2: entity.blockName = p.value; break;
            case 10:
                if (entity.type == QLatin1String("LWPOLYLINE") && haveX)
                {
                    entity.points.append(QPointF(x, y));
                }
                x = p.value.toDouble();
                haveX = true;
                break;
            case 20: y = p.value.toDouble(); break;
            case 11: x2 = p.value.toDouble(); haveSecond = true; break;
            case 21: y2 = p.value.toDouble(); break;
            case 41: entity.scaleX = p.value.toDouble(); break;
            case 42: entity.scaleY = p.value.toDouble(); break;
            case 50: entity.rotation = p.value.toDouble(); break;
            case 70: entity.closed = (p.value.toInt() & 1) != 0; break;
            default: break;
        }
        ++i;
    }
    if (haveX)
    {
        entity.points.append(QPointF(x, y));
    }
    if (haveSecond)
    {
        entity.points.append(QPointF(x2, y2));
    }
    return i;
}

//---------------------------------------------------------------------------------------------------------------------
// Reads entities from pairs[i] until one of the end markers; POLYLINE + VERTEX + SEQEND become one entity.
int readEntities(const QVector<Pair> &pairs, int i, const QStringList &endMarkers, QVector<Entity> &entities)
{
    while (i < pairs.size())
    {
        if (pairs.at(i).code != 0)
        {
            ++i;
            continue;
        }
        if (endMarkers.contains(pairs.at(i).value))
        {
            return i;
        }
        Entity entity;
        i = readEntity(pairs, i, entity);
        if (entity.type == QLatin1String("POLYLINE"))
        {
            entity.points.clear(); // the position of a POLYLINE is not a vertex
            while (i < pairs.size() && pairs.at(i).code == 0 && pairs.at(i).value == QLatin1String("VERTEX"))
            {
                Entity vertex;
                i = readEntity(pairs, i, vertex);
                if (not vertex.points.isEmpty())
                {
                    entity.points.append(vertex.points.first());
                }
            }
            if (i < pairs.size() && pairs.at(i).value == QLatin1String("SEQEND"))
            {
                Entity end;
                i = readEntity(pairs, i, end);
            }
        }
        entities.append(entity);
    }
    return i;
}

//---------------------------------------------------------------------------------------------------------------------
QPointF transformed(const QPointF &p, const Entity &insert)
{
    const double a = qDegreesToRadians(insert.rotation);
    const double x = p.x() * insert.scaleX;
    const double y = p.y() * insert.scaleY;
    const QPointF base = insert.points.isEmpty() ? QPointF() : insert.points.first();
    return base + QPointF(x * std::cos(a) - y * std::sin(a), x * std::sin(a) + y * std::cos(a));
}

//---------------------------------------------------------------------------------------------------------------------
bool near(const QPointF &a, const QPointF &b, double tolerance)
{
    return QLineF(a, b).length() <= tolerance;
}

//---------------------------------------------------------------------------------------------------------------------
AamaPiece makePiece(const Block &block, const Entity &insert, double unit)
{
    AamaPiece piece;
    piece.name = block.name;

    const auto map = [&](const QPointF &p) { return transformed(p, insert) * unit; };

    // The outline: the polyline of layer 1 with most vertices
    const Entity *outline = nullptr;
    QVector<QPointF> turnPoints;
    QVector<QPointF> curvePoints;
    for (const Entity &e : block.entities)
    {
        const bool polyline = e.type == QLatin1String("POLYLINE") || e.type == QLatin1String("LWPOLYLINE");
        if (polyline && e.layer == QLatin1String("1") && (outline == nullptr
                                                           || e.points.size() > outline->points.size()))
        {
            outline = &e;
        }
        else if (e.type == QLatin1String("POINT") && not e.points.isEmpty())
        {
            if (e.layer == QLatin1String("2"))
            {
                turnPoints.append(map(e.points.first()));
            }
            else if (e.layer == QLatin1String("3"))
            {
                curvePoints.append(map(e.points.first()));
            }
            else if (e.layer == QLatin1String("4"))
            {
                piece.notches.append(map(e.points.first()));
            }
        }
        else if (e.type == QLatin1String("LINE") && e.points.size() >= 2)
        {
            if (e.layer == QLatin1String("4"))
            {
                piece.notches.append(map(e.points.first()));
            }
            else if (e.layer == QLatin1String("7"))
            {
                piece.grain = {map(e.points.at(0)), map(e.points.at(1))};
            }
            else if (e.layer == QLatin1String("8") || e.layer == QLatin1String("11"))
            {
                piece.internals.append({map(e.points.at(0)), map(e.points.at(1))});
            }
        }
        else if (polyline && (e.layer == QLatin1String("8") || e.layer == QLatin1String("11")) && e.points.size() >= 2)
        {
            QVector<QPointF> line;
            for (const QPointF &p : e.points)
            {
                line.append(map(p));
            }
            piece.internals.append(line);
        }
        else if ((e.type == QLatin1String("TEXT") || e.type == QLatin1String("MTEXT")) && not e.text.isEmpty())
        {
            piece.texts.append(e.text);
            if (e.text.startsWith(QLatin1String("Piece Name:"), Qt::CaseInsensitive))
            {
                piece.name = e.text.section(QLatin1Char(':'), 1).trimmed();
            }
        }
    }

    if (outline == nullptr || outline->points.size() < 3)
    {
        return AamaPiece();
    }

    for (const QPointF &p : outline->points)
    {
        const QPointF q = map(p);
        if (piece.boundary.isEmpty() || not near(piece.boundary.last(), q, 0.01))
        {
            piece.boundary.append(q); // a vertex repeated right after itself is dropped
        }
    }
    // a closed outline may repeat its first vertex at the end
    if (piece.boundary.size() > 3 && near(piece.boundary.first(), piece.boundary.last(), 0.01))
    {
        piece.boundary.removeLast();
    }

    const double tolerance = 0.05; // mm
    const bool marked = not turnPoints.isEmpty() || not curvePoints.isEmpty();
    const int n = piece.boundary.size();
    for (int i = 0; i < n; ++i)
    {
        const QPointF &p = piece.boundary.at(i);
        bool turn = false;
        if (marked)
        {
            for (const QPointF &t : turnPoints)
            {
                turn = turn || near(p, t, tolerance);
            }
            if (not turn)
            {
                bool curve = false;
                for (const QPointF &c : curvePoints)
                {
                    curve = curve || near(p, c, tolerance);
                }
                turn = not curve; // a vertex that is not marked as a curve point is a corner
            }
        }
        else
        {
            // No marks: a vertex is a corner where the outline turns sharply
            const QPointF &prev = piece.boundary.at((i + n - 1) % n);
            const QPointF &next = piece.boundary.at((i + 1) % n);
            const double a1 = QLineF(prev, p).angle();
            const double a2 = QLineF(p, next).angle();
            double change = std::fmod(std::fabs(a2 - a1), 360.0);
            change = qMin(change, 360.0 - change);
            turn = change > 30;
        }
        piece.turn.append(turn);
    }
    if (not piece.turn.contains(true))
    {
        piece.turn[0] = true;
    }
    return piece;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
AamaFile readAama(const QString &path, double unitMm)
{
    AamaFile file;
    const QVector<Pair> pairs = readPairs(path, file.error);
    if (not file.error.isEmpty())
    {
        return file;
    }

    QHash<QString, Block> blocks;
    QVector<Entity> inserts;
    QVector<Entity> loose; // entities drawn directly, not in a block
    int insUnits = -1;
    int measurement = -1; // $MEASUREMENT: 0 imperial, 1 metric
    QString unitsText;

    int i = 0;
    while (i < pairs.size())
    {
        if (pairs.at(i).code == 0 && pairs.at(i).value == QLatin1String("SECTION") && i + 1 < pairs.size())
        {
            const QString section = pairs.at(i + 1).value;
            i += 2;
            if (section == QLatin1String("HEADER"))
            {
                while (i < pairs.size() && not (pairs.at(i).code == 0 && pairs.at(i).value == QLatin1String("ENDSEC")))
                {
                    if (pairs.at(i).code == 9 && pairs.at(i).value == QLatin1String("$INSUNITS") && i + 1 < pairs.size())
                    {
                        insUnits = pairs.at(i + 1).value.toInt();
                    }
                    if (pairs.at(i).code == 9 && pairs.at(i).value == QLatin1String("$MEASUREMENT") && i + 1 < pairs.size())
                    {
                        measurement = pairs.at(i + 1).value.toInt();
                    }
                    ++i;
                }
            }
            else if (section == QLatin1String("BLOCKS"))
            {
                while (i < pairs.size() && not (pairs.at(i).code == 0 && pairs.at(i).value == QLatin1String("ENDSEC")))
                {
                    if (pairs.at(i).code == 0 && pairs.at(i).value == QLatin1String("BLOCK"))
                    {
                        Entity header;
                        i = readEntity(pairs, i, header);
                        Block block;
                        block.name = header.blockName;
                        i = readEntities(pairs, i, {QStringLiteral("ENDBLK"), QStringLiteral("ENDSEC")}, block.entities);
                        if (i < pairs.size() && pairs.at(i).value == QLatin1String("ENDBLK"))
                        {
                            Entity end;
                            i = readEntity(pairs, i, end);
                        }
                        blocks.insert(block.name, block);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            else if (section == QLatin1String("ENTITIES"))
            {
                QVector<Entity> entities;
                i = readEntities(pairs, i, {QStringLiteral("ENDSEC")}, entities);
                for (const Entity &e : entities)
                {
                    if (e.type == QLatin1String("INSERT"))
                    {
                        inserts.append(e);
                    }
                    else
                    {
                        if (e.type == QLatin1String("TEXT") && e.text.contains(QLatin1String("UNITS"), Qt::CaseInsensitive))
                        {
                            unitsText = e.text;
                        }
                        loose.append(e);
                    }
                }
            }
        }
        else
        {
            ++i;
        }
    }

    // Units
    double unit = 1; // mm
    if (insUnits == 1 || unitsText.contains(QLatin1String("ENGLISH"), Qt::CaseInsensitive)
        || (insUnits <= 0 && unitsText.isEmpty() && measurement == 0))
    {
        unit = 25.4;
        file.units = QStringLiteral("inch");
    }
    else if (insUnits == 5)
    {
        unit = 10;
        file.units = QStringLiteral("cm");
    }
    else if (insUnits == 6)
    {
        unit = 1000;
        file.units = QStringLiteral("m");
    }
    else if (insUnits == 4 || unitsText.contains(QLatin1String("METRIC"), Qt::CaseInsensitive)
             || (insUnits <= 0 && measurement == 1))
    {
        file.units = QStringLiteral("mm");
    }
    if (unitMm > 0)
    {
        unit = unitMm;
    }

    for (const Entity &insert : inserts)
    {
        if (not blocks.contains(insert.blockName))
        {
            continue;
        }
        AamaPiece piece = makePiece(blocks.value(insert.blockName), insert, unit);
        if (not piece.boundary.isEmpty())
        {
            file.pieces.append(piece);
        }
    }

    // A file with the outlines drawn directly (no blocks): one piece
    if (file.pieces.isEmpty() && not loose.isEmpty())
    {
        Block block;
        block.name = QStringLiteral("PIECE");
        block.entities = loose;
        Entity identity;
        identity.points = {QPointF()};
        AamaPiece piece = makePiece(block, identity, unit);
        if (not piece.boundary.isEmpty())
        {
            file.pieces.append(piece);
        }
    }

    if (file.pieces.isEmpty())
    {
        file.error = QObject::tr("No pattern piece found: an AAMA file has the outline of each piece on layer 1.");
    }
    return file;
}
