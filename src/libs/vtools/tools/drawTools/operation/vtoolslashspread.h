/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   vtoolslashspread.h
 **  @brief  Slash and spread: opens a part of the pattern around a hinge point
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

#ifndef VTOOLSLASHSPREAD_H
#define VTOOLSLASHSPREAD_H

#include <qcompilerdetection.h>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "vabstractoperation.h"
#include "slashspread.h"
#include "../vgeometry/vgeometrydef.h"
#include "../vmisc/def.h"
#include "../ifc/xml/vabstractpattern.h"

template <class T> class QSharedPointer;
class VFormula;

/**
 * @brief The VToolSlashSpread class is the draft operation "slash and spread": the selected objects are cut by slashes
 * from the edge to the hinge, evenly spaced, and opened by the spread (a length or a percentage of the edge). The
 * result is a copy: the original objects do not change. The spread can be a formula, so it grades.
 */
class VToolSlashSpread : public VAbstractOperation
{
    Q_OBJECT
public:
    virtual                  ~VToolSlashSpread() Q_DECL_EQ_DEFAULT;
    virtual void              setDialog() override;
    static VToolSlashSpread  *Create(QSharedPointer<DialogTool> dialog, VMainGraphicsScene *scene,
                                     VAbstractPattern *doc, VContainer *data);

    static VToolSlashSpread  *Create(const quint32 _id, quint32 hinge, quint32 edgeStart, quint32 edgeEnd,
                                     int slashes, QString &spread, bool percent, SlashSpread::Fixed fixed,
                                     const QString &suffix, const QVector<SourceItem> &source,
                                     const QVector<DestinationItem> &destination, VMainGraphicsScene *scene,
                                     VAbstractPattern *doc, VContainer *data, const Document &parse,
                                     const Source &typeCreation);

    static const QString      ToolType;
    static const QString      AttrHinge;
    static const QString      AttrEdgeStart;
    static const QString      AttrEdgeEnd;
    static const QString      AttrSlashes;
    static const QString      AttrSpread;
    static const QString      AttrSpreadType;
    static const QString      AttrFixed;

    /** @brief makeSpread the geometry of the operation from the current data (also used by the preview). */
    static SlashSpread        makeSpread(const VContainer *data, quint32 hinge, quint32 edgeStart, quint32 edgeEnd,
                                         int slashes, qreal spread, bool percent, SlashSpread::Fixed fixed,
                                         const QVector<quint32> &objects);

    virtual int               type() const override {return Type;}
    enum                      { Type = UserType + static_cast<int>(Tool::SlashSpread)};

    quint32                   getHingeId() const { return m_hinge; }
    quint32                   getEdgeStartId() const { return m_edgeStart; }
    quint32                   getEdgeEndId() const { return m_edgeEnd; }
    void                      setHingeId(quint32 value);
    void                      setEdgeStartId(quint32 value);
    void                      setEdgeEndId(quint32 value);
    int                       getSlashes() const { return m_slashes; }
    void                      setSlashes(int value);
    bool                      isPercent() const { return m_percent; }
    void                      setPercent(bool value);
    SlashSpread::Fixed        getFixed() const { return m_fixed; }
    void                      setFixed(SlashSpread::Fixed value);

    QString                   pointName(quint32 id) const;

    VFormula                  getFormulaSpread() const;
    void                      setFormulaSpread(const VFormula &value);

    virtual void              ShowVisualization(bool show) override;

protected slots:
    virtual void              showContextMenu(QGraphicsSceneContextMenuEvent *event, quint32 id=NULL_ID) override;

protected:
    virtual void              SetVisualization() override;
    virtual void              SaveDialog(QDomElement &domElement) override;
    virtual void              ReadToolAttributes(const QDomElement &domElement) override;
    virtual void              SaveOptions(QDomElement &tag, QSharedPointer<VGObject> &obj) override;
    virtual QString           makeToolTip() const override;

private:
    Q_DISABLE_COPY(VToolSlashSpread)
    quint32                   m_hinge;
    quint32                   m_edgeStart;
    quint32                   m_edgeEnd;
    int                       m_slashes;
    QString                   m_formulaSpread;
    bool                      m_percent;
    SlashSpread::Fixed        m_fixed;

                              VToolSlashSpread(VAbstractPattern *doc, VContainer *data, quint32 id, quint32 hinge,
                                               quint32 edgeStart, quint32 edgeEnd, int slashes,
                                               const QString &spread, bool percent, SlashSpread::Fixed fixed,
                                               const QString &suffix, const QVector<SourceItem> &source,
                                               const QVector<DestinationItem> &destination,
                                               const Source &typeCreation, QGraphicsItem *parent = nullptr);

    void                      saveChange();
    static VPointF            openedPoint(const SlashSpread &spread, quint32 idItem, const QString &suffix,
                                          VContainer *data);
    static void               registerCurve(VContainer *data, const QSharedPointer<VAbstractCurve> &curve, quint32 id);
};

#endif // VTOOLSLASHSPREAD_H
