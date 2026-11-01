/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   movepiecenode.cpp
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

#include "movepiecenode.h"

#include <QDomElement>

#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/ifcdef.h"
#include "../vmisc/logging.h"
#include "../vmisc/def.h"

namespace
{
QDomElement findNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId);
}

namespace
{
QDomElement findNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId)
{
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
} // namespace

//---------------------------------------------------------------------------------------------------------------------
MovePieceNode::MovePieceNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId, const QPointF &newOffset,
                             QUndoCommand *parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_pieceId(pieceId)
    , m_oldOffset(readOffset(doc, pieceId, nodeId))
    , m_newOffset(newOffset)
{
    setText(QObject::tr("move piece point"));
    this->nodeId = nodeId;
}

//---------------------------------------------------------------------------------------------------------------------
QPointF MovePieceNode::readOffset(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId)
{
    const QDomElement node = findNode(doc, pieceId, nodeId);
    if (node.isNull())
    {
        return QPointF();
    }
    return QPointF(node.attribute(VAbstractPattern::AttrNodeOffsetX, "0").toDouble(),
                   node.attribute(VAbstractPattern::AttrNodeOffsetY, "0").toDouble());
}

//---------------------------------------------------------------------------------------------------------------------
bool MovePieceNode::writeOffset(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId, const QPointF &offset)
{
    QDomElement node = findNode(doc, pieceId, nodeId);
    if (node.isNull())
    {
        qCDebug(vUndo, "Can't find node %u of piece %u.", nodeId, pieceId);
        return false;
    }

    // Values below 0.01 mm are noise
    if (qAbs(offset.x()) < 1e-4 && qAbs(offset.y()) < 1e-4)
    {
        node.removeAttribute(VAbstractPattern::AttrNodeOffsetX);
        node.removeAttribute(VAbstractPattern::AttrNodeOffsetY);
    }
    else
    {
        doc->SetAttribute(node, VAbstractPattern::AttrNodeOffsetX, offset.x());
        doc->SetAttribute(node, VAbstractPattern::AttrNodeOffsetY, offset.y());
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void MovePieceNode::undo()
{
    if (writeOffset(doc, m_pieceId, nodeId, m_oldOffset))
    {
        emit NeedLiteParsing(Document::LiteParse);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MovePieceNode::redo()
{
    if (writeOffset(doc, m_pieceId, nodeId, m_newOffset))
    {
        emit NeedLiteParsing(Document::LiteParse);
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool MovePieceNode::mergeWith(const QUndoCommand *command)
{
    const MovePieceNode *other = static_cast<const MovePieceNode *>(command);
    SCASSERT(other != nullptr)
    if (other->m_pieceId != m_pieceId || other->nodeId != nodeId)
    {
        return false;
    }
    m_newOffset = other->m_newOffset;
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
int MovePieceNode::id() const
{
    return static_cast<int>(UndoCommand::MovePieceNode);
}

//---------------------------------------------------------------------------------------------------------------------
MovePieceNodes::MovePieceNodes(VAbstractPattern *doc, quint32 pieceId, const QMap<quint32, QPointF> &newOffsets,
                               int dragKey, QUndoCommand *parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_pieceId(pieceId)
    , m_dragKey(dragKey)
    , m_oldOffsets()
    , m_newOffsets(newOffsets)
{
    setText(QObject::tr("move piece points"));
    nodeId = pieceId;
    for (auto i = newOffsets.cbegin(); i != newOffsets.cend(); ++i)
    {
        m_oldOffsets.insert(i.key(), MovePieceNode::readOffset(doc, pieceId, i.key()));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MovePieceNodes::apply(const QMap<quint32, QPointF> &offsets)
{
    for (auto i = offsets.cbegin(); i != offsets.cend(); ++i)
    {
        MovePieceNode::writeOffset(doc, m_pieceId, i.key(), i.value());
    }
    emit NeedLiteParsing(Document::LiteParse);
}

//---------------------------------------------------------------------------------------------------------------------
void MovePieceNodes::undo()
{
    apply(m_oldOffsets);
}

//---------------------------------------------------------------------------------------------------------------------
void MovePieceNodes::redo()
{
    apply(m_newOffsets);
}

//---------------------------------------------------------------------------------------------------------------------
bool MovePieceNodes::mergeWith(const QUndoCommand *command)
{
    const MovePieceNodes *other = static_cast<const MovePieceNodes *>(command);
    SCASSERT(other != nullptr)
    if (other->m_pieceId != m_pieceId || other->m_dragKey != m_dragKey)
    {
        return false;
    }
    for (auto i = other->m_newOffsets.cbegin(); i != other->m_newOffsets.cend(); ++i)
    {
        if (not m_oldOffsets.contains(i.key()))
        {
            m_oldOffsets.insert(i.key(), other->m_oldOffsets.value(i.key()));
        }
        m_newOffsets.insert(i.key(), i.value());
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
int MovePieceNodes::id() const
{
    return static_cast<int>(UndoCommand::MovePieceNodes);
}

//---------------------------------------------------------------------------------------------------------------------
PinPieceNode::PinPieceNode(VAbstractPattern *doc, quint32 pieceId, quint32 nodeId, bool pinned, QUndoCommand *parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_pieceId(pieceId)
    , m_pinned(pinned)
{
    setText(pinned ? QObject::tr("pin piece point") : QObject::tr("unpin piece point"));
    this->nodeId = nodeId;
}

//---------------------------------------------------------------------------------------------------------------------
void PinPieceNode::apply(bool pinned)
{
    QDomElement node = findNode(doc, m_pieceId, nodeId);
    if (node.isNull())
    {
        return;
    }
    if (pinned)
    {
        doc->SetAttribute(node, VAbstractPattern::AttrNodePinned, true);
    }
    else
    {
        node.removeAttribute(VAbstractPattern::AttrNodePinned);
    }
    emit NeedLiteParsing(Document::LiteParse);
}

//---------------------------------------------------------------------------------------------------------------------
void PinPieceNode::undo()
{
    apply(not m_pinned);
}

//---------------------------------------------------------------------------------------------------------------------
void PinPieceNode::redo()
{
    apply(m_pinned);
}
