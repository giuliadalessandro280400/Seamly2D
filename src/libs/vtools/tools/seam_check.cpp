/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   seam_check.cpp
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

#include "seam_check.h"
#include "pattern_piece_tool.h"

#include "../vmisc/vabstractapplication.h"
#include "../vmisc/def.h"
#include "../vwidgets/vmaingraphicsscene.h"
#include "../vwidgets/vmaingraphicsview.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../vpatterndb/vcontainer.h"

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QLineF>
#include <QPushButton>
#include <QTimer>
#include <QUndoStack>
#include <QtMath>

SeamCheck *SeamCheck::s_active = nullptr;

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// closed polyline helpers
QVector<qreal> cumulativeLengths(const QVector<QPointF> &points)
{
    QVector<qreal> lengths;
    if (points.isEmpty())
    {
        return lengths;
    }
    lengths.append(0);
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &a = points.at(i);
        const QPointF &b = points.at((i + 1) % points.size());
        lengths.append(lengths.last() + QLineF(a, b).length());
    }
    return lengths; // size = points + 1, last = perimeter
}

qreal wrap(qreal t, qreal perimeter)
{
    if (perimeter <= 0)
    {
        return 0;
    }
    t = std::fmod(t, perimeter);
    if (t < 0)
    {
        t += perimeter;
    }
    return t;
}

QPointF pointAt(const QVector<QPointF> &points, const QVector<qreal> &lengths, qreal t)
{
    if (points.isEmpty())
    {
        return QPointF();
    }
    const qreal perimeter = lengths.last();
    t = wrap(t, perimeter);
    for (int i = 0; i < points.size(); ++i)
    {
        if (t <= lengths.at(i + 1))
        {
            const qreal segment = lengths.at(i + 1) - lengths.at(i);
            const qreal k = segment > 0 ? (t - lengths.at(i)) / segment : 0;
            const QPointF &a = points.at(i);
            const QPointF &b = points.at((i + 1) % points.size());
            return a + (b - a) * k;
        }
    }
    return points.first();
}

qreal project(const QVector<QPointF> &points, const QVector<qreal> &lengths, const QPointF &p)
{
    qreal best = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &a = points.at(i);
        const QPointF &b = points.at((i + 1) % points.size());
        const QPointF ab = b - a;
        const qreal len2 = QPointF::dotProduct(ab, ab);
        qreal k = len2 > 0 ? QPointF::dotProduct(p - a, ab) / len2 : 0;
        k = qBound(0.0, k, 1.0);
        const QPointF q = a + ab * k;
        const qreal d = QLineF(p, q).length();
        if (d < bestDistance)
        {
            bestDistance = d;
            best = lengths.at(i) + k * (lengths.at(i + 1) - lengths.at(i));
        }
    }
    return best;
}

qreal signedArea(const QVector<QPointF> &points)
{
    qreal area = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &a = points.at(i);
        const QPointF &b = points.at((i + 1) % points.size());
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area / 2;
}

QPointF mirrored(const QPointF &p)
{
    return QPointF(p.x(), -p.y());
}

QString mm(qreal pixels)
{
    return QString::number(FromPixel(pixels, Unit::Mm), 'f', 1);
}

const QVector<QColor> &palette()
{
    static const QVector<QColor> colors {QColor(140, 80, 200, 110), QColor(0, 150, 150, 110),
                                         QColor(230, 130, 0, 110),  QColor(60, 170, 60, 110),
                                         QColor(210, 50, 50, 110)};
    return colors;
}
}

