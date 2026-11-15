/******************************************************************************
 **  @file   prova3d_panel.cpp
 **  @brief  Prova 3D: the panel beside the pattern (view of the form, the fitting and the drop).
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#include "prova3d_panel.h"
#include "prova3d_setup.h"
#include "prova3d_setup_dialog.h"
#include "prova3d_grid.h"

#include "../vpatterndb/vcontainer.h"

#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QFrame>
#include <QLocale>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
// the palette of the panel: paper, ink, a red pencil
const QColor paper(0xF3, 0xEF, 0xE7);
const QColor paperLight(0xFB, 0xF9, 0xF4);
const QColor ink(0x1C, 0x1B, 0x19);
const QColor rule(0xCF, 0xC7, 0xB8);
const QColor redPencil(0xA8, 0x26, 0x1E);
const QColor formColor(0xD6, 0xCF, 0xC2);

QStringList serifFamilies()
{
    return {QStringLiteral("Didot"), QStringLiteral("Bodoni MT"), QStringLiteral("Bodoni 72"),
            QStringLiteral("Georgia"), QStringLiteral("Times New Roman"), QStringLiteral("serif")};
}

QStringList handFamilies()
{
    return {QStringLiteral("Ink Free"), QStringLiteral("Segoe Print"), QStringLiteral("Bradley Hand"),
            QStringLiteral("Noteworthy"), QStringLiteral("Comic Neue"), QStringLiteral("cursive")};
}

QFont smallCaps(int pointSize)
{
    QFont f;
    f.setPointSizeF(pointSize);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
    return f;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(smallCaps(7));
    l->setObjectName(QStringLiteral("p3section"));
    return l;
}

QFrame *ruleLine(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("p3rule"));
    f->setFrameShape(QFrame::HLine);
    f->setFixedHeight(1);
    return f;
}

QDoubleSpinBox *field(double lo, double hi, double value, double step, int decimals, const QString &suffix,
                      QWidget *parent)
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi);
    s->setValue(value);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setSuffix(suffix);
    s->setAlignment(Qt::AlignRight);
    QFont f;
    f.setFamilies(serifFamilies());
    f.setPointSizeF(11);
    s->setFont(f);
    return s;
}

const char *panelStyle = R"(
#prova3dPanel { background: #F3EFE7; }
#prova3dPanel QLabel { color: #1C1B19; background: transparent; }
#prova3dPanel QLabel#p3section { color: #6E685D; }
#prova3dPanel QFrame#p3rule { background: #CFC7B8; border: none; }
#prova3dPanel QPushButton {
    background: transparent; color: #1C1B19; border: 1px solid #1C1B19;
    padding: 7px 12px; border-radius: 0px;
}
#prova3dPanel QPushButton:hover { background: #1C1B19; color: #F3EFE7; }
#prova3dPanel QPushButton:disabled { color: #ABA497; border-color: #CFC7B8; background: transparent; }
#prova3dPanel QPushButton#p3primary { background: #1C1B19; color: #F3EFE7; }
#prova3dPanel QPushButton#p3primary:hover { background: #A8261E; border-color: #A8261E; }
#prova3dPanel QPushButton#p3primary:disabled { background: #CFC7B8; border-color: #CFC7B8; color: #F3EFE7; }
#prova3dPanel QPushButton#p3quiet { border: none; text-decoration: underline; padding: 7px 4px; }
#prova3dPanel QPushButton#p3quiet:hover { background: transparent; color: #A8261E; }
#prova3dPanel QDoubleSpinBox {
    background: transparent; color: #1C1B19; border: none; border-bottom: 1px solid #1C1B19;
    padding: 1px 0px; selection-background-color: #1C1B19; selection-color: #F3EFE7;
}
#prova3dPanel QDoubleSpinBox:focus { border-bottom: 1px solid #A8261E; }
#prova3dPanel QComboBox {
    background: transparent; color: #1C1B19; border: none; border-bottom: 1px solid #1C1B19; padding: 2px 18px 2px 2px;
}
#prova3dPanel QComboBox:hover { border-bottom: 1px solid #A8261E; }
#prova3dPanel QComboBox::drop-down { border: none; width: 14px; }
#prova3dPanel QComboBox QAbstractItemView {
    background: #FBF9F4; color: #1C1B19; border: 1px solid #1C1B19; selection-background-color: #1C1B19;
    selection-color: #F3EFE7; outline: none;
}
#prova3dPanel QTableWidget { background: #FBF9F4; border: 1px solid #CFC7B8; color: #1C1B19; }
#prova3dPanel QTableWidget::item { border-bottom: 1px solid #E6E0D4; padding: 2px 6px; }
#prova3dPanel QListWidget#p3pins { background: transparent; border: none; color: #1C1B19; }
#prova3dPanel QListWidget#p3pins::item { border-bottom: 1px solid #E6E0D4; padding: 2px 0px; }
#prova3dPanel QListWidget#p3pins::item:selected { background: transparent; color: #A8261E; }
#prova3dPanel QPushButton:checked { background: #A8261E; border-color: #A8261E; color: #F3EFE7; }
#prova3dPanel QHeaderView::section {
    background: #F3EFE7; color: #6E685D; border: none; border-bottom: 1px solid #1C1B19; padding: 5px 6px;
    font-size: 8pt;
}
#prova3dPanel QCheckBox::indicator { width: 13px; height: 13px; border: 1px solid #1C1B19; background: #FBF9F4; }
#prova3dPanel QCheckBox::indicator:checked { background: #1C1B19; }
#prova3dPanel QPushButton#p3toggle { border: none; border-bottom: 1px solid transparent; padding: 3px 2px; color: #8A8478; }
#prova3dPanel QPushButton#p3toggle:checked { color: #1C1B19; border-bottom: 1px solid #1C1B19; }
#prova3dPanel QPushButton#p3toggle:hover { background: transparent; color: #A8261E; }
#prova3dPanel QDoubleSpinBox::up-button, #prova3dPanel QDoubleSpinBox::down-button { width: 0px; border: none; }
)";
} // namespace

// ============================================================================ shared look
QString Prova3dPanel::styleSheetText() { return QString::fromLatin1(panelStyle); }

QFont Prova3dPanel::titleFont()
{
    QFont tf;
    tf.setFamilies(serifFamilies());
    tf.setPointSizeF(20);
    tf.setItalic(true);
    return tf;
}

QFont Prova3dPanel::noteFont()
{
    QFont f;
    f.setFamilies(serifFamilies());
    f.setItalic(true);
    f.setPointSizeF(10);
    return f;
}

QFont Prova3dPanel::capsFont() { return smallCaps(7); }

QLabel *Prova3dPanel::sectionLabel(const QString &text, QWidget *parent) { return ::sectionLabel(text, parent); }

// ============================================================================ view
Prova3dView::Prova3dView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 320);
    setCursor(Qt::OpenHandCursor);
    setMouseTracking(true);
}

sim::V3 Prova3dView::toView(const sim::V3 &v) const
{
    const double cy = std::cos(m_yaw), sy = std::sin(m_yaw), cp = std::cos(m_pitch), sp = std::sin(m_pitch);
    const double x = v.x * cy + v.z * sy;
    const double z0 = -v.x * sy + v.z * cy;
    const double y0 = v.y - 116.0;
    return sim::V3(x, y0 * cp - z0 * sp, y0 * sp + z0 * cp);
}

sim::V3 Prova3dView::fromView(const sim::V3 &v) const
{
    const double cy = std::cos(m_yaw), sy = std::sin(m_yaw), cp = std::cos(m_pitch), sp = std::sin(m_pitch);
    const double y0 = v.y * cp + v.z * sp;
    const double z0 = -v.y * sp + v.z * cp;
    return sim::V3(v.x * cy - z0 * sy, y0 + 116.0, v.x * sy + z0 * cy);
}

double Prova3dView::viewScale() const
{
    return (std::min)(width() / 70.0, height() / 92.0) * m_zoom;
}

bool Prova3dView::pick(const QPoint &pos, double &theta, double &y, bool snap) const
{
    if (m_form == nullptr) return false;
    const double scale = viewScale();
    const double vx = (pos.x() - width() / 2.0) / scale, vy = -(pos.y() - height() / 2.0) / scale;
    // a ray from the viewer into the scene, walked with the distance field
    const sim::V3 o = fromView(sim::V3(vx, vy, 80));
    const sim::V3 d = fromView(sim::V3(vx, vy, -80)) - o;
    const sim::V3 dir = d * (1.0 / d.len());
    double t = 0;
    for (int i = 0; i < 600 && t < 160; ++i)
    {
        const sim::V3 q = o + dir * t;
        const double dist = m_form->distance(q, nullptr);
        if (dist < 0.05)
        {
            m_form->surfaceCoords(q, theta, y);
            y = (std::max)(m_form->bottom(), (std::min)(m_form->top(), y));
            if (snap) prova3d::snapToGrid(*m_form, theta, y);
            return true;
        }
        t += dist > 1e8 ? 1.0 : (std::max)(dist * 0.8, 0.05);
    }
    return false;
}

void Prova3dView::setPreview(const std::vector<PreviewPiece> &pieces, const std::vector<PreviewPin> &pins)
{
    m_previewPieces = pieces;
    m_previewPins = pins;
    update();
}

void Prova3dView::setPicking(bool on)
{
    m_picking = on;
    m_hover = false;
    setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
    update();
}

void Prova3dView::leaveEvent(QEvent *)
{
    if (m_hover) { m_hover = false; update(); }
}

void Prova3dView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_picking) { setCursor(Qt::OpenHandCursor); return; }
    if ((event->pos() - m_pressPos).manhattanLength() > 4) return;   // it was a turn of the view
    double theta, y;
    if (pick(event->pos(), theta, y, !(event->modifiers() & Qt::AltModifier))) emit picked(theta, y);
}

void Prova3dView::setForm(const sim::DressForm *form)
{
    m_form = form;
    m_formVerts.clear();
    m_formTris.clear();
    m_grid.clear();
    if (form != nullptr)
    {
        form->mesh(m_formVerts, m_formTris, 56);
        for (const prova3d::GridLine &gl : prova3d::gridLines(*form))
            for (size_t i = 1; i < gl.points.size(); ++i)
            {
                const sim::V3 a = gl.points[i - 1], b = gl.points[i];
                sim::V3 n;
                form->distance((a + b) * 0.5, &n);
                if (n.dot(n) < 0.5) { n = sim::V3(a.x, 0, a.z); n = n * (1.0 / (std::max)(n.len(), 1e-9)); }
                // a hair outside the surface, so that the form doesn't hide it
                m_grid.push_back({a + n * 0.08, b + n * 0.08, n, gl.tape});
            }
    }
    update();
}

void Prova3dView::setGarment(std::shared_ptr<sim::Garment> garment, const std::vector<QString> &seamNames)
{
    m_garment = std::move(garment);
    m_seamNames = seamNames;
    update();
}

void Prova3dView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), paperLight);

    // a thin frame, like a photo pasted on the page
    p.setPen(QPen(rule, 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const double scale = viewScale();
    const QPointF centre(width() / 2.0, height() / 2.0);
    auto view = [&](const sim::V3 &v) { return toView(v); };
    auto screen = [&](const sim::V3 &v) { return QPointF(centre.x() + v.x * scale, centre.y() - v.y * scale); };

    const sim::V3 light = [] { sim::V3 l(-0.35, 0.55, 1.0); return l * (1.0 / l.len()); }();

    struct Item { double depth; int kind; int index; };   // kind 0 form, 1 garment, 2 border, 3 grid, 4 flat piece
    std::vector<Item> items;
    std::vector<sim::V3> fv(m_formVerts.size());
    for (size_t i = 0; i < m_formVerts.size(); ++i) fv[i] = view(m_formVerts[i]);
    for (size_t t = 0; t < m_formTris.size(); ++t)
    {
        const auto &tr = m_formTris[t];
        const sim::V3 n = sim::cross(fv[tr[1]] - fv[tr[0]], fv[tr[2]] - fv[tr[0]]);
        if (n.z <= 0) continue;   // the form is closed: its back faces are hidden
        items.push_back({(fv[tr[0]].z + fv[tr[1]].z + fv[tr[2]].z) / 3 - 0.2, 0, int(t)});
    }

    std::vector<sim::V3> gv;
    std::vector<std::array<int, 3>> gt;
    std::vector<int> gp;
    std::vector<std::pair<int, int>> border;
    std::vector<sim::V3> gn;
    if (m_garment)
    {
        const std::vector<sim::V3> x = m_garment->shownPositions();
        gt = m_garment->meshTriangles();
        gp = m_garment->meshTrianglePiece();
        gv.resize(x.size());
        for (size_t i = 0; i < x.size(); ++i) gv[i] = view(x[i]);
        std::map<std::pair<int, int>, int> edgeCount;
        for (const auto &tr : gt)
            for (int e = 0; e < 3; ++e)
            {
                const int a = tr[e], b = tr[(e + 1) % 3];
                ++edgeCount[{(std::min)(a, b), (std::max)(a, b)}];
            }
        for (const auto &kv : edgeCount)
            if (kv.second == 1) border.push_back(kv.first);
        // smooth shading: normals averaged on the vertices
        gn.assign(gv.size(), sim::V3());
        for (const auto &tr : gt)
        {
            sim::V3 n = sim::cross(gv[tr[1]] - gv[tr[0]], gv[tr[2]] - gv[tr[0]]);
            const double l = n.len();
            if (l < 1e-12) continue;
            n = n * (1.0 / l);
            // the cloth has two sides: keep the normals of a vertex on one side
            for (int k : tr) gn[k] += (gn[k].dot(n) < 0 ? n * -1.0 : n);
        }
        for (sim::V3 &n : gn) { const double l = n.len(); if (l > 1e-12) n = n * (1.0 / l); }
        for (size_t t = 0; t < gt.size(); ++t)
        {
            const auto &tr = gt[t];
            items.push_back({(gv[tr[0]].z + gv[tr[1]].z + gv[tr[2]].z) / 3 + 0.25, 1, int(t)});
        }
        for (size_t e = 0; e < border.size(); ++e)
            items.push_back({(gv[border[e].first].z + gv[border[e].second].z) / 2 + 0.35, 2, int(e)});
    }
    // the grid on the form, only on the side facing the viewer
    std::vector<std::pair<sim::V3, sim::V3>> gridView(m_grid.size());
    for (size_t i = 0; i < m_grid.size(); ++i)
    {
        const sim::V3 a = view(m_grid[i].a), b = view(m_grid[i].b);
        const sim::V3 n = view(m_grid[i].n + sim::V3(0, 116.0, 0));   // a direction: no translation
        gridView[i] = {a, b};
        if (n.z <= 0.05) continue;
        items.push_back({(a.z + b.z) / 2 - 0.1, 3, int(i)});
    }
    // pieces laid flat where they are pinned, before the fitting
    std::vector<std::vector<sim::V3>> flatView;
    if (!m_garment)
    {
        for (size_t i = 0; i < m_previewPieces.size(); ++i)
        {
            std::vector<sim::V3> pv;
            double z = 0;
            for (const sim::V3 &q : m_previewPieces[i].outline) { pv.push_back(view(q)); z += pv.back().z; }
            flatView.push_back(pv);
            if (!pv.empty()) items.push_back({z / pv.size() + 0.3, 4, int(i)});
        }
    }
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.depth < b.depth; });
    const QPen tapePen(QColor(0x1C, 0x1B, 0x19, 150), 0.9);
    const QPen finePen(QColor(0x8A, 0x84, 0x78, 70), 0.6);

    const QColor pieceColors[2] = {QColor(0xFD, 0xFC, 0xF8), QColor(0xF0, 0xEC, 0xE2)};
    QPen inkPen(ink, 1.1);
    inkPen.setCapStyle(Qt::RoundCap);
    for (const Item &it : items)
    {
        if (it.kind == 2)
        {
            p.setPen(inkPen);
            p.drawLine(screen(gv[border[it.index].first]), screen(gv[border[it.index].second]));
            continue;
        }
        if (it.kind == 3)
        {
            p.setPen(m_grid[size_t(it.index)].tape ? tapePen : finePen);
            p.drawLine(screen(gridView[size_t(it.index)].first), screen(gridView[size_t(it.index)].second));
            continue;
        }
        if (it.kind == 4)
        {
            QPolygonF poly;
            for (const sim::V3 &q : flatView[size_t(it.index)]) poly << screen(q);
            p.setPen(inkPen);
            p.setBrush(QColor(0xFD, 0xFC, 0xF8, 225));
            p.drawPolygon(poly);
            continue;
        }
        const bool isForm = it.kind == 0;
        const auto &tr = isForm ? m_formTris[it.index] : gt[it.index];
        const std::vector<sim::V3> &vv = isForm ? fv : gv;
        sim::V3 n = sim::cross(vv[tr[1]] - vv[tr[0]], vv[tr[2]] - vv[tr[0]]);
        const double nl = n.len();
        if (nl < 1e-12) continue;
        n = n * (1.0 / nl);
        if (!isForm)
        {
            sim::V3 s = gn[tr[0]] + gn[tr[1]] + gn[tr[2]];
            if (s.len() > 1e-9) n = s * (1.0 / s.len());
        }
        const double shade = 0.62 + 0.38 * std::fabs(n.dot(light));
        QColor base = isForm ? formColor : pieceColors[(std::min)(1, (std::max)(0, gp[it.index]))];
        QColor c = QColor::fromRgbF(base.redF() * shade, base.greenF() * shade, base.blueF() * shade);
        p.setPen(QPen(c, 0.6));
        p.setBrush(c);
        const QPointF poly[3] = {screen(vv[tr[0]]), screen(vv[tr[1]]), screen(vv[tr[2]])};
        p.drawPolygon(poly, 3);
    }

    // the seams that don't close: a red pencil line across the gap and how much is missing
    if (m_garment && m_showSeams)
    {
        QFont hand;
        hand.setFamilies(handFamilies());
        hand.setPointSizeF(11);
        p.setFont(hand);
        QPen red(redPencil, 1.2, Qt::DashLine);
        QList<QRectF> taken;
        const QFontMetricsF fm(hand);
        for (const sim::Garment::OpenSeam &os : m_garment->openSeams())
        {
            if (os.pairs.empty() || os.maxGap < 0.3) continue;
            p.setPen(red);
            QPointF labelAt;
            double best = -1;
            for (const auto &pr : os.pairs)
            {
                const QPointF a = screen(view(pr.first)), b = screen(view(pr.second));
                p.drawLine(a, b);
                const double g = (pr.second - pr.first).len();
                if (g > best) { best = g; labelAt = (a + b) / 2; }
            }
            p.setPen(redPencil);
            const QString text = QStringLiteral("%1 cm").arg(QLocale().toString(os.maxGap, 'f', 1));
            // to the outside of the figure, and never on top of another note
            const bool right = labelAt.x() >= centre.x();
            QRectF box = fm.boundingRect(text);
            box.moveTopLeft(labelAt + QPointF(right ? 8 : -8 - box.width(), -box.height() / 2));
            for (int guard = 0; guard < 20; ++guard)
            {
                bool clash = false;
                for (const QRectF &r : taken) if (r.intersects(box)) { clash = true; break; }
                if (!clash) break;
                box.translate(0, box.height() * 0.9);
            }
            taken.append(box);
            p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    }

    // pins: a red head on the form; on a flat piece, a thread to the point of the piece it holds
    p.setBrush(redPencil);
    for (const PreviewPin &pin : m_previewPins)
    {
        const QPointF on = screen(view(pin.on));
        if (!m_showSeams)
        {
            p.setPen(QPen(redPencil, 0.8, Qt::DotLine));
            p.drawLine(on, screen(view(pin.from)));
        }
        p.setPen(QPen(paperLight, 1));
        p.drawEllipse(on, 3.4, 3.4);
    }

    // where the click would go, with its place on the grid
    if (m_picking && m_hover && m_form != nullptr)
    {
        const QPointF at = screen(view(m_form->surfacePoint(m_hoverTheta, m_hoverY)));
        p.setPen(QPen(redPencil, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(at, 5, 5);
        p.drawLine(at + QPointF(-8, 0), at + QPointF(-5, 0));
        p.drawLine(at + QPointF(5, 0), at + QPointF(8, 0));
        QFont hand;
        hand.setFamilies(handFamilies());
        hand.setPointSizeF(10);
        p.setFont(hand);
        const QFontMetricsF fm(hand);
        QRectF box = fm.boundingRect(m_hoverLabel).adjusted(-4, -2, 4, 2);
        box.moveTopLeft(at + QPointF(10, 8));
        if (box.right() > width() - 4) box.moveRight(at.x() - 10);
        if (box.bottom() > height() - 4) box.moveBottom(at.y() - 8);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xFB, 0xF9, 0xF4, 230));
        p.drawRect(box);
        p.setPen(redPencil);
        p.drawText(box, Qt::AlignCenter, m_hoverLabel);
    }
}

void Prova3dView::mousePressEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
    m_pressPos = event->pos();
    if (!m_picking) setCursor(Qt::ClosedHandCursor);
}

void Prova3dView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_picking && !(event->buttons() & Qt::LeftButton))
    {
        double theta, y;
        m_hover = pick(event->pos(), theta, y, !(event->modifiers() & Qt::AltModifier));
        if (m_hover)
        {
            m_hoverTheta = theta;
            m_hoverY = y;
            m_hoverLabel = prova3d::gridLabel(*m_form, theta, y);
        }
        update();
        return;
    }
    if (!(event->buttons() & Qt::LeftButton)) return;
    const QPoint d = event->pos() - m_lastPos;
    m_lastPos = event->pos();
    m_yaw += d.x() * 0.012;
    m_pitch = (std::max)(-0.9, (std::min)(0.9, m_pitch + d.y() * 0.008));
    update();
}

void Prova3dView::wheelEvent(QWheelEvent *event)
{
    m_zoom = (std::max)(0.5, (std::min)(4.0, m_zoom * std::pow(1.0015, event->angleDelta().y())));
    update();
}

void Prova3dView::mouseDoubleClickEvent(QMouseEvent *)
{
    m_yaw = 0.35;
    m_pitch = 0.08;
    m_zoom = 1.0;
    update();
}

// ============================================================================ panel
Prova3dPanel::Prova3dPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("prova3dPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString::fromLatin1(panelStyle));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("3D Fitting"), this);
    QFont tf;
    tf.setFamilies(serifFamilies());
    tf.setPointSizeF(20);
    tf.setItalic(true);
    title->setFont(tf);
    layout->addWidget(title);
    auto *sub = new QLabel(tr("Dress form · size 42"), this);
    sub->setFont(smallCaps(7));
    sub->setObjectName(QStringLiteral("p3section"));
    layout->addWidget(sub);
    layout->addWidget(ruleLine(this));

    m_view = new Prova3dView(this);
    layout->addWidget(m_view, 1);

    // what goes on the form: the pattern, or the test bodice
    layout->addSpacing(4);
    auto *source = new QHBoxLayout();
    source->setSpacing(14);
    m_patternSource = new QPushButton(tr("Pattern"), this);
    m_demoSource = new QPushButton(tr("Test bodice"), this);
    for (QPushButton *b : {m_patternSource, m_demoSource})
    {
        b->setObjectName(QStringLiteral("p3toggle"));
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setFont(smallCaps(7));
        b->setCursor(Qt::PointingHandCursor);
        source->addWidget(b);
    }
    source->addStretch(1);
    m_setupButton = new QPushButton(tr("3D setup\u2026"), this);
    m_setupButton->setObjectName(QStringLiteral("p3quiet"));
    m_setupButton->setCursor(Qt::PointingHandCursor);
    source->addWidget(m_setupButton);
    layout->addLayout(source);
    connect(m_patternSource, &QPushButton::clicked, this, [this]() { setSource(true); });
    connect(m_demoSource, &QPushButton::clicked, this, [this]() { setSource(false); });
    connect(m_setupButton, &QPushButton::clicked, this, &Prova3dPanel::openSetup);

    // pins: a point of a piece on a point of the form
    m_pinBox = new QWidget(this);
    {
        auto *pinLayout = new QVBoxLayout(m_pinBox);
        pinLayout->setContentsMargins(0, 2, 0, 0);
        pinLayout->setSpacing(4);
        pinLayout->addWidget(sectionLabel(tr("Pins"), this));
        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        m_pinPiece = new QComboBox(m_pinBox);
        m_pinPoint = new QComboBox(m_pinBox);
        m_pinButton = new QPushButton(tr("pin on the form"), m_pinBox);
        m_pinButton->setCheckable(true);
        m_pinButton->setFont(smallCaps(7));
        m_pinButton->setCursor(Qt::PointingHandCursor);
        m_pinButton->setToolTip(tr("Then click on the form: the point snaps to the grid (hold Alt for a free point)"));
        row->addWidget(m_pinPiece, 1);
        row->addWidget(m_pinPoint, 1);
        row->addWidget(m_pinButton);
        pinLayout->addLayout(row);
        m_pinList = new QListWidget(m_pinBox);
        m_pinList->setObjectName(QStringLiteral("p3pins"));
        m_pinList->setMaximumHeight(84);
        m_pinList->setFont(noteFont());
        pinLayout->addWidget(m_pinList);
        auto *remove = new QPushButton(tr("remove the selected pin"), m_pinBox);
        remove->setObjectName(QStringLiteral("p3quiet"));
        remove->setFont(noteFont());
        remove->setCursor(Qt::PointingHandCursor);
        auto *removeRow = new QHBoxLayout();
        removeRow->addStretch(1);
        removeRow->addWidget(remove);
        pinLayout->addLayout(removeRow);
        connect(remove, &QPushButton::clicked, this, &Prova3dPanel::removePin);
        connect(m_pinButton, &QPushButton::toggled, this, &Prova3dPanel::startPin);
        connect(m_pinPiece, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]()
        {
            m_pinPoint->clear();
            if (m_data == nullptr) return;
            for (const prova3d::NodeInfo &n : prova3d::pieceNodes(m_data, m_pinPiece->currentData().toUInt()))
                m_pinPoint->addItem(n.name, n.id);
        });
        connect(m_view, &Prova3dView::picked, this, &Prova3dPanel::addPin);
    }
    layout->addWidget(m_pinBox);

    m_demoBox = new QWidget(this);
    auto *bodice = new QGridLayout(m_demoBox);
    bodice->setContentsMargins(0, 0, 0, 0);
    bodice->setHorizontalSpacing(14);
    QFont labelFont;
    labelFont.setFamilies(serifFamilies());
    labelFont.setPointSizeF(10);
    labelFont.setItalic(true);
    auto addField = [&](QGridLayout *grid, int row, int col, const QString &label, QDoubleSpinBox *box)
    {
        auto *l = new QLabel(label, this);
        l->setFont(labelFont);
        grid->addWidget(l, row, col * 2);
        grid->addWidget(box, row, col * 2 + 1);
    };
    m_width = field(80, 120, 100, 1, 0, QStringLiteral(" %"), this);
    m_width->setToolTip(tr("Width of the test bodice: below 100 % it gets tighter"));
    addField(bodice, 0, 0, tr("width"), m_width);
    bodice->setColumnStretch(2, 1);
    layout->addWidget(m_demoBox);

    layout->addSpacing(2);
    layout->addWidget(sectionLabel(tr("Cloth for the drop"), this));
    auto *cloth = new QGridLayout();
    cloth->setHorizontalSpacing(14);
    cloth->setVerticalSpacing(6);
    m_warp = field(0, 60, 0, 0.5, 1, QStringLiteral(" %"), this);
    m_weft = field(0, 60, 0, 0.5, 1, QStringLiteral(" %"), this);
    m_lock = field(5, 60, 35, 1, 0, QStringLiteral("°"), this);
    m_weight = field(30, 600, 150, 10, 0, QStringLiteral(" g/m²"), this);
    m_warp->setToolTip(tr("How much the warp threads can stretch (0 for a woven without elastane)"));
    m_weft->setToolTip(tr("How much the weft threads can stretch"));
    m_lock->setToolTip(tr("Locking angle: how far the bias gives before the threads jam"));
    addField(cloth, 0, 0, tr("warp"), m_warp);
    addField(cloth, 0, 1, tr("weft"), m_weft);
    addField(cloth, 1, 0, tr("locking angle"), m_lock);
    addField(cloth, 1, 1, tr("weight"), m_weight);
    layout->addLayout(cloth);

    layout->addWidget(ruleLine(this));
    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    m_paperButton = new QPushButton(tr("Fit in paper"), this);
    m_paperButton->setObjectName(QStringLiteral("p3primary"));
    m_dropButton = new QPushButton(tr("Drop"), this);
    m_restartButton = new QPushButton(tr("start over"), this);
    m_restartButton->setObjectName(QStringLiteral("p3quiet"));
    for (QPushButton *b : {m_paperButton, m_dropButton})
    {
        b->setFont(smallCaps(7));
        b->setCursor(Qt::PointingHandCursor);
    }
    QFont quiet;
    quiet.setFamilies(serifFamilies());
    quiet.setItalic(true);
    quiet.setPointSizeF(10);
    m_restartButton->setFont(quiet);
    m_setupButton->setFont(quiet);
    m_restartButton->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_paperButton);
    buttons->addWidget(m_dropButton);
    buttons->addStretch(1);
    buttons->addWidget(m_restartButton);
    layout->addLayout(buttons);

    m_status = new QLabel(this);
    m_status->setFont(quiet);
    m_status->setObjectName(QStringLiteral("p3section"));
    layout->addWidget(m_status);

    m_notes = new QLabel(this);
    m_notes->setWordWrap(true);
    m_notes->setTextFormat(Qt::RichText);
    m_notes->setMinimumHeight(48);
    layout->addWidget(m_notes);

    connect(m_paperButton, &QPushButton::clicked, this, &Prova3dPanel::fitInPaper);
    connect(m_dropButton, &QPushButton::clicked, this, &Prova3dPanel::drop);
    connect(m_restartButton, &QPushButton::clicked, this, &Prova3dPanel::restart);
    connect(this, &Prova3dPanel::phaseFinished, this, &Prova3dPanel::onPhaseFinished, Qt::QueuedConnection);

    m_dropButton->setEnabled(false);
    setSource(true);
}

void Prova3dPanel::setPattern(VAbstractPattern *doc, const VContainer *data)
{
    m_doc = doc;
    m_data = data;
}

void Prova3dPanel::refreshPins()
{
    if (m_pinPiece == nullptr) return;
    // pieces of the pattern
    const quint32 current = m_pinPiece->currentData().toUInt();
    m_pinPiece->blockSignals(true);
    m_pinPiece->clear();
    if (m_data != nullptr)
    {
        QList<quint32> ids = m_data->DataPieces()->keys();
        std::sort(ids.begin(), ids.end());
        for (quint32 id : ids)
        {
            m_pinPiece->addItem(prova3d::pieceName(m_data, id), id);
            if (id == current) m_pinPiece->setCurrentIndex(m_pinPiece->count() - 1);
        }
    }
    m_pinPiece->blockSignals(false);
    emit m_pinPiece->currentIndexChanged(m_pinPiece->currentIndex());

    m_pinList->clear();
    if (m_doc == nullptr || m_data == nullptr || m_form == nullptr) return;
    const prova3d::Setup setup = prova3d::Setup::load(m_doc);
    for (int i = 0; i < setup.anchors.size(); ++i)
    {
        const prova3d::AnchorSetup &a = setup.anchors.at(i);
        QString point = QString::number(a.node);
        for (const prova3d::NodeInfo &n : prova3d::pieceNodes(m_data, a.piece)) if (n.id == a.node) point = n.name;
        auto *item = new QListWidgetItem(QStringLiteral("%1 \u00b7 %2  \u2192  %3")
                                         .arg(prova3d::pieceName(m_data, a.piece), point,
                                              prova3d::gridLabel(*m_form, a.theta, a.y)));
        item->setData(Qt::UserRole, i);
        m_pinList->addItem(item);
    }
}

void Prova3dPanel::startPin(bool on)
{
    if (on && (m_data == nullptr || m_pinPoint->count() == 0 || m_form == nullptr))
    {
        m_pinButton->setChecked(false);
        m_status->setText(tr("Open a pattern with pieces first."));
        return;
    }
    m_view->setPicking(on);
    if (on) m_status->setText(tr("Click on the form where %1 goes.").arg(m_pinPoint->currentText()));
}

void Prova3dPanel::addPin(double theta, double y)
{
    if (m_doc == nullptr) return;
    prova3d::Setup setup = prova3d::Setup::load(m_doc);
    prova3d::AnchorSetup a;
    a.piece = m_pinPiece->currentData().toUInt();
    a.node = m_pinPoint->currentData().toUInt();
    a.theta = theta;
    a.y = y;
    // one pin per point: a new one replaces the old
    for (int i = setup.anchors.size() - 1; i >= 0; --i)
        if (setup.anchors.at(i).piece == a.piece && setup.anchors.at(i).node == a.node) setup.anchors.remove(i);
    setup.anchors << a;
    if (setup.piece(a.piece) == nullptr)
    {
        prova3d::PieceSetup p;
        p.id = a.piece;
        p.mode = prova3d::Mode::Single;
        setup.pieces << p;
    }
    setup.save(m_doc);
    m_pinButton->setChecked(false);
    m_status->setText(tr("%1 pinned at %2").arg(m_pinPoint->currentText(), prova3d::gridLabel(*m_form, theta, y)));
    // a new pin starts the fitting over: the flat pieces show where things go
    m_garment.reset();
    m_dropped = false;
    m_view->setGarment(nullptr, {});
    m_notes->clear();
    refreshPins();
    refreshPreview();
}

void Prova3dPanel::removePin()
{
    QListWidgetItem *item = m_pinList->currentItem();
    if (item == nullptr || m_doc == nullptr) return;
    prova3d::Setup setup = prova3d::Setup::load(m_doc);
    const int i = item->data(Qt::UserRole).toInt();
    if (i >= 0 && i < setup.anchors.size()) setup.anchors.remove(i);
    setup.save(m_doc);
    refreshPins();
    refreshPreview();
}

void Prova3dPanel::refreshPreview()
{
    if (m_garment) return;   // a fitting is on show
    std::vector<Prova3dView::PreviewPin> pins;
    std::shared_ptr<sim::Garment> start;
    if (m_usePattern && m_doc != nullptr && m_data != nullptr && m_form && !m_data->DataPieces()->isEmpty())
    {
        prova3d::Input input;
        QString error;
        const prova3d::Setup setup = prova3d::Setup::load(m_doc);
        if (!setup.anchors.isEmpty() && prova3d::buildInput(setup, m_data, input, &error))
        {
            prova3d::applyAnchors(input, *m_form);
            for (const auto &a : input.anchors)
                pins.push_back({m_form->surfacePoint(a.theta, a.y),
                                sim::initialPosition(input.pieces[size_t(a.piece)], a.at, 23.0, m_form.get())});
            // the pieces where they start: that is where the pins put them
            start = std::make_shared<sim::Garment>(input.pieces, input.seams, *m_form, 2.0, 23.0);
        }
    }
    m_view->setShowSeams(false);
    m_view->setGarment(start, {});
    m_view->setPreview({}, pins);
}

void Prova3dPanel::setSource(bool pattern)
{
    m_usePattern = pattern;
    m_patternSource->setChecked(pattern);
    m_demoSource->setChecked(!pattern);
    m_demoBox->setVisible(!pattern);
    m_setupButton->setVisible(pattern);
    m_pinBox->setVisible(pattern);
    if (!pattern && m_pinButton->isChecked()) m_pinButton->setChecked(false);
    refreshPins();
    refreshPreview();
    if (m_worker == nullptr)
    {
        m_status->setText(pattern ? tr("Set up the pieces and the seams, then fit the pattern in paper.")
                                  : tr("Put the test bodice on the form and fit it in paper."));
    }
}

void Prova3dPanel::openSetup()
{
    if (m_doc == nullptr || m_data == nullptr || m_data->DataPieces()->isEmpty())
    {
        m_status->setText(tr("Open a pattern with pieces first."));
        return;
    }
    Prova3dSetupDialog dialog(prova3d::Setup::load(m_doc), m_data, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        prova3d::Setup saved = dialog.setup();
        saved.anchors = prova3d::Setup::load(m_doc).anchors;   // the pins are set here, not in the window
        saved.save(m_doc);
        m_status->setText(tr("Set up saved with the pattern."));
        refreshPins();
        refreshPreview();
    }
}

Prova3dPanel::~Prova3dPanel()
{
    if (m_worker != nullptr)
    {
        if (m_garment) m_garment->requestStop();
        m_worker->wait();
    }
}

sim::Material Prova3dPanel::fabric() const
{
    sim::Material m = sim::Material::fabricDefault();
    m.stretchWarp = m_warp->value() / 100.0;
    m.stretchWeft = m_weft->value() / 100.0;
    m.lockAngleDeg = m_lock->value();
    m.density = m_weight->value() / 1000.0;
    return m;
}

void Prova3dPanel::setBusy(bool busy)
{
    m_paperButton->setEnabled(!busy);
    m_dropButton->setEnabled(!busy && m_garment && !m_dropped);
    m_restartButton->setEnabled(!busy);
    m_width->setEnabled(!busy);
}

void Prova3dPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_form && m_worker == nullptr) runPhase(0);   // the form shows up as soon as the panel opens
}

void Prova3dPanel::fitInPaper()
{
    runPhase(1);
}

void Prova3dPanel::drop()
{
    runPhase(2);
}

void Prova3dPanel::restart()
{
    m_garment.reset();
    m_dropped = false;
    m_view->setGarment(nullptr, {});
    m_notes->clear();
    setSource(m_usePattern);
    setBusy(false);
}

void Prova3dPanel::runPhase(int phase)
{
    if (m_worker != nullptr) return;
    setBusy(true);
    m_notes->clear();
    m_status->setText(phase == 1 ? tr("Putting it on and fitting in paper…") : tr("Pins out, the cloth drops…"));

    const double width = m_width->value() / 100.0;
    const bool usePattern = m_usePattern;
    prova3d::Input input;
    if (phase == 1 && usePattern)
    {
        QString error;
        if (m_doc == nullptr || m_data == nullptr
            || !prova3d::buildInput(prova3d::Setup::load(m_doc), m_data, input, &error))
        {
            m_status->setText(error.isEmpty() ? tr("Open a pattern with pieces first.") : error);
            setBusy(false);
            return;
        }
        if (input.seams.empty())
        {
            m_status->setText(tr("No seams yet: add them in the 3D setup."));
            setBusy(false);
            return;
        }
    }
    const sim::Material cloth = fabric();
    const bool needForm = !m_form;
    QPointer<Prova3dView> view = m_view;

    m_worker = QThread::create([this, phase, width, cloth, needForm, view, usePattern, input]()
    {
        QElapsedTimer timer;
        timer.start();
        if (needForm)
        {
            m_form = std::make_unique<sim::DressForm>(sim::DressForm::standard42());
            const sim::DressForm *form = m_form.get();
            QMetaObject::invokeMethod(this, [view, form]() { if (view) view->setForm(form); }, Qt::QueuedConnection);
        }
        auto repaint = [view]() { QMetaObject::invokeMethod(view, "update", Qt::QueuedConnection); };

        const double seamForce = 5e-4;
        QString summary;
        bool pinsKept = false;
        if (phase == 1)
        {
            sim::Demo demo;
            std::vector<sim::PinInput> userPins;
            if (usePattern)
            {
                prova3d::Input in = input;
                prova3d::applyAnchors(in, *m_form);
                demo.pieces = in.pieces;
                demo.seams = in.seams;
                demo.pins = in.pins;
                for (const auto &a : in.anchors)
                {
                    sim::V3 n;
                    const sim::V3 on = m_form->surfacePoint(a.theta, a.y, &n);
                    userPins.push_back({a.piece, a.at, on + n * 0.4});
                }
            }
            else
            {
                demo = sim::demoBodice(width);
            }
            auto garment = std::make_shared<sim::Garment>(demo.pieces, demo.seams, *m_form, 1.5, 23.0);
            garment->setProgress(repaint);
            std::vector<QString> names;
            for (const sim::SeamInput &s : demo.seams) names.push_back(QString::fromStdString(s.name));
            m_seamNames = names;
            m_garment = garment;
            QMetaObject::invokeMethod(this, [this, view, garment, names]()
            {
                if (view) { view->setShowSeams(true); view->setGarment(garment, names); }
            }, Qt::QueuedConnection);

            // putting it on: soft and slippery
            sim::Material dressing = sim::Material::fabricDefault();
            dressing.friction = 0;
            dressing.lockAngleDeg = 45;
            garment->setMaterial(dressing);
            garment->setLatchSeams(false);
            // the user's pins hold from the start; the cloth is drawn gently onto the form
            if (!userPins.empty())
            {
                garment->setPins(userPins);
                garment->setAttraction(0.03);
            }
            garment->settle(0, 4000, 0.002, seamForce * 10);
            garment->setAttraction(0);

            // paper: pinned at centre front and back, assembled, then its weight
            garment->setMaterial(sim::Material::paperDefault());
            garment->setLatchSeams(true);
            garment->latchClosedSeams(1.0);   // what closed while dressing starts sewn: the paper opens what it can't hold
            std::vector<sim::PinInput> pins;
            for (const auto &pp : demo.pins) pins.push_back({pp.first, pp.second, garment->nodeAt(pp.first, pp.second)});
            pins.insert(pins.end(), userPins.begin(), userPins.end());
            garment->setPins(pins);
            garment->settle(0, 4000, 0.002, seamForce);
            garment->settle(981, 4000, 0.002, seamForce);
            summary = tr("Paper fitting: %1 s").arg(QLocale().toString(timer.elapsed() / 1000.0, 'f', 1));
        }
        else if (phase == 0)
        {
            // only the form: the status line stays as it is
        }
        else if (m_garment)
        {
            // a seam still open can't hold the garment: the pins stay, as they would on the form
            bool open = false;
            for (const sim::Garment::OpenSeam &os : m_garment->openSeams())
            {
                const sim::SeamInput &si = m_garment->seams()[size_t(os.seam)];
                if (si.pieceA != si.pieceB && os.sewnFraction < 0.5) open = true;
            }
            if (!open) m_garment->releasePins();
            pinsKept = open;
            m_garment->setMaterial(cloth);
            m_garment->settle(981, 6000, 0.002, seamForce);
            summary = tr("Drop: %1 s").arg(QLocale().toString(timer.elapsed() / 1000.0, 'f', 1));
            if (pinsKept) summary += QStringLiteral(" \u2014 ") + tr("some seams are open, the pins stay in");
        }
        emit phaseFinished(summary);
    });
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &QThread::finished, this, [this]()
    {
        m_worker = nullptr;
        setBusy(false);
        refreshNotes();
        refreshPins();
        refreshPreview();
        m_view->update();
    });
    if (phase == 2) m_dropped = true;
    m_worker->start();
}

void Prova3dPanel::onPhaseFinished(const QString &summary)
{
    if (!summary.isEmpty()) m_status->setText(summary);
}

void Prova3dPanel::refreshNotes()
{
    if (!m_garment)
    {
        m_notes->clear();
        return;
    }
    const QString handFont = handFamilies().join(QStringLiteral("','"));
    const QString serif = serifFamilies().join(QStringLiteral("','"));
    QStringList open;
    QStringList closed;
    for (const sim::Garment::OpenSeam &os : m_garment->openSeams())
    {
        const QString name = os.seam < int(m_seamNames.size()) ? m_seamNames[size_t(os.seam)] : QString::number(os.seam);
        if (os.maxGap >= 0.3)
        {
            open << tr("%1 — %2 cm missing").arg(name, QLocale().toString(os.maxGap, 'f', 1));
        }
        else
        {
            closed << name;
        }
    }
    QString html;
    for (const QString &line : open)
    {
        html += QStringLiteral("<div style=\"font-family:'%1'; color:#A8261E; font-size:13pt;\">%2</div>")
                    .arg(handFont, line.toHtmlEscaped());
    }
    if (!closed.isEmpty())
    {
        html += QStringLiteral("<div style=\"font-family:'%1'; font-style:italic; color:#6E685D; font-size:9pt; "
                               "margin-top:4px;\">%2 %3</div>")
                    .arg(serif, tr("Closed:").toHtmlEscaped(), closed.join(QStringLiteral(", ")).toHtmlEscaped());
    }
    m_notes->setText(html);
}
