/******************************************************************************
 **  @file   prova3d_setup_dialog.cpp
 **  @brief  Prova 3D: the window where the pieces and the seams of the pattern are set up.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#include "prova3d_setup_dialog.h"
#include "prova3d_panel.h"

#include "../vgeometry/vpointf.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "../vmisc/def.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
enum PieceCol { PUse, PName, PMode, PC1, PC2, PPos, PCount };
enum SeamCol { SA, SA1, SA2, SB, SB1, SB2, SRev, SSides, SCount };

template <typename T>
T *cell(QTableWidget *t, int row, int col) { return qobject_cast<T *>(t->cellWidget(row, col)); }

QWidget *centred(QWidget *w)
{
    auto *holder = new QWidget;
    auto *l = new QHBoxLayout(holder);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(w, 0, Qt::AlignCenter);
    return holder;
}

// a guess of the centre line: the two nodes furthest apart on the outermost straight vertical edge
void guessCenter(const VContainer *data, quint32 pieceId, quint32 &c1, quint32 &c2)
{
    const QVector<prova3d::NodeInfo> nodes = prova3d::pieceNodes(data, pieceId);
    if (nodes.size() < 2) return;
    QVector<QPointF> pos;
    for (const prova3d::NodeInfo &n : nodes) pos << data->GeometricObject<VPointF>(n.id)->toQPointF();
    int minX = 0;
    for (int i = 1; i < pos.size(); ++i) if (pos[i].x() < pos[minX].x()) minX = i;
    int other = -1;
    double span = 0;
    const double tol = ToPixel(0.5, Unit::Cm);
    for (int i = 0; i < pos.size(); ++i)
    {
        if (i == minX || std::fabs(pos[i].x() - pos[minX].x()) > tol) continue;
        const double d = std::fabs(pos[i].y() - pos[minX].y());
        if (d > span) { span = d; other = i; }
    }
    if (other < 0) return;
    c1 = nodes[pos[minX].y() < pos[other].y() ? minX : other].id;
    c2 = nodes[pos[minX].y() < pos[other].y() ? other : minX].id;
}
} // namespace

Prova3dSetupDialog::Prova3dSetupDialog(const prova3d::Setup &setup, const VContainer *data, QWidget *parent)
    : QDialog(parent), m_data(data)
{
    setObjectName(QStringLiteral("prova3dPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(Prova3dPanel::styleSheetText());
    setWindowTitle(tr("3D Setup"));
    resize(980, 620);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 16);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("3D Setup"), this);
    title->setFont(Prova3dPanel::titleFont());
    layout->addWidget(title);
    auto *hint = new QLabel(tr("Halves are made whole on the form; a seam set on the half is sewn on both halves, "
                               "unless it is separated."), this);
    hint->setWordWrap(true);
    hint->setFont(Prova3dPanel::noteFont());
    hint->setObjectName(QStringLiteral("p3section"));
    layout->addWidget(hint);

    // pieces
    layout->addWidget(Prova3dPanel::sectionLabel(tr("Pieces"), this));
    m_pieces = new QTableWidget(0, PCount, this);
    m_pieces->setHorizontalHeaderLabels({QString(), tr("Piece"), tr("Made whole"), tr("Centre line from"),
                                         tr("to"), tr("On the form")});
    m_pieces->verticalHeader()->hide();
    m_pieces->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pieces->horizontalHeader()->setSectionResizeMode(PUse, QHeaderView::ResizeToContents);
    m_pieces->setSelectionMode(QAbstractItemView::NoSelection);
    m_pieces->setShowGrid(false);
    layout->addWidget(m_pieces, 2);

    const QHash<quint32, VPiece> *all = data->DataPieces();
    QList<quint32> ids = all->keys();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : ids)
    {
        m_pieceIds << id;
        if (const prova3d::PieceSetup *p = setup.piece(id))
        {
            addPieceRow(*p);
            continue;
        }
        prova3d::PieceSetup p;
        p.id = id;
        guessCenter(data, id, p.center1, p.center2);
        const QString name = prova3d::pieceName(data, id).toLower();
        if (name.contains(QLatin1String("diet")) || name.contains(QLatin1String("back")))
            p.position = prova3d::Position::Back;
        addPieceRow(p);
    }

    // seams
    layout->addSpacing(6);
    layout->addWidget(Prova3dPanel::sectionLabel(tr("Seams"), this));
    m_seams = new QTableWidget(0, SCount, this);
    m_seams->setHorizontalHeaderLabels({tr("Piece"), tr("from"), tr("to"), tr("sewn with"), tr("from"), tr("to"),
                                        tr("other way"), tr("Halves")});
    m_seams->verticalHeader()->hide();
    m_seams->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_seams->horizontalHeader()->setSectionResizeMode(SRev, QHeaderView::ResizeToContents);
    m_seams->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_seams->setShowGrid(false);
    m_seams->setColumnHidden(SRev, true);   // each edge is walked the short way: no need to say which
    layout->addWidget(m_seams, 3);
    for (const prova3d::SeamSetup &s : setup.seams) addSeamRow(s);

    auto *seamButtons = new QHBoxLayout();
    auto *add = new QPushButton(tr("+ add a seam"), this);
    auto *remove = new QPushButton(tr("remove the selected one"), this);
    for (QPushButton *b : {add, remove})
    {
        b->setObjectName(QStringLiteral("p3quiet"));
        b->setFont(Prova3dPanel::noteFont());
        b->setCursor(Qt::PointingHandCursor);
        seamButtons->addWidget(b);
    }
    seamButtons->addStretch(1);
    layout->addLayout(seamButtons);
    connect(add, &QPushButton::clicked, this, &Prova3dSetupDialog::addSeam);
    connect(remove, &QPushButton::clicked, this, &Prova3dSetupDialog::removeSeam);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    auto *ok = new QPushButton(tr("Save"), this);
    ok->setObjectName(QStringLiteral("p3primary"));
    for (QPushButton *b : {cancel, ok})
    {
        b->setFont(Prova3dPanel::capsFont());
        b->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(b);
    }
    layout->addLayout(buttons);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

QComboBox *Prova3dSetupDialog::pieceCombo(quint32 current) const
{
    auto *box = new QComboBox;
    for (quint32 id : m_pieceIds)
    {
        box->addItem(prova3d::pieceName(m_data, id), id);
        if (id == current) box->setCurrentIndex(box->count() - 1);
    }
    return box;
}

void Prova3dSetupDialog::fillNodes(QComboBox *box, quint32 pieceId, quint32 current) const
{
    box->blockSignals(true);
    box->clear();
    box->addItem(QStringLiteral("—"), 0u);
    for (const prova3d::NodeInfo &n : prova3d::pieceNodes(m_data, pieceId))
    {
        box->addItem(n.name, n.id);
        if (n.id == current) box->setCurrentIndex(box->count() - 1);
    }
    box->blockSignals(false);
}

void Prova3dSetupDialog::addPieceRow(const prova3d::PieceSetup &p)
{
    const int row = m_pieces->rowCount();
    m_pieces->insertRow(row);

    auto *use = new QCheckBox;
    use->setChecked(p.use);
    use->setProperty("pieceId", p.id);
    m_pieces->setCellWidget(row, PUse, centred(use));

    auto *name = new QTableWidgetItem(prova3d::pieceName(m_data, p.id));
    name->setFlags(Qt::ItemIsEnabled);
    name->setData(Qt::UserRole, p.id);
    m_pieces->setItem(row, PName, name);

    auto *mode = new QComboBox;
    for (prova3d::Mode m : {prova3d::Mode::Fold, prova3d::Mode::Pair, prova3d::Mode::Single})
    {
        mode->addItem(prova3d::modeName(m), int(m));
        if (m == p.mode) mode->setCurrentIndex(mode->count() - 1);
    }
    m_pieces->setCellWidget(row, PMode, mode);

    auto *c1 = new QComboBox;
    auto *c2 = new QComboBox;
    fillNodes(c1, p.id, p.center1);
    fillNodes(c2, p.id, p.center2);
    m_pieces->setCellWidget(row, PC1, c1);
    m_pieces->setCellWidget(row, PC2, c2);

    auto *pos = new QComboBox;
    for (prova3d::Position q : {prova3d::Position::Front, prova3d::Position::Back, prova3d::Position::FrontWaist,
                                prova3d::Position::BackWaist})
    {
        pos->addItem(prova3d::positionName(q), int(q));
        if (q == p.position) pos->setCurrentIndex(pos->count() - 1);
    }
    m_pieces->setCellWidget(row, PPos, pos);
    m_pieces->setRowHeight(row, 34);
}

void Prova3dSetupDialog::addSeamRow(const prova3d::SeamSetup &s)
{
    const int row = m_seams->rowCount();
    m_seams->insertRow(row);
    auto *pa = pieceCombo(s.pieceA);
    auto *pb = pieceCombo(s.pieceB);
    auto *a1 = new QComboBox, *a2 = new QComboBox, *b1 = new QComboBox, *b2 = new QComboBox;
    fillNodes(a1, pa->currentData().toUInt(), s.a1);
    fillNodes(a2, pa->currentData().toUInt(), s.a2);
    fillNodes(b1, pb->currentData().toUInt(), s.b1);
    fillNodes(b2, pb->currentData().toUInt(), s.b2);
    connect(pa, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pa, a1, a2]()
    {
        fillNodes(a1, pa->currentData().toUInt(), 0);
        fillNodes(a2, pa->currentData().toUInt(), 0);
    });
    connect(pb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, pb, b1, b2]()
    {
        fillNodes(b1, pb->currentData().toUInt(), 0);
        fillNodes(b2, pb->currentData().toUInt(), 0);
    });
    m_seams->setCellWidget(row, SA, pa);
    m_seams->setCellWidget(row, SA1, a1);
    m_seams->setCellWidget(row, SA2, a2);
    m_seams->setCellWidget(row, SB, pb);
    m_seams->setCellWidget(row, SB1, b1);
    m_seams->setCellWidget(row, SB2, b2);
    auto *rev = new QCheckBox;
    rev->setChecked(s.reverse);
    rev->setToolTip(tr("Walk the second edge the other way round"));
    m_seams->setCellWidget(row, SRev, centred(rev));
    auto *sides = new QComboBox;
    for (prova3d::Sides q : {prova3d::Sides::Both, prova3d::Sides::Right, prova3d::Sides::Left})
    {
        sides->addItem(prova3d::sidesName(q), int(q));
        if (q == s.sides) sides->setCurrentIndex(sides->count() - 1);
    }
    sides->setToolTip(tr("Separate seams: sew this one on one half only"));
    m_seams->setCellWidget(row, SSides, sides);
    m_seams->setRowHeight(row, 34);
}

void Prova3dSetupDialog::addSeam()
{
    prova3d::SeamSetup s;
    if (!m_pieceIds.isEmpty())
    {
        s.pieceA = m_pieceIds.first();
        s.pieceB = m_pieceIds.size() > 1 ? m_pieceIds.at(1) : m_pieceIds.first();
    }
    addSeamRow(s);
    m_seams->scrollToBottom();
}

void Prova3dSetupDialog::removeSeam()
{
    const int row = m_seams->currentRow();
    if (row >= 0) m_seams->removeRow(row);
}

prova3d::Setup Prova3dSetupDialog::setup() const
{
    prova3d::Setup s;
    for (int row = 0; row < m_pieces->rowCount(); ++row)
    {
        prova3d::PieceSetup p;
        p.id = m_pieces->item(row, PName)->data(Qt::UserRole).toUInt();
        if (QWidget *holder = m_pieces->cellWidget(row, PUse))
            if (auto *use = holder->findChild<QCheckBox *>()) p.use = use->isChecked();
        p.mode = prova3d::Mode(cell<QComboBox>(m_pieces, row, PMode)->currentData().toInt());
        p.center1 = cell<QComboBox>(m_pieces, row, PC1)->currentData().toUInt();
        p.center2 = cell<QComboBox>(m_pieces, row, PC2)->currentData().toUInt();
        p.position = prova3d::Position(cell<QComboBox>(m_pieces, row, PPos)->currentData().toInt());
        s.pieces << p;
    }
    for (int row = 0; row < m_seams->rowCount(); ++row)
    {
        prova3d::SeamSetup m;
        m.pieceA = cell<QComboBox>(m_seams, row, SA)->currentData().toUInt();
        m.a1 = cell<QComboBox>(m_seams, row, SA1)->currentData().toUInt();
        m.a2 = cell<QComboBox>(m_seams, row, SA2)->currentData().toUInt();
        m.pieceB = cell<QComboBox>(m_seams, row, SB)->currentData().toUInt();
        m.b1 = cell<QComboBox>(m_seams, row, SB1)->currentData().toUInt();
        m.b2 = cell<QComboBox>(m_seams, row, SB2)->currentData().toUInt();
        if (QWidget *holder = m_seams->cellWidget(row, SRev))
            if (auto *rev = holder->findChild<QCheckBox *>()) m.reverse = rev->isChecked();
        m.sides = prova3d::Sides(cell<QComboBox>(m_seams, row, SSides)->currentData().toInt());
        if (m.a1 == 0 || m.a2 == 0 || m.b1 == 0 || m.b2 == 0) continue;
        s.seams << m;
    }
    return s;
}
