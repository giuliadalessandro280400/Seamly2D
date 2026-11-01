/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   piece_node_handle.cpp
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

#include "piece_node_handle.h"

#include <QBrush>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QMenu>
#include <QPainterPathStroker>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace
{
const qreal handleSize = 12; // pixels on screen
}

//---------------------------------------------------------------------------------------------------------------------
PieceNodeHandle::PieceNodeHandle(quint32 nodeId, QGraphicsItem *parent)
    : QObject()
    , QGraphicsRectItem(-handleSize/2, -handleSize/2, handleSize, handleSize, parent)
    , m_nodeId(nodeId)
    , m_dragging(false)
    , m_modified(false)
    , m_pinned(false)
    , m_grabOffset()
{
    // Same mechanism Seamly uses for the handles of curves: Qt moves the item, we follow its position.
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true); // same size at every zoom
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::SizeAllCursor);
    setZValue(1000); // above the point marks of the piece (z 100)
    setToolTip(tr("Drag to move this point of the piece (the draft does not change). "
                  "Double click to put it back."));
    updateLook(false);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::setModified(bool modified)
{
    m_modified = modified;
    updateLook(false);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::setPinned(bool pinned)
{
    m_pinned = pinned;
    setCursor(pinned ? Qt::ForbiddenCursor : Qt::SizeAllCursor);
    updateLook(false);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::updateLook(bool hover)
{
    if (m_pinned)
    {
        // A pin: green, filled, it does not move
        QPen pen(QColor(20, 150, 60));
        pen.setCosmetic(true);
        pen.setWidthF(2);
        setPen(pen);
        setBrush(QColor(20, 150, 60));
        return;
    }
    QPen pen(m_modified ? QColor(200, 40, 40) : QColor(30, 90, 200));
    pen.setCosmetic(true);
    pen.setWidthF(2);
    setPen(pen);
    setBrush(hover || m_dragging ? pen.color() : QColor(Qt::white));
}

//---------------------------------------------------------------------------------------------------------------------
QVariant PieceNodeHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged && m_dragging && not m_pinned)
    {
        // The piece recalculates while we move: avoid re-entering while it does.
        static bool changeFinished = true;
        if (changeFinished)
        {
            changeFinished = false;
            emit moved(m_nodeId, value.toPointF());
            changeFinished = true;
        }
    }
    return QGraphicsRectItem::itemChange(change, value);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_pinned)
    {
        event->accept(); // a pinned point does not move
        return;
    }
    if (event->button() == Qt::LeftButton && event->type() != QEvent::GraphicsSceneMouseDoubleClick)
    {
        m_dragging = true;
        m_grabOffset = pos() - parentItem()->mapFromScene(event->scenePos());
        updateLook(true);
        event->accept();
        emit dragStarted(m_nodeId);
    }
    QGraphicsRectItem::mousePressEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (not m_dragging)
    {
        QGraphicsRectItem::mouseMoveEvent(event);
        return;
    }
    // Follow the cursor in piece coordinates. The default movement of Qt does not work well with items that ignore
    // the zoom, so the position is computed here; itemChange() reports it.
    setPos(parentItem()->mapFromScene(event->scenePos()) + m_grabOffset);
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);
    const bool wasDragging = m_dragging;
    m_dragging = false;
    updateLook(isUnderMouse());
    if (wasDragging)
    {
        emit dragFinished(m_nodeId);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *pin = menu.addAction(m_pinned ? tr("Unpin Point") : tr("Pin Point"));
    QAction *reset = menu.addAction(tr("Put Back Where the Draft Has It"));
    reset->setEnabled(m_modified);
    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == pin)
    {
        emit pinToggled(m_nodeId);
    }
    else if (chosen == reset)
    {
        emit resetRequested(m_nodeId);
    }
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_pinned)
    {
        event->accept();
        return;
    }
    m_dragging = false;
    emit resetRequested(m_nodeId);
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    updateLook(true);
    QGraphicsRectItem::hoverEnterEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    updateLook(false);
    QGraphicsRectItem::hoverLeaveEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
// PieceSegmentHandle
//---------------------------------------------------------------------------------------------------------------------
PieceSegmentHandle::PieceSegmentHandle(quint32 firstNodeId, quint32 secondNodeId, QGraphicsItem *parent)
    : QObject()
    , QGraphicsPathItem(parent)
    , m_firstNodeId(firstNodeId)
    , m_secondNodeId(secondNodeId)
    , m_dragging(false)
    , m_startScenePos()
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::SizeAllCursor);
    setZValue(900); // under the point handles, over the piece
    setToolTip(tr("Drag to move this line of the piece (the draft does not change)."));
    setBrush(Qt::NoBrush);
    updateLook(false);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::setSegment(const QPainterPath &path)
{
    prepareGeometryChange();
    setPath(path);
}

//---------------------------------------------------------------------------------------------------------------------
QPainterPath PieceSegmentHandle::shape() const
{
    // Easy to grab: a band about 10 pixels wide on screen
    qreal scale = 1;
    if (scene() != nullptr && not scene()->views().isEmpty())
    {
        scale = scene()->views().first()->transform().m11();
    }
    QPainterPathStroker stroker;
    stroker.setWidth(10.0 / qMax(scale, 0.0001));
    return stroker.createStroke(path());
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::updateLook(bool hover)
{
    QPen pen(hover || m_dragging ? QColor(30, 90, 200, 160) : QColor(0, 0, 0, 0));
    pen.setCosmetic(true);
    pen.setWidthF(4);
    setPen(pen);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->type() != QEvent::GraphicsSceneMouseDoubleClick)
    {
        m_dragging = true;
        m_startScenePos = event->scenePos();
        updateLook(true);
        event->accept();
        emit dragStarted(m_firstNodeId, m_secondNodeId);
    }
    QGraphicsPathItem::mousePressEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (not m_dragging)
    {
        QGraphicsPathItem::mouseMoveEvent(event);
        return;
    }
    const QPointF delta = parentItem()->mapFromScene(event->scenePos())
                          - parentItem()->mapFromScene(m_startScenePos);
    setPos(delta);
    emit moved(m_firstNodeId, m_secondNodeId, delta);
    event->accept();
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsPathItem::mouseReleaseEvent(event);
    const bool wasDragging = m_dragging;
    m_dragging = false;
    setPos(QPointF());
    updateLook(isUnderMouse());
    if (wasDragging)
    {
        emit dragFinished();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    updateLook(true);
    QGraphicsPathItem::hoverEnterEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void PieceSegmentHandle::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    updateLook(false);
    QGraphicsPathItem::hoverLeaveEvent(event);
}
