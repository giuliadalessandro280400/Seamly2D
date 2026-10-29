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
