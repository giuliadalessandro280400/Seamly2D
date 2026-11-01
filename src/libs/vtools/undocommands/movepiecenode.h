/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   movepiecenode.h
 **  @brief  Undo command: moves a point of a pattern piece in the piece editor
 **          without changing the draft (stores an offset on the piece node).
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

#ifndef MOVEPIECENODE_H
#define MOVEPIECENODE_H

#include <QMap>
#include <QPointF>
#include <QtGlobal>

#include "vundocommand.h"

/**
 * @brief The MovePieceNode class changes the manual offset of one point of a pattern piece.
 * The offset is stored on the <node> of the piece, in pattern units. Consecutive moves of the same point merge into
 * one undo step.
 */
class MovePieceNode : public VUndoCommand
{
    Q_OBJECT
public:
    MovePieceNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId, const QPointF &newOffset,
                  QUndoCommand *parent = nullptr);
    virtual ~MovePieceNode() = default;

    virtual void undo() override;
    virtual void redo() override;
    virtual bool mergeWith(const QUndoCommand *command) override;
    virtual int  id() const override;

    /** @brief writeOffset stores an offset on a piece node; returns false if the node was not found. */
    static bool  writeOffset(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId, const QPointF &offset);
    static QPointF readOffset(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId);

private:
    Q_DISABLE_COPY(MovePieceNode)
    quint32 m_pieceId;
    QPointF m_oldOffset;
    QPointF m_newOffset;
};

/**
 * @brief The MovePieceNodes class changes the offsets of several points of a piece at once (a point dragged with the
 * points that follow it up to the pins, or a dragged line). Moves with the same drag key merge into one undo step.
 */
class MovePieceNodes : public VUndoCommand
{
    Q_OBJECT
public:
    MovePieceNodes(VAbstractPattern *doc, quint32 pieceId, const QMap<quint32, QPointF> &newOffsets, int dragKey,
                   QUndoCommand *parent = nullptr);
    virtual ~MovePieceNodes() = default;

    virtual void undo() override;
    virtual void redo() override;
    virtual bool mergeWith(const QUndoCommand *command) override;
    virtual int  id() const override;

private:
    Q_DISABLE_COPY(MovePieceNodes)
    quint32                 m_pieceId;
    int                     m_dragKey;
    QMap<quint32, QPointF>  m_oldOffsets;
    QMap<quint32, QPointF>  m_newOffsets;

    void apply(const QMap<quint32, QPointF> &offsets);
};

/**
 * @brief The PinPieceNode class pins or unpins a point of a piece in the piece editor.
 */
class PinPieceNode : public VUndoCommand
{
    Q_OBJECT
public:
    PinPieceNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId, bool pinned, QUndoCommand *parent = nullptr);
    virtual ~PinPieceNode() = default;

    virtual void undo() override;
    virtual void redo() override;

private:
    Q_DISABLE_COPY(PinPieceNode)
    quint32 m_pieceId;
    bool    m_pinned;

    void apply(bool pinned);
};

#endif // MOVEPIECENODE_H
