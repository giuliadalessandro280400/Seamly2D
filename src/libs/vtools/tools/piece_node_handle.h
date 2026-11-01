/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   piece_node_handle.h
 **  @brief  Draggable handle of a point in the piece editor.
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

#ifndef PIECE_NODE_HANDLE_H
#define PIECE_NODE_HANDLE_H

#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QObject>
#include <QPointF>

/**
 * @brief The PieceNodeHandle class is a small square drawn on a point of a selected pattern piece.
 * Dragging it moves the point of the piece only (the draft does not change). Double click puts the point back
 * where the draft has it.
 */
class PieceNodeHandle : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    PieceNodeHandle(quint32 nodeId, QGraphicsItem *parent);
    virtual ~PieceNodeHandle() = default;

    quint32      nodeId() const { return m_nodeId; }
    bool         isDragging() const { return m_dragging; }
    void         setModified(bool modified);
    void         setPinned(bool pinned);
    bool         isPinned() const { return m_pinned; }

signals:
    void         dragStarted(quint32 nodeId);
    void         moved(quint32 nodeId, const QPointF &position);  // position in parent (piece) coordinates
    void         dragFinished(quint32 nodeId);
    void         resetRequested(quint32 nodeId);
    void         pinToggled(quint32 nodeId);

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    Q_DISABLE_COPY(PieceNodeHandle)
    quint32 m_nodeId;
    bool    m_dragging;
    bool    m_modified;
    bool    m_pinned;
    QPointF m_grabOffset;

    void    updateLook(bool hover);
};

/**
 * @brief The PieceSegmentHandle class lets the user drag a line of the contour of a selected piece: both end points
 * move together (and the contour follows up to the pins).
 */
class PieceSegmentHandle : public QObject, public QGraphicsPathItem
{
    Q_OBJECT
public:
    PieceSegmentHandle(quint32 firstNodeId, quint32 secondNodeId, QGraphicsItem *parent);
    virtual ~PieceSegmentHandle() = default;

    quint32      firstNodeId() const { return m_firstNodeId; }
    quint32      secondNodeId() const { return m_secondNodeId; }
    bool         isDragging() const { return m_dragging; }
    void         setSegment(const QPainterPath &path);

    virtual QPainterPath shape() const override;

signals:
    void         dragStarted(quint32 firstNodeId, quint32 secondNodeId);
    void         moved(quint32 firstNodeId, quint32 secondNodeId, const QPointF &delta); // in piece coordinates
    void         dragFinished();

protected:
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    Q_DISABLE_COPY(PieceSegmentHandle)
    quint32 m_firstNodeId;
    quint32 m_secondNodeId;
    bool    m_dragging;
    QPointF m_startScenePos;

    void    updateLook(bool hover);
};

#endif // PIECE_NODE_HANDLE_H