//---------------------------------------------------------------------------------------------------------------------
SeamCheck::SeamCheck(PatternPieceTool *checked, VMainGraphicsScene *scene, VAbstractPattern *doc)
    : QObject(nullptr)
    , m_checked(checked)
    , m_base()
    , m_scene(scene)
    , m_doc(doc)
    , m_stage(Stage::PickBase)
    , m_color()
    , m_checkedMark(-1)
    , m_baseMark(-1)
    , m_direction(1)
    , m_mirror(false)
    , m_walked(0)
    , m_baseContour()
    , m_baseLengths()
    , m_checkedContour()
    , m_checkedLengths()
    , m_checkedMarks()
    , m_baseMarks()
    , m_baseStart(0)
    , m_checkedStart(0)
    , m_panel(nullptr)
    , m_status(nullptr)
    , m_readout(nullptr)
    , m_walkButton(nullptr)
{
    static int counter = 0;
    m_color = palette().at(counter++ % palette().size());

    if (s_active != nullptr && s_active->m_stage == Stage::Walking)
    {
        s_active->toggleWalk(); // only one piece follows the mouse
    }
    s_active = this;

    // the piece must not be moved for real while it is laid on another one
    m_checked->EnableToolMove(false);

    connect(checked, &QObject::destroyed, this, &SeamCheck::deleteLater);
    connect(m_scene, &VMainGraphicsScene::mouseMove, this, &SeamCheck::mouseMoved);
    connect(m_scene, &VMainGraphicsScene::MouseLeftPressed, this, &SeamCheck::leftPressed);
    connect(qApp->getUndoStack(), &QUndoStack::indexChanged, this, [this]()
    {
        QTimer::singleShot(0, this, &SeamCheck::patternChanged); // after the pattern is rebuilt
    });

    if (VMainGraphicsView *view = qApp->getSceneView())
    {
        view->installEventFilter(this);
        view->viewport()->installEventFilter(this);
    }

    createPanel();
    setStatus(tr("Click the base piece, the one to check against."));
}

