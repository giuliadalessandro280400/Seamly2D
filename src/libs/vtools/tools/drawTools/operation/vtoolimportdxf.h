/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolimportdxf.h
 **  @brief  A DXF pattern (AAMA) placed in the draft
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

#ifndef VTOOLIMPORTDXF_H
#define VTOOLIMPORTDXF_H

#include <qcompilerdetection.h>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "vabstractoperation.h"
#include "../vmisc/def.h"
#include "../ifc/xml/vabstractpattern.h"

class VFormula;
struct AamaFile;

/**
 * @brief A DXF point tied to a point of the draft. The position of the draft point when the link was made is kept:
 * when the draft changes (another size), the DXF follows how the tied points moved, and every other point of the DXF
 * moves in proportion (moving least squares: near points follow the near links).
 */
struct DxfLink
{
    int     shape {-1};      // index of the DXF point among the shapes
    quint32 point {NULL_ID}; // point of the draft
    QPointF reference;       // where the draft point was when tied (pattern units)
};

/**
 * @brief One object of an imported DXF: a point, or a curve through points. Coordinates in millimetres, y down,
 * as in the file (only flipped).
 */
struct DxfShape
{
    bool             point {true};
    QString          name;
    QVector<QPointF> points;
};

/**
 * @brief The VToolImportDxf class is a DXF pattern laid on the draft. The geometry of the file is kept in the pattern,
 * so the file is no longer needed. It is placed by making one of its points sit on a point of the draft (and,
 * optionally, turning it so that one of its lines follows a line of the draft); the angle can be a formula. The
 * points and curves it makes are normal draft objects, ready to build pieces on.
 */
class VToolImportDxf : public VAbstractOperation
{
    Q_OBJECT
public:
    virtual                ~VToolImportDxf() Q_DECL_EQ_DEFAULT;
    virtual void            setDialog() override {}

    static VToolImportDxf  *Create(const quint32 _id, quint32 anchor, int dxfAnchor, quint32 direction,
                                   int dxfDirection, QString &angle, const QVector<DxfShape> &shapes,
                                   const QPointF &anchorReference, const QPointF &directionReference,
                                   const QVector<DxfLink> &links,
                                   const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                                   VAbstractPattern *doc, VContainer *data, const Document &parse,
                                   const Source &typeCreation);

    /** @brief shapesFromFile the points and curves of the chosen pieces, with names not yet used in the pattern. */
    static QVector<DxfShape> shapesFromFile(const AamaFile &file, const QVector<int> &pieces, const VContainer *data);

    static QVector<DxfShape> readShapes(const QDomElement &domElement);
    static QVector<DxfLink>  readLinks(const QString &value);
    static QString           formatLinks(const QVector<DxfLink> &links);

    static const QString    ToolType;
    static const QString    AttrAnchor;
    static const QString    AttrDxfAnchor;
    static const QString    AttrDirection;
    static const QString    AttrDxfDirection;
    static const QString    TagShapes;
    static const QString    AttrLinks;
    static const QString    AttrReference;

    virtual int             type() const override {return Type;}
    enum                    { Type = UserType + static_cast<int>(Tool::ImportDxf)};

    quint32                 getAnchorId() const { return m_anchor; }
    void                    setAnchorId(quint32 value);
    quint32                 getDirectionId() const { return m_direction; }
    void                    setDirectionId(quint32 value);
    QString                 pointName(quint32 id) const;

    QVector<DxfLink>        getLinks() const { return m_links; }
    /** @brief tie the DXF point (destination object) to a point of the draft; NULL_ID unties it. */
    void                    linkPoint(quint32 destinationId, quint32 draftPoint);
    int                     shapeOfDestination(quint32 destinationId) const;

    VFormula                getFormulaAngle() const;
    void                    setFormulaAngle(const VFormula &value);

    virtual void            ShowVisualization(bool show) override { Q_UNUSED(show) }

protected slots:
    virtual void            showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id=NULL_ID) override;

protected:
    virtual void            SetVisualization() override {}
    virtual void            SaveDialog(QDomElement &domElement) override { Q_UNUSED(domElement) }
    virtual void            ReadToolAttributes(const QDomElement &domElement) override;
    virtual void            SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj) override;
    virtual QString         makeToolTip() const override;

private:
    Q_DISABLE_COPY(VToolImportDxf)
    quint32                 m_anchor;
    int                     m_dxfAnchor;
    quint32                 m_direction;
    int                     m_dxfDirection;
    QString                 m_formulaAngle;
    QVector<DxfShape>       m_shapes;
    QPointF                 m_anchorReference;     // pattern units
    QPointF                 m_directionReference;  // pattern units
    QVector<DxfLink>        m_links;

                            VToolImportDxf(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 anchor,
                                           int dxfAnchor, quint32 direction, int dxfDirection, const QString &angle,
                                           const QVector<DxfShape> &shapes, const QPointF &anchorReference,
                                           const QPointF &directionReference, const QVector<DxfLink> &links,
                                           const QVector<DestinationItem> &destination, const Source &typeCreation,
                                           QGraphicsItem *parent = nullptr);
    void                    saveChange();
};

#endif // VTOOLIMPORTDXF_H
