/******************************************************************************
 **  @file   prova3d_panel.cpp
 **  @brief  Prova 3D: the panel beside the pattern (view of the form, the fitting and the drop).
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#include "prova3d_panel.h"

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
#prova3dPanel QDoubleSpinBox::up-button, #prova3dPanel QDoubleSpinBox::down-button { width: 0px; border: none; }
)";
} // namespace

// ============================================================================ view
Prova3dView::Prova3dView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 320);
    setCursor(Qt::OpenHandCursor);
}

void Prova3dView::setForm(const sim::DressForm *form)
{
    m_formVerts.clear();
    m_formTris.clear();
    if (form != nullptr)
    {
        form->mesh(m_formVerts, m_formTris, 56);
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

    const double cy = std::cos(m_yaw), sy = std::sin(m_yaw), cp = std::cos(m_pitch), sp = std::sin(m_pitch);
    const double scale = std::min(width() / 70.0, height() / 92.0) * m_zoom;
    const QPointF centre(width() / 2.0, height() / 2.0);
    const double midY = 116.0;
    auto view = [&](const sim::V3 &v) {
        const double x = v.x * cy + v.z * sy;
        const double z0 = -v.x * sy + v.z * cy;
        const double y0 = v.y - midY;
        const double y = y0 * cp - z0 * sp;
        const double z = y0 * sp + z0 * cp;
        return sim::V3(x, y, z);
    };
    auto screen = [&](const sim::V3 &v) { return QPointF(centre.x() + v.x * scale, centre.y() - v.y * scale); };

    const sim::V3 light = [] { sim::V3 l(-0.35, 0.55, 1.0); return l * (1.0 / l.len()); }();

    struct Item { double depth; int kind; int index; };   // kind 0 form, 1 garment, 2 border
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
                ++edgeCount[{std::min(a, b), std::max(a, b)}];
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
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.depth < b.depth; });

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
        QColor base = isForm ? formColor : pieceColors[std::min(1, std::max(0, gp[it.index]))];
        QColor c = QColor::fromRgbF(base.redF() * shade, base.greenF() * shade, base.blueF() * shade);
        p.setPen(QPen(c, 0.6));
        p.setBrush(c);
        const QPointF poly[3] = {screen(vv[tr[0]]), screen(vv[tr[1]]), screen(vv[tr[2]])};
        p.drawPolygon(poly, 3);
    }

    // the seams that don't close: a red pencil line across the gap and how much is missing
    if (m_garment)
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
}

void Prova3dView::mousePressEvent(QMouseEvent *event)
{
    m_lastPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
}

void Prova3dView::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) return;
    const QPoint d = event->pos() - m_lastPos;
    m_lastPos = event->pos();
    m_yaw += d.x() * 0.012;
    m_pitch = std::max(-0.9, std::min(0.9, m_pitch + d.y() * 0.008));
    update();
}

void Prova3dView::wheelEvent(QWheelEvent *event)
{
    m_zoom = std::max(0.5, std::min(4.0, m_zoom * std::pow(1.0015, event->angleDelta().y())));
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

    // the test bodice
    layout->addSpacing(4);
    layout->addWidget(sectionLabel(tr("Test bodice"), this));
    auto *bodice = new QGridLayout();
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
    layout->addLayout(bodice);

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
    m_status->setText(tr("Put the test bodice on the form and fit it in paper."));
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
    m_status->setText(tr("Put the test bodice on the form and fit it in paper."));
    setBusy(false);
}

void Prova3dPanel::runPhase(int phase)
{
    if (m_worker != nullptr) return;
    setBusy(true);
    m_notes->clear();
    m_status->setText(phase == 1 ? tr("Putting it on and fitting in paper…") : tr("Pins out, the cloth drops…"));

    const double width = m_width->value() / 100.0;
    const sim::Material cloth = fabric();
    const bool needForm = !m_form;
    QPointer<Prova3dView> view = m_view;

    m_worker = QThread::create([this, phase, width, cloth, needForm, view]()
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
            const sim::Demo demo = sim::demoBodice(width);
            auto garment = std::make_shared<sim::Garment>(demo.pieces, demo.seams, *m_form, 1.5, 23.0);
            garment->setProgress(repaint);
            std::vector<QString> names;
            for (const sim::SeamInput &s : demo.seams) names.push_back(QString::fromStdString(s.name));
            m_seamNames = names;
            m_garment = garment;
            QMetaObject::invokeMethod(this, [this, view, garment, names]()
            {
                if (view) view->setGarment(garment, names);
            }, Qt::QueuedConnection);

            // putting it on: soft and slippery
            sim::Material dressing = sim::Material::fabricDefault();
            dressing.friction = 0;
            dressing.lockAngleDeg = 45;
            garment->setMaterial(dressing);
            garment->setLatchSeams(false);
            garment->settle(0, 4000, 0.002, seamForce * 10);

            // paper: pinned at centre front and back, assembled, then its weight
            garment->setMaterial(sim::Material::paperDefault());
            garment->setLatchSeams(true);
            std::vector<sim::PinInput> pins;
            for (const auto &pp : demo.pins) pins.push_back({pp.first, pp.second, garment->nodeAt(pp.first, pp.second)});
            garment->setPins(pins);
            garment->settle(0, 4000, 0.002, seamForce);
            garment->settle(981, 4000, 0.002, seamForce);
            summary = tr("Paper fitting: %1 s").arg(QLocale().toString(timer.elapsed() / 1000.0, 'f', 1));
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
        m_view->update();
    });
    if (phase == 2) m_dropped = true;
    m_worker->start();
}

void Prova3dPanel::onPhaseFinished(const QString &summary)
{
    m_status->setText(summary);
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
