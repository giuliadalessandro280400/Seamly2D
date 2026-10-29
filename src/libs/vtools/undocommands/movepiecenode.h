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

#endif // MOVEPIECENODE_H
