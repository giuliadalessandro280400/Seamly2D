/******************************************************************************
 **  @file   prova3d_setup.h
 **  @brief  Prova 3D: how the pieces of the pattern go on the form (halves, centre line, position)
 **          and which edges are sewn together. Kept in the pattern file, <prova3d> element.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#ifndef PROVA3D_SETUP_H
#define PROVA3D_SETUP_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "prova3d_engine.h"

class VAbstractPattern;
class VContainer;

namespace prova3d
{

// how a piece of the pattern becomes whole on the form
enum class Mode { Fold, Pair, Single };          // on the fold / cut 2 / as it is
enum class Position { Front, Back, FrontWaist, BackWaist };
enum class Sides { Both, Right, Left };          // "separate seams": one half only

struct PieceSetup
{
    quint32  id{0};
    bool     use{true};
    Mode     mode{Mode::Fold};
    quint32  center1{0};    // centre line (or fold) from this node...
    quint32  center2{0};    // ...to this one
    Position position{Position::Front};
};

struct SeamSetup
{
    quint32 pieceA{0}, a1{0}, a2{0};
    quint32 pieceB{0}, b1{0}, b2{0};
    bool    reverse{false};   // B walked the other way round
    Sides   sides{Sides::Both};
};

// a point of a piece pinned on the form (as on the dress form, a pin through the tape)
struct AnchorSetup
{
    quint32 piece{0};
    quint32 node{0};
    double  theta{0};   // around the form: 0 centre front, +pi/2 right side
    double  y{0};       // height, cm
};

struct Setup
{
    QVector<PieceSetup> pieces;
    QVector<SeamSetup>  seams;
    QVector<AnchorSetup> anchors;

    static Setup load(const VAbstractPattern *doc);
    void save(VAbstractPattern *doc) const;
    const PieceSetup *piece(quint32 id) const;
};

/// A point node of a piece, as the user sees it.
struct NodeInfo { quint32 id; QString name; };
QVector<NodeInfo> pieceNodes(const VContainer *data, quint32 pieceId);
QString pieceName(const VContainer *data, quint32 pieceId);

/// What the simulator needs, built from the pattern and the setup.
struct Input
{
    std::vector<sim::PieceInput> pieces;
    std::vector<sim::SeamInput>  seams;
    std::vector<std::pair<int, sim::P2>> pins;
    struct AnchorIn { int piece; sim::P2 at; double theta; double y; };
    std::vector<AnchorIn> anchors;   // user pins: where pieces are laid and held
    QStringList warnings;
};

bool buildInput(const Setup &setup, const VContainer *data, Input &out, QString *error);

/// lays flat the pieces that have pins and turns those pins into held points (pieces without pins keep the
/// automatic placement and pins)
void applyAnchors(Input &in, const sim::DressForm &form);

QString modeName(Mode m);
QString positionName(Position p);
QString sidesName(Sides s);

} // namespace prova3d

#endif // PROVA3D_SETUP_H
