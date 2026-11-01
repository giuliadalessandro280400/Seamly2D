/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   seam_check.h
 **  @brief  Seam check: lay a piece on another one and walk it along the seam
 **          to compare the lengths. Separate puts the piece back.
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

#ifndef SEAM_CHECK_H
#define SEAM_CHECK_H

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QString>
#include <QTransform>
#include <QVector>

class PatternPieceTool;
class VMainGraphicsScene;
class VAbstractPattern;
class QLabel;
class QPushButton;
class QWidget;

/**
 * @brief The SeamCheck class walks a piece along another one, like sliding one paper pattern on the other.
 *
 * 1. the user chooses the base piece,
 * 2. a point of the checked piece and the point of the base piece it starts from,
 * 3. moving the mouse along the base piece the checked piece rolls along it, so that both contours touch at the same
 *    walked length. Space walks the other way. A click stops it so the points can be corrected (on the real piece),
 * 4. Separate puts the piece back in its place.
 *
 * Only the view of the checked piece changes while checking: its position in the file is not touched.
 */
class SeamCheck : public QObject
{
    Q_OBJECT
public:
    SeamCheck(PatternPieceTool *checked, VMainGraphicsScene *scene, VAbstractPattern *doc);
    virtual ~SeamCheck();

protected:
    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void mouseMoved(const QPointF &scenePos);
    void leftPressed();
    void switchSide();
    void toggleMirror();
    void toggleWalk();
    void separate();
    void patternChanged();

private:
    Q_DISABLE_COPY(SeamCheck)

    enum class Stage { PickBase, PickCheckedPoint, PickBasePoint, Walking, Stopped };

    QPointer<PatternPieceTool> m_checked;
    QPointer<PatternPieceTool> m_base;
    VMainGraphicsScene        *m_scene;
    VAbstractPattern          *m_doc;
    Stage                      m_stage;
    QColor                     m_color;

    int                        m_checkedMark;   // index of the start point among the marks of the checked piece
    int                        m_baseMark;      // index of the start point among the marks of the base piece
    int                        m_direction;     // +1 / -1 along the base contour
    bool                       m_mirror;        // the checked piece is flipped (face to face)
    qreal                      m_walked;        // walked length (pixels)

    // contours: base in scene coordinates, checked in its own coordinates; cumulative lengths
    QVector<QPointF>           m_baseContour;
    QVector<qreal>             m_baseLengths;
    QVector<QPointF>           m_checkedContour;
    QVector<qreal>             m_checkedLengths;
    struct WalkMark { qreal position; QString name; bool notch; };
    QVector<WalkMark>          m_checkedMarks;
    QVector<WalkMark>          m_baseMarks;
    qreal                      m_baseStart;
    qreal                      m_checkedStart;

    QWidget                   *m_panel;
    QLabel                    *m_status;
    QLabel                    *m_readout;
    QPushButton               *m_walkButton;
    QString                    m_lastDifference;
    QPointF                    m_pressPos;
    bool                       m_hasPressPos {false};

    static SeamCheck          *s_active;

    void                       createPanel();
    void                       setStatus(const QString &text);
    PatternPieceTool          *pieceAt(const QPointF &scenePos, PatternPieceTool *exclude) const;
    int                        markAt(PatternPieceTool *piece, const QPointF &scenePos) const;
    void                       updateContours();
    void                       place();
    void                       startWalking();
    int                        checkedDirection() const;
    QTransform                 placement(qreal walked) const;
    void                       updateReadout();

public:
    // for tests
    qreal                      walked() const { return m_walked; }
    QString                    lastDifference() const { return m_lastDifference; }
    static SeamCheck          *active() { return s_active; }
};

#endif // SEAM_CHECK_H
