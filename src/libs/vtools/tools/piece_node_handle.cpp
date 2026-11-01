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
void PieceNodeHandle::updateLook(bool hover)
{
    QPen pen(m_modified ? QColor(200, 40, 40) : QColor(30, 90, 200));
    pen.setCosmetic(true);
    pen.setWidthF(2);
    setPen(pen);
    setBrush(hover || m_dragging ? pen.color() : QColor(Qt::white));
}

//---------------------------------------------------------------------------------------------------------------------
QVariant PieceNodeHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged && m_dragging)
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
    if (event->button() == Qt::LeftButton && event->type() != QEvent::GraphicsSceneMouseDoubleClick)
    {
        m_dragging = true;
        m_grabOffset = pos() - parentItem()->mapFromScene(event->scenePos());
        updateLook(true);
        event->accept();
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
    m_dragging = false;
    updateLook(isUnderMouse());
}

//---------------------------------------------------------------------------------------------------------------------
void PieceNodeHandle::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
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
