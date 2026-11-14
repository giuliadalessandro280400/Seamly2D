/******************************************************************************
 **  @file   prova3d_engine.h
 **  @brief  Prova 3D: thread-net garment simulator (paper fitting, then the drop in cloth).
 **          The cloth is a net of warp and weft threads; paper is the same net with the shear locked.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/
// Units: centimetres for geometry, seconds, kg/m^2 for density (converted inside).
#pragma once
#include <array>
#include <functional>
#include <map>
#include <mutex>
#include <tuple>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

namespace sim {

struct V3 {
    double x{0}, y{0}, z{0};
    V3() = default;
    V3(double a, double b, double c) : x(a), y(b), z(c) {}
    V3 operator+(const V3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(double s) const { return {x * s, y * s, z * s}; }
    V3 &operator+=(const V3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3 &operator-=(const V3 &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    double dot(const V3 &o) const { return x * o.x + y * o.y + z * o.z; }
    double len() const { return std::sqrt(dot(*this)); }
};
inline V3 cross(const V3 &a, const V3 &b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

struct P2 { double x{0}, y{0}; };

// ---------------------------------------------------------------- dress form
// Horizontal sections from measurements; y is height from the floor (cm), z points to the front.
struct FormLevel {
    double y;          // height
    double circ;       // tape circumference (convex hull)
    double halfWidth;  // lateral half width (shape ratio, rescaled to circ)
    double front;      // depth in front of the axis
    double back;       // depth behind the axis
    double bust;       // bust prominence (cm), 0 elsewhere
};

class DressForm {
public:
    explicit DressForm(std::vector<FormLevel> levels);
    static DressForm standard42();

    // radius of the section at height y in direction theta (theta = atan2(x, z): 0 front, +pi/2 left side)
    double radius(double theta, double y) const;
    // pushes p out of the form (plus thickness). Returns true if it moved; normal = outward normal.
    bool project(V3 &p, double thickness, V3 *normal) const;
    double bottom() const { return m_levels.front().y; }
    double top() const { return m_levels.back().y; }
    void mesh(std::vector<V3> &verts, std::vector<std::array<int, 3>> &tris, int around = 96) const;

private:
    struct Shape { double a, f, b, bust; };
    Shape shapeAt(double y) const;
    static double rawRadius(const Shape &s, double theta);
    std::vector<FormLevel> m_levels;   // sorted by y, already rescaled

    // signed distance field (cm), negative inside
    void buildSdf();
    double sdf(const V3 &p) const;
    double m_cell{0.4};
    V3 m_origin;
    int m_nx{0}, m_ny{0}, m_nz{0};
    std::vector<float> m_sdf;
};

// ---------------------------------------------------------------- material
struct Material {
    bool   paper{true};
    double stretchWarp{0};       // allowed thread elongation (0.03 = 3%)
    double stretchWeft{0};
    double lockAngleDeg{35};     // max shear angle before the threads jam (fabric only)
    double shearCompliance{2e-3};// softness of the shear before locking (fabric)
    double bendCompliance{1e-1}; // bending softness (bigger = softer)
    double density{0.08};        // kg/m^2 (paper 80 g/m^2)
    double thickness{0.15};      // cm, collision gap
    bool   stayStitch{false};    // fabric only: border taped, it doesn't stretch
    double friction{std::getenv("FRIC") ? std::atof(std::getenv("FRIC")) : 0.6};

    static Material paperDefault() { Material m; m.paper = true; m.bendCompliance = std::getenv("PBEND") ? std::atof(std::getenv("PBEND")) : 3.0; m.density = 0.08; return m; }
    static Material fabricDefault() { Material m; m.paper = false; m.bendCompliance = std::getenv("FBEND") ? std::atof(std::getenv("FBEND")) : 5000.0; m.density = 0.15; return m; }
};

// ---------------------------------------------------------------- input
struct PieceInput {
    std::string name;
    std::vector<P2> outline;  // seam line, cm, pattern coordinates (y down), counter or clockwise
    double grainDeg{90};      // direction of the warp in pattern coordinates (90 = along y)
    // placement: wrapped on a cylinder around the form
    double centerX{0};        // pattern x that goes to angle `angleDeg`
    double angleDeg{0};       // 0 front, 180 back
    double topY{0};           // pattern y that goes to height `height`
    double height{140};
    bool   mirrored{false};   // pattern +x goes to -theta
};

struct SeamInput {
    int pieceA, fromA, toA;   // outline vertex indices, walked forward
    int pieceB, fromB, toB;   // walked forward if !reverseB, backward otherwise
    bool reverseB{false};
    std::string name;
};

struct PinInput {
    int piece; P2 at;         // point of the pattern
    V3 to;                    // where it is held (cm)
};

// ---------------------------------------------------------------- simulator
class Garment {
public:
    Garment(const std::vector<PieceInput> &pieces, const std::vector<SeamInput> &seams,
            const DressForm &form, double spacing, double wrapRadius);

    void setMaterial(const Material &m);
    void setPins(const std::vector<PinInput> &pins);
    void releasePins() { m_pins.clear(); }
    // seams that close get sewn (and then hold) only when this is on
    void setLatchSeams(bool on) {
        m_latch = on;
        if (!on) for (SeamPt &s : m_seam) if (m_seamInput[s.seam].pieceA != m_seamInput[s.seam].pieceB) s.sewn = false;
    }
    // opening of a seam along its length: (distance from its start on side A in cm, gap in cm)
    std::vector<std::pair<double, double>> seamProfile(int seam) const;

    struct Stats { int steps{0}; double seconds{0}; double maxMove{0}; };
    // runs until it settles (max move per step below tol, cm) or maxSteps
    Stats settle(double gravity, int maxSteps, double tol, double seamForceN);

    // largest remaining opening of each seam (cm)
    std::vector<double> seamGaps() const;
    double borderRoughness() const;
    const std::vector<SeamInput> &seams() const { return m_seamInput; }

    V3 nodeAt(int piece, P2 pat) const { int best=0; double bd=1e18; for (size_t k=0;k<m_x.size();++k){ if(m_nodePiece[k]!=piece) continue; double d=std::hypot(m_nodePattern[k].x-pat.x,m_nodePattern[k].y-pat.y); if(d<bd){bd=d;best=int(k);} } return m_x[best]; }
    double formSdf(const V3 &p) const { V3 q=p; V3 n; bool in = m_form.project(q, 0, &n); return in ? -(q-p).len() : 0.0; }
    int nodeCount() const { return int(m_x.size()); }
    int constraintCount() const { return int(m_dist.size() + m_bend.size()); }
    void writeObj(const std::string &path) const;
    void triangles(std::vector<V3> &verts, std::vector<std::array<int, 3>> &tris, std::vector<int> &piece) const;
    // called from settle() every few steps (from the thread that runs it)
    void setProgress(std::function<void()> f) { m_progress = std::move(f); }
    // thread safe copy of the last published positions
    std::vector<V3> shownPositions() const { std::lock_guard<std::mutex> l(m_shownMutex); return m_shown; }
    const std::vector<std::array<int, 3>> &meshTriangles() const { return m_tris; }
    const std::vector<int> &meshTrianglePiece() const { return m_triPiece; }
    void requestStop() { m_stop = true; }
    // the open stretch of every seam: both sides, from the shown positions
    struct OpenSeam { int seam; std::vector<std::pair<V3, V3>> pairs; double maxGap{0}; double sewnFraction{0}; };
    std::vector<OpenSeam> openSeams() const;

private:
    enum Kind { Thread, Shear, Edge };
    struct Dist { int a, b; double rest, lo, hi, compliance; double lambda; Kind kind; double dir; };
    struct Bend { int a, b; double rest, compliance, lambda; bool warp; };
    struct SeamPt { int a0, a1; double wa; int b0, b1; double wb; double lambda; int seam; bool sewn; };
    struct Pin { int node; V3 to; };
    struct Hinge { int i[4]; double K[4]; double scale; double kk; };   // isometric bending across an edge

    void build(const PieceInput &p, int pieceIndex, double spacing, double wrapRadius);
    int  nodeFor(int piece, const P2 &uv, const P2 &pattern);

    const DressForm &m_form;
    Material m_mat;
    std::vector<V3> m_x, m_prev, m_v;
    std::vector<double> m_w;          // inverse mass
    std::vector<double> m_mass;
    std::vector<int> m_nodePiece;
    std::vector<P2> m_nodePattern;    // pattern coordinates
    std::vector<P2> m_nodeUV;         // grain frame coordinates
    std::vector<Dist> m_dist;
    std::vector<Bend> m_bend;
    std::vector<Dist> m_tether;
    std::vector<Dist> m_border;
    std::vector<Hinge> m_hinge;
    std::vector<int> m_orphan;
    bool m_latch{true};
    std::map<std::tuple<int, long long, long long>, int> m_nodeIndex;
    std::function<void()> m_progress;
    mutable std::mutex m_shownMutex;
    std::vector<V3> m_shown;
    volatile bool m_stop{false};
    void publish() { std::lock_guard<std::mutex> l(m_shownMutex); m_shown = m_x; }
    std::vector<SeamPt> m_seam;
    std::vector<Pin> m_pins;
    std::vector<std::array<int, 3>> m_tris;
    std::vector<int> m_triPiece;
    std::vector<SeamInput> m_seamInput;
    struct Boundary { std::vector<int> nodes; std::vector<double> param; std::vector<double> vertexParam; double length; };
    std::vector<Boundary> m_boundary; // per piece, nodes along the outline in order
    std::vector<std::vector<P2>> m_outlines;
};

// the bodice used to try the engine (size 42 block, full front and back, darts)
struct Demo {
    std::vector<PieceInput> pieces;
    std::vector<SeamInput> seams;
    std::vector<std::pair<int, P2>> pins;   // piece, pattern point
};
Demo demoBodice(double widthScale);

} // namespace sim