//---------------------------------------------------------------------------------------------------------------------
SeamCheck::~SeamCheck()
{
    if (s_active == this)
    {
        s_active = nullptr;
    }
    if (VMainGraphicsView *view = qApp->getSceneView())
    {
        view->removeEventFilter(this);
        view->viewport()->removeEventFilter(this);
    }
    delete m_panel;
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamCheck::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        // the scene only knows the position of the last move: take the one of the click
        if (VMainGraphicsView *view = qApp->getSceneView())
        {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (watched == view->viewport())
            {
                m_pressPos = view->mapToScene(mouseEvent->position().toPoint());
                m_hasPressPos = true;
            }
        }
    }
    if (s_active == this && event->type() == QEvent::ShortcutOverride)
    {
        // keep our keys away from the shortcuts of the main window
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Space || key == Qt::Key_F || key == Qt::Key_Return || key == Qt::Key_Enter
            || key == Qt::Key_Escape)
        {
            event->accept();
            return true;
        }
    }
    if (s_active == this && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
            case Qt::Key_Space:
                switchSide();
                return true;
            case Qt::Key_F:
                toggleMirror();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                toggleWalk();
                return true;
            case Qt::Key_Escape:
                separate();
                return true;
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::createPanel()
{
    m_panel = new QWidget(qApp->getMainWindow(), Qt::Tool);
    m_panel->setWindowTitle(tr("Seam Check"));
    m_panel->setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(m_panel);

    auto *swatch = new QLabel(m_panel);
    swatch->setFixedHeight(8);
    QColor solid = m_color;
    solid.setAlpha(255);
    swatch->setStyleSheet(QStringLiteral("background-color: %1;").arg(solid.name()));
    layout->addWidget(swatch);

    m_status = new QLabel(m_panel);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_readout = new QLabel(m_panel);
    m_readout->setTextFormat(Qt::RichText);
    m_readout->setMinimumWidth(320);
    layout->addWidget(m_readout);

    auto *buttons = new QHBoxLayout();
    auto *sideButton = new QPushButton(tr("Other Side (Space)"), m_panel);
    auto *mirrorButton = new QPushButton(tr("Flip (F)"), m_panel);
    m_walkButton = new QPushButton(tr("Stop (Enter)"), m_panel);
    auto *separateButton = new QPushButton(tr("Separate (Esc)"), m_panel);
    buttons->addWidget(sideButton);
    buttons->addWidget(mirrorButton);
    buttons->addWidget(m_walkButton);
    buttons->addWidget(separateButton);
    layout->addLayout(buttons);

    connect(sideButton, &QPushButton::clicked, this, &SeamCheck::switchSide);
    connect(mirrorButton, &QPushButton::clicked, this, &SeamCheck::toggleMirror);
    connect(m_walkButton, &QPushButton::clicked, this, &SeamCheck::toggleWalk);
    connect(separateButton, &QPushButton::clicked, this, &SeamCheck::separate);

    m_panel->show();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::setStatus(const QString &text)
{
    if (m_status != nullptr)
    {
        m_status->setText(text);
    }
}

//---------------------------------------------------------------------------------------------------------------------
PatternPieceTool *SeamCheck::pieceAt(const QPointF &scenePos, PatternPieceTool *exclude) const
{
    const QList<QGraphicsItem *> items = m_scene->items(scenePos);
    for (QGraphicsItem *item : items)
    {
        for (QGraphicsItem *current = item; current != nullptr; current = current->parentItem())
        {
            if (current->type() == PatternPieceTool::Type)
            {
                auto *piece = static_cast<PatternPieceTool *>(current);
                if (piece != exclude)
                {
                    return piece;
                }
                break;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
int SeamCheck::markAt(PatternPieceTool *piece, const QPointF &scenePos) const
{
    qreal scale = 1;
    if (VMainGraphicsView *view = qApp->getSceneView())
    {
        scale = qMax(view->transform().m11(), 0.0001);
    }
    const qreal tolerance = 15.0 / scale;

    const QVector<PatternPieceTool::ContourMark> marks = piece->editorMarks();
    int best = -1;
    qreal bestDistance = tolerance;
    for (int i = 0; i < marks.size(); ++i)
    {
        const qreal d = QLineF(piece->mapToScene(marks.at(i).position), scenePos).length();
        if (d <= bestDistance)
        {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief updateContours reads both contours again (they change when points are corrected).
 *
 * The base contour is in scene coordinates, the checked one in its own coordinates (mirrored when flipped), so that
 * placing it does not depend on where it is shown now.
 */
void SeamCheck::updateContours()
{
    if (m_base.isNull() || m_checked.isNull())
    {
        return;
    }

    m_baseContour.clear();
    for (const QPointF &p : m_base->editorContour())
    {
        m_baseContour.append(m_base->mapToScene(p));
    }
    m_baseLengths = cumulativeLengths(m_baseContour);

    m_checkedContour = m_checked->editorContour();
    if (m_mirror)
    {
        for (QPointF &p : m_checkedContour)
        {
            p = mirrored(p);
        }
    }
    m_checkedLengths = cumulativeLengths(m_checkedContour);

    m_baseMarks.clear();
    for (const PatternPieceTool::ContourMark &mark : m_base->editorMarks())
    {
        WalkMark walkMark;
        walkMark.name = mark.name;
        walkMark.notch = mark.notch;
        walkMark.position = project(m_baseContour, m_baseLengths, m_base->mapToScene(mark.position));
        m_baseMarks.append(walkMark);
    }

    m_checkedMarks.clear();
    for (const PatternPieceTool::ContourMark &mark : m_checked->editorMarks())
    {
        WalkMark walkMark;
        walkMark.name = mark.name;
        walkMark.notch = mark.notch;
        walkMark.position = project(m_checkedContour, m_checkedLengths,
                                    m_mirror ? mirrored(mark.position) : mark.position);
        m_checkedMarks.append(walkMark);
    }

    if (m_baseMark >= 0 && m_baseMark < m_baseMarks.size())
    {
        m_baseStart = m_baseMarks.at(m_baseMark).position;
    }
    if (m_checkedMark >= 0 && m_checkedMark < m_checkedMarks.size())
    {
        m_checkedStart = m_checkedMarks.at(m_checkedMark).position;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief checkedDirection the checked piece walks its contour so that both pieces lie on the same side of the seam,
 * one on top of the other, whichever way the contours were drawn.
 */
int SeamCheck::checkedDirection() const
{
    const qreal baseArea = signedArea(m_baseContour);
    const qreal checkedArea = signedArea(m_checkedContour);
    const int same = ((baseArea >= 0) == (checkedArea >= 0)) ? 1 : -1;
    return m_direction * same;
}

//---------------------------------------------------------------------------------------------------------------------
QTransform SeamCheck::placement(qreal walked) const
{
    const qreal step = ToPixel(5, Unit::Mm);
    const int checkedDir = checkedDirection();

    const qreal tb = m_baseStart + m_direction * walked;
    const qreal to = m_checkedStart + checkedDir * walked;

    const QPointF b = pointAt(m_baseContour, m_baseLengths, tb);
    const QPointF bNext = pointAt(m_baseContour, m_baseLengths, tb + m_direction * step);
    const QPointF o = pointAt(m_checkedContour, m_checkedLengths, to);
    const QPointF oNext = pointAt(m_checkedContour, m_checkedLengths, to + checkedDir * step);

    const qreal angle = qAtan2(bNext.y() - b.y(), bNext.x() - b.x()) - qAtan2(oNext.y() - o.y(), oNext.x() - o.x());

    QTransform t;
    t.translate(b.x(), b.y());
    t.rotateRadians(angle);
    t.translate(-o.x(), -o.y());
    if (m_mirror)
    {
        t.scale(1, -1);
    }
    return t;
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::place()
{
    if (m_base.isNull() || m_checked.isNull() || m_baseContour.size() < 2 || m_checkedContour.size() < 2)
    {
        return;
    }
    m_checked->setCheckPlacement(placement(m_walked), m_color);
    updateReadout();
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief updateReadout shows the walked length and, for every point met on the way, how far it is on both pieces.
 */
void SeamCheck::updateReadout()
{
    if (m_readout == nullptr || m_baseLengths.isEmpty() || m_checkedLengths.isEmpty())
    {
        return;
    }

    const qreal basePerimeter = m_baseLengths.last();
    const qreal checkedPerimeter = m_checkedLengths.last();
    const int checkedDir = checkedDirection();

    struct Met { qreal distance; QString name; bool notch; };
    auto collect = [](const QVector<WalkMark> &marks, qreal start, int direction, qreal perimeter)
    {
        QVector<Met> met;
        for (const WalkMark &mark : marks)
        {
            qreal d = wrap((mark.position - start) * direction, perimeter);
            if (perimeter - d < 0.01)
            {
                d = 0; // the start point itself
            }
            met.append({d, mark.name, mark.notch});
        }
        std::sort(met.begin(), met.end(), [](const Met &a, const Met &b) { return a.distance < b.distance; });
        return met;
    };

    const QVector<Met> base = collect(m_baseMarks, m_baseStart, m_direction, basePerimeter);
    const QVector<Met> checked = collect(m_checkedMarks, m_checkedStart, checkedDir, checkedPerimeter);

    auto label = [](const Met &m)
    {
        return (m.notch ? QStringLiteral("◆ ") : QString()) + m.name.toHtmlEscaped();
    };

    QString html = QStringLiteral("<p><b>%1</b> %2 mm</p>").arg(tr("Walked:"), mm(m_walked));
    html += QStringLiteral("<table cellspacing='0' cellpadding='3'><tr><th align='left'>%1</th><th>mm</th>"
                           "<th align='left'>%2</th><th>mm</th><th>%3</th></tr>")
                .arg(tr("Base"), tr("Checked"), tr("Difference"));

    QString lastDifference;
    const int rows = qMin(qMax(base.size(), checked.size()), 12);
    for (int i = 0; i < rows; ++i)
    {
        const bool hasBase = i < base.size();
        const bool hasChecked = i < checked.size();
        // the mouse walks the base piece: a row is reached when its base point is
        const bool passed = hasBase && base.at(i).distance <= m_walked + ToPixel(0.5, Unit::Mm);

        QString diff;
        if (hasBase && hasChecked)
        {
            const qreal d = checked.at(i).distance - base.at(i).distance;
            diff = (d > 0 ? QStringLiteral("+") : QString()) + mm(d);
            if (passed)
            {
                lastDifference = diff;
            }
        }

        const QString style = passed ? QStringLiteral(" style='font-weight:bold'") : QString();
        html += QStringLiteral("<tr%1><td>%2</td><td align='right'>%3</td><td>%4</td><td align='right'>%5</td>"
                               "<td align='right'>%6</td></tr>")
                    .arg(style,
                         hasBase ? label(base.at(i)) : QString(),
                         hasBase ? mm(base.at(i).distance) : QString(),
                         hasChecked ? label(checked.at(i)) : QString(),
                         hasChecked ? mm(checked.at(i).distance) : QString(),
                         diff);
    }
    html += QStringLiteral("</table>");
    if (not lastDifference.isEmpty())
    {
        html += QStringLiteral("<p><b>%1</b> %2 mm</p>").arg(tr("Difference at the last point reached:"),
                                                             lastDifference);
    }
    m_readout->setText(html);
    m_lastDifference = lastDifference;
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::mouseMoved(const QPointF &scenePos)
{
    if (m_stage != Stage::Walking || m_baseLengths.isEmpty())
    {
        return;
    }
    const qreal t = project(m_baseContour, m_baseLengths, scenePos);
    const qreal perimeter = m_baseLengths.last();
    qreal walked = wrap((t - m_baseStart) * m_direction, perimeter);
    if (m_walked < perimeter / 4 && walked > perimeter * 3 / 4)
    {
        walked = 0; // the mouse went back behind the start point: stay there instead of jumping around the piece
    }
    m_walked = walked;
    place();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::leftPressed()
{
    if (s_active != this || m_checked.isNull())
    {
        return;
    }
    const QPointF pos = m_hasPressPos ? m_pressPos : m_scene->getScenePos();
    m_hasPressPos = false;

    switch (m_stage)
    {
        case Stage::PickBase:
        {
            PatternPieceTool *base = pieceAt(pos, m_checked);
            if (base == nullptr)
            {
                return;
            }
            m_base = base;
            connect(base, &QObject::destroyed, this, &SeamCheck::separate);
            m_stage = Stage::PickCheckedPoint;
            setStatus(tr("Click the start point on the piece to check (colored)."));
            break;
        }
        case Stage::PickCheckedPoint:
        {
            const int mark = markAt(m_checked, pos);
            if (mark < 0)
            {
                return;
            }
            m_checkedMark = mark;
            m_stage = Stage::PickBasePoint;
            setStatus(tr("Click the point of the base piece where it starts."));
            break;
        }
        case Stage::PickBasePoint:
        {
            if (m_base.isNull())
            {
                return;
            }
            const int mark = markAt(m_base, pos);
            if (mark < 0)
            {
                return;
            }
            m_baseMark = mark;
            updateContours();
            m_walked = 0;
            startWalking();
            mouseMoved(pos);
            place();
            break;
        }
        case Stage::Walking:
            toggleWalk(); // a click stops it, so the points can be corrected
            break;
        case Stage::Stopped:
        default:
            break; // clicks belong to the piece editor now
    }
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::startWalking()
{
    m_stage = Stage::Walking;
    s_active = this;
    if (m_walkButton != nullptr)
    {
        m_walkButton->setText(tr("Stop (Enter)"));
    }
    setStatus(tr("Move the mouse along the base piece: the colored piece walks on it. Space: other side. "
                 "F: flip. Click or Enter: stop and correct the points."));
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::switchSide()
{
    if (m_stage != Stage::Walking && m_stage != Stage::Stopped)
    {
        return;
    }
    m_direction = -m_direction;
    m_walked = 0;
    if (m_stage == Stage::Walking)
    {
        mouseMoved(m_scene->getScenePos());
    }
    place();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::toggleMirror()
{
    if (m_stage != Stage::Walking && m_stage != Stage::Stopped)
    {
        return;
    }
    m_mirror = not m_mirror;
    updateContours();
    place();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::toggleWalk()
{
    if (m_stage == Stage::Walking)
    {
        m_stage = Stage::Stopped;
        if (m_walkButton != nullptr)
        {
            m_walkButton->setText(tr("Walk (Enter)"));
        }
        setStatus(tr("Stopped. Drag the points of the colored piece to correct it, the lengths update. "
                     "Enter: walk again. Separate: put it back."));
        if (not m_checked.isNull())
        {
            m_scene->clearSelection();
            m_checked->setSelected(true); // shows the point handles of the real piece
        }
    }
    else if (m_stage == Stage::Stopped)
    {
        startWalking();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::patternChanged()
{
    if (m_stage != Stage::Walking && m_stage != Stage::Stopped)
    {
        return;
    }
    updateContours();
    place();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamCheck::separate()
{
    if (not m_checked.isNull())
    {
        m_checked->clearCheckPlacement();
        const VPiece piece = m_checked->getData()->GetPiece(m_checked->getId());
        m_checked->EnableToolMove(not piece.isLocked());
    }
    m_stage = Stage::PickBase;
    if (s_active == this)
    {
        s_active = nullptr;
    }
    deleteLater();
}
