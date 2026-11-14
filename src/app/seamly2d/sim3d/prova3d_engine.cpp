#include "prova3d_engine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <map>
#include <unordered_map>

namespace sim {

namespace {
const double kPi = 3.14159265358979323846;

double pchip(const std::vector<double> &xs, const std::vector<double> &ys, double x)
{
    const int n = int(xs.size());
    if (x <= xs.front()) return ys.front();
    if (x >= xs.back()) return ys.back();
    int k = int(std::upper_bound(xs.begin(), xs.end(), x) - xs.begin()) - 1;
    auto slope = [&](int i) { return (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]); };
    auto deriv = [&](int i) {
        if (i == 0) return slope(0);
        if (i == n - 1) return slope(n - 2);
        const double s0 = slope(i - 1), s1 = slope(i);
        if (s0 * s1 <= 0) return 0.0;
        return 2.0 / (1.0 / s0 + 1.0 / s1);
    };
    const double h = xs[k + 1] - xs[k], t = (x - xs[k]) / h;
    const double d0 = deriv(k) * h, d1 = deriv(k + 1) * h;
    const double t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * ys[k] + (t3 - 2 * t2 + t) * d0 + (-2 * t3 + 3 * t2) * ys[k + 1] + (t3 - t2) * d1;
}

double hullPerimeter(std::vector<P2> pts)
{
    std::sort(pts.begin(), pts.end(), [](const P2 &a, const P2 &b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    auto crossz = [](const P2 &o, const P2 &a, const P2 &b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    std::vector<P2> h(2 * pts.size());
    int k = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && crossz(h[k - 2], h[k - 1], pts[i]) <= 0) --k;
        h[k++] = pts[i];
    }
    for (int i = int(pts.size()) - 2, t = k + 1; i >= 0; --i) {
        while (k >= t && crossz(h[k - 2], h[k - 1], pts[i]) <= 0) --k;
        h[k++] = pts[i];
    }
    h.resize(k - 1);
    double p = 0;
    for (size_t i = 0; i < h.size(); ++i) {
        const P2 &a = h[i], &b = h[(i + 1) % h.size()];
        p += std::hypot(b.x - a.x, b.y - a.y);
    }
    return p;
}

double area(const std::vector<P2> &poly)
{
    double a = 0;
    for (size_t i = 0; i < poly.size(); ++i) {
        const P2 &p = poly[i], &q = poly[(i + 1) % poly.size()];
        a += p.x * q.y - q.x * p.y;
    }
    return a / 2;
}

// ear clipping, returns triangles as index triples (CCW input expected)
std::vector<std::array<int, 3>> earClip(const std::vector<P2> &poly)
{
    std::vector<int> idx(poly.size());
    for (size_t i = 0; i < poly.size(); ++i) idx[i] = int(i);
    if (area(poly) < 0) std::reverse(idx.begin(), idx.end());
    std::vector<std::array<int, 3>> tris;
    auto cr = [&](int a, int b, int c) {
        return (poly[b].x - poly[a].x) * (poly[c].y - poly[a].y) - (poly[b].y - poly[a].y) * (poly[c].x - poly[a].x);
    };
    auto inside = [&](int p, int a, int b, int c) {
        return cr(a, b, p) > 1e-12 && cr(b, c, p) > 1e-12 && cr(c, a, p) > 1e-12;
    };
    int guard = 0;
    while (idx.size() > 3 && guard++ < 100000) {
        bool clipped = false;
        const int n = int(idx.size());
        for (int i = 0; i < n; ++i) {
            const int a = idx[(i + n - 1) % n], b = idx[i], c = idx[(i + 1) % n];
            if (cr(a, b, c) <= 1e-12) continue;
            bool ear = true;
            for (int j = 0; j < n && ear; ++j) {
                const int p = idx[j];
                if (p == a || p == b || p == c) continue;
                if (inside(p, a, b, c)) ear = false;
            }
            if (!ear) continue;
            tris.push_back({a, b, c});
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) { // degenerate: drop a collinear vertex
            idx.erase(idx.begin());
        }
    }
    if (idx.size() == 3) tris.push_back({idx[0], idx[1], idx[2]});
    return tris;
}

bool pointInPolygon(const std::vector<P2> &poly, const P2 &p)
{
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y))
            && (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) in = !in;
    }
    return in;
}

std::vector<P2> clipConvex(const std::vector<P2> &subject, double x0, double y0, double x1, double y1)
{
    std::vector<P2> out = subject;
    auto clipEdge = [&](auto insideFn, auto intersectFn) {
        std::vector<P2> in = out;
        out.clear();
        if (in.empty()) return;
        P2 s = in.back();
        for (const P2 &e : in) {
            const bool ei = insideFn(e), si = insideFn(s);
            if (ei) {
                if (!si) out.push_back(intersectFn(s, e));
                out.push_back(e);
            } else if (si) {
                out.push_back(intersectFn(s, e));
            }
            s = e;
        }
    };
    auto atX = [](double X) { return [X](const P2 &a, const P2 &b) { double t = (X - a.x) / (b.x - a.x); return P2{X, a.y + t * (b.y - a.y)}; }; };
    auto atY = [](double Y) { return [Y](const P2 &a, const P2 &b) { double t = (Y - a.y) / (b.y - a.y); return P2{a.x + t * (b.x - a.x), Y}; }; };
    clipEdge([x0](const P2 &p) { return p.x >= x0; }, atX(x0));
    clipEdge([x1](const P2 &p) { return p.x <= x1; }, atX(x1));
    clipEdge([y0](const P2 &p) { return p.y >= y0; }, atY(y0));
    clipEdge([y1](const P2 &p) { return p.y <= y1; }, atY(y1));
    // drop duplicates
    std::vector<P2> clean;
    for (const P2 &p : out) {
        if (clean.empty() || std::hypot(p.x - clean.back().x, p.y - clean.back().y) > 1e-9) clean.push_back(p);
    }
    if (clean.size() > 1 && std::hypot(clean.front().x - clean.back().x, clean.front().y - clean.back().y) < 1e-9) clean.pop_back();
    return clean;
}
} // namespace

// ============================================================================ DressForm
DressForm::DressForm(std::vector<FormLevel> levels) : m_levels(std::move(levels))
{
    std::sort(m_levels.begin(), m_levels.end(), [](const FormLevel &a, const FormLevel &b) { return a.y < b.y; });
    for (FormLevel &l : m_levels) {
        if (l.circ <= 0) continue;
        for (int it = 0; it < 4; ++it) {
            Shape s{l.halfWidth, l.front, l.back, l.bust};
            std::vector<P2> pts;
            for (int i = 0; i < 360; ++i) {
                const double th = 2 * kPi * i / 360;
                const double r = rawRadius(s, th);
                pts.push_back({r * std::sin(th), r * std::cos(th)});
            }
            const double k = l.circ / hullPerimeter(pts);
            l.halfWidth *= k; l.front *= k; l.back *= k; l.bust *= k;
        }
    }
    buildSdf();
}

DressForm DressForm::standard42()
{
    // Generic dress form, Italian size 42 (bust 88, waist 70, hip 96), nape at 141 cm.
    // circ = 0: shape taken as is (shoulder region, not a tape measure).
    return DressForm({
        {74.0, 95.0, 17.5, 11.0, 12.5, 0},
        {82.0, 96.0, 17.5, 11.0, 12.5, 0},
        {92.0, 86.0, 16.0, 10.0, 11.0, 0},
        {101.0, 70.0, 13.0, 8.5, 9.0, 0},
        {112.0, 75.0, 13.5, 9.5, 9.5, 0},
        {121.0, 88.0, 14.5, 9.5, 10.0, 3.0},
        {128.0, 87.0, 15.5, 9.0, 10.0, 1.0},
        {134.0, 0, 18.0, 7.5, 8.5, 0},
        {137.0, 0, 19.0, 6.5, 7.0, 0},
        {140.0, 0, 11.0, 5.8, 6.3, 0},
        {142.5, 36.0, 6.0, 5.5, 6.0, 0},
        {152.0, 34.0, 5.5, 5.0, 5.5, 0},
    });
}

double DressForm::rawRadius(const Shape &s, double theta)
{
    const double n = 2.4;
    const double sx = std::fabs(std::sin(theta));
    const double cz = std::cos(theta);
    const double d = cz > 0 ? s.f : s.b;
    double r = 1.0 / std::pow(std::pow(sx / s.a, n) + std::pow(std::fabs(cz) / d, n), 1.0 / n);
    if (s.bust > 0 && cz > 0) {
        const double sigma = 0.32;
        for (double tb : {0.72, -0.72}) {
            double dt = std::remainder(theta - tb, 2 * kPi);
            r += s.bust * std::exp(-dt * dt / (2 * sigma * sigma));
        }
    }
    return r;
}

DressForm::Shape DressForm::shapeAt(double y) const
{
    static thread_local const DressForm *cached = nullptr;
    static thread_local std::vector<double> ys, as, fs, bs, us;
    if (cached != this) {
        ys.clear(); as.clear(); fs.clear(); bs.clear(); us.clear();
        for (const FormLevel &l : m_levels) {
            ys.push_back(l.y); as.push_back(l.halfWidth); fs.push_back(l.front); bs.push_back(l.back); us.push_back(l.bust);
        }
        cached = this;
    }
    return {pchip(ys, as, y), pchip(ys, fs, y), pchip(ys, bs, y), std::max(0.0, pchip(ys, us, y))};
}

double DressForm::radius(double theta, double y) const { return rawRadius(shapeAt(y), theta); }

namespace {
// squared Euclidean distance transform, 1D (Felzenszwalb & Huttenlocher)
void edt1d(const std::vector<double> &f, std::vector<double> &d, int n)
{
    std::vector<int> v(n);
    std::vector<double> z(n + 1);
    int k = 0;
    v[0] = 0; z[0] = -1e20; z[1] = 1e20;
    for (int q = 1; q < n; ++q) {
        double s;
        while (true) {
            s = ((f[q] + double(q) * q) - (f[v[k]] + double(v[k]) * v[k])) / (2.0 * q - 2.0 * v[k]);
            if (s <= z[k] && k > 0) { --k; continue; }
            break;
        }
        if (s <= z[k]) { v[0] = q; z[0] = -1e20; z[1] = 1e20; k = 0; continue; }
        ++k; v[k] = q; z[k] = s; z[k + 1] = 1e20;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < q) ++k;
        d[q] = double(q - v[k]) * (q - v[k]) + f[v[k]];
    }
}

std::vector<double> edt3d(const std::vector<char> &target, int nx, int ny, int nz)
{
    const double INF = 1e20;
    std::vector<double> g(size_t(nx) * ny * nz);
    for (size_t i = 0; i < g.size(); ++i) g[i] = target[i] ? 0 : INF;
    auto at = [&](int x, int y, int z) -> double & { return g[(size_t(z) * ny + y) * nx + x]; };
    std::vector<double> f, d;
    f.resize(std::max({nx, ny, nz})); d.resize(f.size());
    for (int z = 0; z < nz; ++z) for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx; ++x) f[x] = at(x, y, z);
        edt1d(f, d, nx);
        for (int x = 0; x < nx; ++x) at(x, y, z) = d[x];
    }
    for (int z = 0; z < nz; ++z) for (int x = 0; x < nx; ++x) {
        for (int y = 0; y < ny; ++y) f[y] = at(x, y, z);
        edt1d(f, d, ny);
        for (int y = 0; y < ny; ++y) at(x, y, z) = d[y];
    }
    for (int y = 0; y < ny; ++y) for (int x = 0; x < nx; ++x) {
        for (int z = 0; z < nz; ++z) f[z] = at(x, y, z);
        edt1d(f, d, nz);
        for (int z = 0; z < nz; ++z) at(x, y, z) = d[z];
    }
    return g;
}
} // namespace

void DressForm::buildSdf()
{
    double maxR = 0;
    for (const FormLevel &l : m_levels) maxR = std::max({maxR, l.halfWidth, l.front + l.bust, l.back});
    const double pad = 4;
    m_origin = V3(-maxR - pad, bottom() - pad, -maxR - pad);
    m_nx = int((2 * (maxR + pad)) / m_cell) + 1;
    m_ny = int((top() - bottom() + 2 * pad) / m_cell) + 1;
    m_nz = m_nx;
    std::vector<char> inside(size_t(m_nx) * m_ny * m_nz), outside(inside.size());
    for (int z = 0; z < m_nz; ++z) for (int y = 0; y < m_ny; ++y) for (int x = 0; x < m_nx; ++x) {
        const V3 p = m_origin + V3(x, y, z) * m_cell;
        bool in = false;
        if (p.y >= bottom() && p.y <= top()) in = std::hypot(p.x, p.z) < radius(std::atan2(p.x, p.z), p.y);
        const size_t i = (size_t(z) * m_ny + y) * m_nx + x;
        inside[i] = in; outside[i] = !in;
    }
    const std::vector<double> dIn = edt3d(inside, m_nx, m_ny, m_nz);    // distance to the form
    const std::vector<double> dOut = edt3d(outside, m_nx, m_ny, m_nz);  // distance to the outside
    m_sdf.resize(inside.size());
    for (size_t i = 0; i < m_sdf.size(); ++i) {
        m_sdf[i] = float(inside[i] ? -(std::sqrt(dOut[i]) - 0.5) * m_cell : (std::sqrt(dIn[i]) - 0.5) * m_cell);
    }
}

double DressForm::sdf(const V3 &p) const
{
    const double fx = (p.x - m_origin.x) / m_cell, fy = (p.y - m_origin.y) / m_cell, fz = (p.z - m_origin.z) / m_cell;
    if (fx < 0 || fy < 0 || fz < 0 || fx >= m_nx - 1 || fy >= m_ny - 1 || fz >= m_nz - 1) return 1e9;
    const int x = int(fx), y = int(fy), z = int(fz);
    const double tx = fx - x, ty = fy - y, tz = fz - z;
    auto at = [&](int i, int j, int k) { return double(m_sdf[(size_t(k) * m_ny + j) * m_nx + i]); };
    const double c00 = at(x, y, z) * (1 - tx) + at(x + 1, y, z) * tx;
    const double c10 = at(x, y + 1, z) * (1 - tx) + at(x + 1, y + 1, z) * tx;
    const double c01 = at(x, y, z + 1) * (1 - tx) + at(x + 1, y, z + 1) * tx;
    const double c11 = at(x, y + 1, z + 1) * (1 - tx) + at(x + 1, y + 1, z + 1) * tx;
    return (c00 * (1 - ty) + c10 * ty) * (1 - tz) + (c01 * (1 - ty) + c11 * ty) * tz;
}

bool DressForm::project(V3 &p, double thickness, V3 *normal) const
{
    double d = sdf(p);
    if (d >= thickness) return false;
    V3 n;
    for (int it = 0; it < 3 && d < thickness; ++it) {
        const double h = m_cell * 0.5;
        n = V3(sdf(p + V3(h, 0, 0)) - sdf(p - V3(h, 0, 0)), sdf(p + V3(0, h, 0)) - sdf(p - V3(0, h, 0)),
               sdf(p + V3(0, 0, h)) - sdf(p - V3(0, 0, h)));
        const double l = n.len();
        if (l < 1e-9) break;
        n = n * (1.0 / l);
        p += n * (thickness - d);
        d = sdf(p);
    }
    if (normal) *normal = n;
    return true;
}

void DressForm::mesh(std::vector<V3> &verts, std::vector<std::array<int, 3>> &tris, int around) const
{
    const int rows = 120;
    const int base = int(verts.size());
    for (int r = 0; r <= rows; ++r) {
        const double y = bottom() + (top() - bottom()) * r / rows;
        for (int i = 0; i < around; ++i) {
            const double th = 2 * kPi * i / around;
            const double rad = radius(th, y);
            verts.push_back({rad * std::sin(th), y, rad * std::cos(th)});
        }
    }
    for (int r = 0; r < rows; ++r)
        for (int i = 0; i < around; ++i) {
            const int a = base + r * around + i, b = base + r * around + (i + 1) % around;
            const int c = a + around, d = b + around;
            tris.push_back({a, b, d});
            tris.push_back({a, d, c});
        }
}

// ============================================================================ Garment
Garment::Garment(const std::vector<PieceInput> &pieces, const std::vector<SeamInput> &seams,
                 const DressForm &form, double spacing, double wrapRadius)
    : m_form(form), m_seamInput(seams)
{
    m_boundary.resize(pieces.size());
    m_outlines.resize(pieces.size());
    for (size_t i = 0; i < pieces.size(); ++i) build(pieces[i], int(i), spacing, wrapRadius);

    // masses
    m_mass.assign(m_x.size(), 0);
    for (size_t t = 0; t < m_tris.size(); ++t) {
        const auto &tr = m_tris[t];
        const P2 &a = m_nodePattern[tr[0]], &b = m_nodePattern[tr[1]], &c = m_nodePattern[tr[2]];
        const double ar = std::fabs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) / 2; // cm^2
        for (int k : tr) m_mass[k] += ar / 3;
    }
    {
        double avg = 0;
        for (double m : m_mass) avg += m;
        avg /= std::max<size_t>(1, m_mass.size());
        for (double &m : m_mass) m = std::max(m, 0.25 * avg); // tiny corner nodes would get huge corrections
        for (int k : m_orphan) m_mass[k] = 0;
    }
    m_v.assign(m_x.size(), V3());
    m_prev = m_x;

    // seams: sample points along both edges
    for (size_t s = 0; s < seams.size(); ++s) {
        const SeamInput &si = seams[s];
        const Boundary &A = m_boundary[si.pieceA], &B = m_boundary[si.pieceB];
        auto span = [](const Boundary &bd, int from, int to, bool backward) {
            double a = bd.vertexParam[from], b = bd.vertexParam[to];
            double len = backward ? a - b : b - a;
            if (len < 0) len += bd.length;
            return std::make_pair(a, len);
        };
        auto [a0, lenA] = span(A, si.fromA, si.toA, false);
        auto [b0, lenB] = span(B, si.fromB, si.toB, si.reverseB);
        const int n = std::max(2, int(std::ceil(std::max(lenA, lenB) / (spacing * 0.5))));
        auto locate = [](const Boundary &bd, double p, int &n0, int &n1, double &w) {
            p = std::fmod(p, bd.length);
            if (p < 0) p += bd.length;
            const int m = int(bd.nodes.size());
            int k = int(std::upper_bound(bd.param.begin(), bd.param.end(), p) - bd.param.begin()) - 1;
            if (k < 0) k = m - 1;
            const int k1 = (k + 1) % m;
            double p0 = bd.param[k], p1 = bd.param[k1];
            if (p1 <= p0) p1 += bd.length;
            if (p < p0) p += bd.length;
            w = (p1 - p0) > 1e-12 ? (p - p0) / (p1 - p0) : 0;
            n0 = bd.nodes[k]; n1 = bd.nodes[k1];
        };
        for (int k = 0; k <= n; ++k) {
            const double t = double(k) / n;
            SeamPt sp{};
            locate(A, a0 + t * lenA, sp.a0, sp.a1, sp.wa);
            locate(B, si.reverseB ? b0 - t * lenB : b0 + t * lenB, sp.b0, sp.b1, sp.wb);
            sp.seam = int(s);
            m_seam.push_back(sp);
        }
    }
    setMaterial(Material::paperDefault());
    publish();
    if (std::getenv("SIMDBG")) {
        int over = 0; double worst = 0; int wa = 0, wb = 0;
        for (const Dist &d : m_dist) { double e = std::fabs((m_x[d.b] - m_x[d.a]).len() / d.rest - 1); if (e > 0.05) ++over; if (e > worst) { worst = e; wa = d.a; wb = d.b; } }
        std::fprintf(stderr, "initial: %d edges off by >5%%, worst %.3f between pattern (%.2f,%.2f) and (%.2f,%.2f) rest %.3f\n", over, worst, m_nodePattern[wa].x, m_nodePattern[wa].y, m_nodePattern[wb].x, m_nodePattern[wb].y, 0.0);
    }
}

int Garment::nodeFor(int piece, const P2 &uv, const P2 &pattern)
{
    // nodes are unique per piece and position
    const auto key = std::make_tuple(piece, std::llround(uv.x * 1e6), std::llround(uv.y * 1e6));
    auto it = m_nodeIndex.find(key);
    if (it != m_nodeIndex.end()) return it->second;
    const int id = int(m_x.size());
    m_nodeIndex.emplace(key, id);
    m_x.push_back(V3());
    m_nodePiece.push_back(piece);
    m_nodePattern.push_back(pattern);
    m_nodeUV.push_back(uv);
    return id;
}

void Garment::build(const PieceInput &p, int pieceIndex, double h, double R)
{
    m_outlines[pieceIndex] = p.outline;
    const double g = p.grainDeg * kPi / 180;
    const P2 gw{std::cos(g), std::sin(g)};   // warp direction
    const P2 gf{-gw.y, gw.x};                // weft direction
    auto toUV = [&](const P2 &q) { return P2{q.x * gf.x + q.y * gf.y, q.x * gw.x + q.y * gw.y}; };
    auto toPattern = [&](const P2 &uv) { return P2{uv.x * gf.x + uv.y * gw.x, uv.x * gf.y + uv.y * gw.y}; };

    std::vector<P2> uvOutline;
    for (const P2 &q : p.outline) uvOutline.push_back(toUV(q));

    const int first = int(m_x.size());
    // emits the triangles of one region of a cell (snapping to grid crossings and outline corners)
    auto emit = [&](const std::vector<P2> &region) {
        if (region.size() < 3 || std::fabs(area(region)) < 1e-6) return;
        std::vector<int> ids;
        for (P2 q : region) {
            bool snapped = false;
            for (const P2 &v : uvOutline) {
                if (std::hypot(q.x - v.x, q.y - v.y) < 0.25 * h) { q = v; snapped = true; break; }
            }
            if (!snapped) {
                const P2 gc{std::round(q.x / h) * h, std::round(q.y / h) * h};
                if (std::hypot(q.x - gc.x, q.y - gc.y) < 0.25 * h && pointInPolygon(uvOutline, gc)) q = gc;
            }
            const int id = nodeFor(pieceIndex, q, toPattern(q));
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        }
        if (ids.size() < 3) return;
        std::vector<P2> poly;
        for (int id : ids) poly.push_back(m_nodeUV[id]);
        for (const auto &t : earClip(poly)) {
            std::array<int, 3> tr{ids[t[0]], ids[t[1]], ids[t[2]]};
            const P2 &pa = m_nodeUV[tr[0]], &pb = m_nodeUV[tr[1]], &pc = m_nodeUV[tr[2]];
            const double cr = (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x);
            if (std::fabs(cr) < 1e-4 * h * h) continue;
            if (cr < 0) std::swap(tr[1], tr[2]);
            m_tris.push_back(tr);
            m_triPiece.push_back(pieceIndex);
        }
    };
    auto segCross = [](const P2 &p1, const P2 &p2, const P2 &q1, const P2 &q2) {
        auto orient = [](const P2 &a, const P2 &b, const P2 &c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); };
        const double d1 = orient(q1, q2, p1), d2 = orient(q1, q2, p2), d3 = orient(p1, p2, q1), d4 = orient(p1, p2, q2);
        return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
    };
    const auto ears = earClip(uvOutline);
    double bx0 = 1e18, by0 = 1e18, bx1 = -1e18, by1 = -1e18;
    for (const P2 &q : uvOutline) { bx0 = std::min(bx0, q.x); bx1 = std::max(bx1, q.x); by0 = std::min(by0, q.y); by1 = std::max(by1, q.y); }
    const int nv0 = int(uvOutline.size());
    for (int i = int(std::floor(bx0 / h)); i * h < bx1; ++i)
        for (int j = int(std::floor(by0 / h)); j * h < by1; ++j) {
            const double x0 = i * h, y0 = j * h, x1 = x0 + h, y1 = y0 + h;
            const P2 sq[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
            int crossings = 0, inside = 0;
            for (int e = 0; e < nv0; ++e) {
                const P2 &a = uvOutline[e], &b = uvOutline[(e + 1) % nv0];
                if (std::max(a.x, b.x) < x0 || std::min(a.x, b.x) > x1 || std::max(a.y, b.y) < y0 || std::min(a.y, b.y) > y1) continue;
                for (int k = 0; k < 4; ++k) if (segCross(a, b, sq[k], sq[(k + 1) % 4])) ++crossings;
                if (a.x > x0 && a.x < x1 && a.y > y0 && a.y < y1) ++inside;
            }
            if (crossings == 0 && inside == 0) {
                if (pointInPolygon(uvOutline, P2{x0 + h / 2, y0 + h / 2})) emit({sq[0], sq[1], sq[2], sq[3]});   // a whole cell
                continue;
            }
            if (crossings <= 2) {
                emit(clipConvex(uvOutline, x0, y0, x1, y1));   // one piece of outline through the cell
                continue;
            }
            // tricky cells (dart tips, narrow parts): clip the triangles of the outline, only here
            for (const auto &t : ears) {
                const std::vector<P2> tri{uvOutline[t[0]], uvOutline[t[1]], uvOutline[t[2]]};
                emit(clipConvex(tri, x0, y0, x1, y1));
            }
        }
    const int last = int(m_x.size());

    // initial placement: wrapped on a cylinder
    for (int k = first; k < last; ++k) {
        const P2 &q = m_nodePattern[k];
        const double th = p.angleDeg * kPi / 180 + (p.mirrored ? -1 : 1) * (q.x - p.centerX) / R;
        m_x[k] = V3(R * std::sin(th), p.height - (q.y - p.topY), R * std::cos(th));
    }

    // nodes that ended up in no triangle (merged corners) are left out
    std::vector<char> used(last - first, 0);
    for (size_t t = 0; t < m_tris.size(); ++t)
        if (m_triPiece[t] == pieceIndex)
            for (int k : m_tris[t]) used[k - first] = 1;
    for (int k = first; k < last; ++k) if (!used[k - first]) m_orphan.push_back(k);

    // boundary: nodes lying on the outline, in order
    Boundary &bd = m_boundary[pieceIndex];
    double acc = 0;
    std::vector<std::pair<double, int>> onEdge;
    const int nv = int(uvOutline.size());
    for (int e = 0; e < nv; ++e) {
        const P2 a = uvOutline[e], b = uvOutline[(e + 1) % nv];
        const double len = std::hypot(b.x - a.x, b.y - a.y);
        bd.vertexParam.push_back(acc);
        for (int k = first; k < last; ++k) {
            if (!used[k - first]) continue;
            const P2 &q = m_nodeUV[k];
            const double t = ((q.x - a.x) * (b.x - a.x) + (q.y - a.y) * (b.y - a.y)) / (len * len);
            if (t < -1e-9 || t > 1 + 1e-9) continue;
            const double dx = a.x + t * (b.x - a.x) - q.x, dy = a.y + t * (b.y - a.y) - q.y;
            if (std::hypot(dx, dy) > 1e-5) continue;
            if (t > 1 - 1e-9) continue; // belongs to the next edge's start
            onEdge.push_back({acc + t * len, k});
        }
        acc += len;
    }
    bd.length = acc;
    std::sort(onEdge.begin(), onEdge.end());
    for (const auto &pe : onEdge) {
        if (!bd.nodes.empty() && bd.nodes.back() == pe.second) continue;
        bd.param.push_back(pe.first);
        bd.nodes.push_back(pe.second);
    }

    // the border: a ribbon along the outline that closes the net (chords over 1, 2 and 3 border nodes)
    {
        const int m = int(bd.nodes.size());
        for (int i = 0; i < m; ++i)
            for (int s = 1; s <= 3; ++s) {
                const int a = bd.nodes[i], b = bd.nodes[(i + s) % m];
                if (a == b) continue;
                Dist d{};
                d.a = a; d.b = b; d.kind = Edge;
                const double du = m_nodeUV[b].x - m_nodeUV[a].x, dv = m_nodeUV[b].y - m_nodeUV[a].y;
                d.rest = std::hypot(du, dv);
                if (d.rest < 1e-4) continue;
                d.dir = std::fabs(std::sin(2 * std::atan2(dv, du)));   // 0 on grain, 1 on the bias
                m_border.push_back(d);
            }
    }

    // edges
    std::map<std::pair<int, int>, bool> seen;
    for (size_t t = 0; t < m_tris.size(); ++t) {
        if (m_triPiece[t] != pieceIndex) continue;
        for (int e = 0; e < 3; ++e) {
            int a = m_tris[t][e], b = m_tris[t][(e + 1) % 3];
            if (a > b) std::swap(a, b);
            if (seen.count({a, b})) continue;
            seen[{a, b}] = true;
            const P2 &ua = m_nodeUV[a], &ub = m_nodeUV[b];
            const double du = ub.x - ua.x, dv = ub.y - ua.y;
            Dist d{};
            d.a = a; d.b = b;
            d.rest = std::hypot(du, dv);
            if (d.rest < 1e-6) continue;
            if (std::fabs(du) < 1e-7) { d.kind = Thread; d.dir = 1; }       // warp
            else if (std::fabs(dv) < 1e-7) { d.kind = Thread; d.dir = 0; }  // weft
            else { d.kind = Shear; d.dir = std::fabs(std::sin(2 * std::atan2(dv, du))); }
            m_dist.push_back(d);
        }
    }

    // bending along the threads: consecutive nodes on the same grid line
    std::map<long long, std::vector<std::pair<double, int>>> warpLines, weftLines;
    for (int k = first; k < last; ++k) {
        const P2 &q = m_nodeUV[k];
        const double fu = q.x / h, fv = q.y / h;
        if (std::fabs(fu - std::round(fu)) < 1e-6) warpLines[std::llround(fu)].push_back({q.y, k});
        if (std::fabs(fv - std::round(fv)) < 1e-6) weftLines[std::llround(fv)].push_back({q.x, k});
    }
    auto addBends = [&](std::map<long long, std::vector<std::pair<double, int>>> &lines, bool warp) {
        for (auto &kv : lines) {
            auto &v = kv.second;
            std::sort(v.begin(), v.end());
            for (size_t i = 1; i + 1 < v.size(); ++i) {
                const int a = v[i - 1].second, b = v[i].second, c = v[i + 1].second;
                auto connected = [&](int x, int y) { return seen.count({std::min(x, y), std::max(x, y)}) > 0; };
                if (!connected(a, b) || !connected(b, c)) continue;
                Bend bd2{};
                bd2.a = a; bd2.b = c; bd2.warp = warp;
                bd2.rest = std::fabs(v[i + 1].first - v[i - 1].first);
                m_bend.push_back(bd2);
            }
        }
    };
    addBends(warpLines, true);
    addBends(weftLines, false);

    // bending across every inner edge (isometric bending, Bergou et al.): zero when the cloth is flat
    {
        std::map<std::pair<int, int>, std::vector<int>> edgeTris;
        for (size_t t = 0; t < m_tris.size(); ++t) {
            if (m_triPiece[t] != pieceIndex) continue;
            for (int e = 0; e < 3; ++e) {
                int a = m_tris[t][e], b = m_tris[t][(e + 1) % 3];
                edgeTris[{std::min(a, b), std::max(a, b)}].push_back(int(t));
            }
        }
        auto cot = [](const P2 &a, const P2 &b) {
            const double cr = std::fabs(a.x * b.y - a.y * b.x);
            const double c = (a.x * b.x + a.y * b.y) / std::max(cr, 1e-9);
            return std::max(-8.0, std::min(8.0, c));
        };
        auto triArea = [&](int t) {
            const P2 &a = m_nodeUV[m_tris[t][0]], &b = m_nodeUV[m_tris[t][1]], &c = m_nodeUV[m_tris[t][2]];
            return std::fabs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) / 2;
        };
        for (const auto &kv : edgeTris) {
            if (kv.second.size() != 2) continue;
            const int x0 = kv.first.first, x1 = kv.first.second;
            auto opposite = [&](int t) { for (int k : m_tris[t]) if (k != x0 && k != x1) return k; return -1; };
            const int x2 = opposite(kv.second[0]), x3 = opposite(kv.second[1]);
            if (x2 < 0 || x3 < 0) continue;
            const double A = triArea(kv.second[0]) + triArea(kv.second[1]);
            if (A < 1e-3 * h * h) continue;
            auto d = [&](int a, int b) { return P2{m_nodeUV[b].x - m_nodeUV[a].x, m_nodeUV[b].y - m_nodeUV[a].y}; };
            const P2 e0 = d(x0, x1), e1 = d(x0, x2), e2 = d(x0, x3), e3 = d(x1, x2), e4 = d(x1, x3);
            const P2 me0{-e0.x, -e0.y};
            const double c01 = cot(e0, e1), c02 = cot(e0, e2), c03 = cot(me0, e3), c04 = cot(me0, e4);
            Hinge hg{};
            hg.i[0] = x0; hg.i[1] = x1; hg.i[2] = x2; hg.i[3] = x3;
            hg.K[0] = c03 + c04; hg.K[1] = c01 + c02; hg.K[2] = -c01 - c03; hg.K[3] = -c02 - c04;
            hg.scale = 3.0 / A;
            hg.kk = 0;
            for (double k : hg.K) hg.kk += k * k;
            m_hinge.push_back(hg);
        }
    }

    // long-range tethers: grid crossings far apart can't get further than the cloth allows
    std::map<std::pair<long long, long long>, int> grid;
    for (int k = first; k < last; ++k) {
        const P2 &q = m_nodeUV[k];
        const double fu = q.x / h, fv = q.y / h;
        if (std::fabs(fu - std::round(fu)) < 1e-6 && std::fabs(fv - std::round(fv)) < 1e-6) grid[{std::llround(fu), std::llround(fv)}] = k;
    }
    auto insideSegment = [&](const P2 &a, const P2 &b) {
        const double len = std::hypot(b.x - a.x, b.y - a.y);
        const int n = std::max(2, int(len / (0.5 * h)));
        for (int i = 1; i < n; ++i) {
            const P2 q{a.x + (b.x - a.x) * i / n, a.y + (b.y - a.y) * i / n};
            if (!pointInPolygon(uvOutline, q)) return false;
        }
        return true;
    };
    const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (const auto &kv : grid) {
        for (int s : {2, 4, 8, 16, 32}) {
            for (const auto &d : dirs) {
                auto it = grid.find({kv.first.first + d[0] * s, kv.first.second + d[1] * s});
                if (it == grid.end()) continue;
                const int a = kv.second, b = it->second;
                if (!insideSegment(m_nodeUV[a], m_nodeUV[b])) continue;
                Dist t{};
                t.a = a; t.b = b; t.kind = Edge;
                t.rest = std::hypot(m_nodeUV[b].x - m_nodeUV[a].x, m_nodeUV[b].y - m_nodeUV[a].y);
                t.dir = (d[0] != 0 && d[1] != 0) ? 1.0 : (d[1] != 0 ? -1.0 : -2.0); // 1 diagonal, -1 warp, -2 weft
                m_tether.push_back(t);
            }
        }
    }
}

void Garment::setMaterial(const Material &m)
{
    m_mat = m;
    const double sinLock = std::sin(m.lockAngleDeg * kPi / 180);
    for (Dist &d : m_dist) {
        if (m.paper) {
            d.lo = d.hi = d.rest;
            d.compliance = 0;
            continue;
        }
        if (d.kind == Thread) {
            const double eps = d.dir > 0.5 ? m.stretchWarp : m.stretchWeft;
            d.lo = d.rest * 0.97;
            d.hi = d.rest * (1 + eps);
            d.compliance = 0;
        } else {
            const double eps = std::max(m.stretchWarp, m.stretchWeft);
            d.lo = d.rest * std::sqrt(std::max(0.0, 1 - d.dir * sinLock));
            d.hi = d.rest * std::sqrt(1 + d.dir * sinLock) * (1 + eps);
            d.compliance = m.shearCompliance;
        }
    }
    for (Bend &b : m_bend) b.compliance = m.bendCompliance;
    for (Dist &d : m_border) {
        if (m.paper || m.stayStitch) { d.lo = d.hi = d.rest; d.compliance = 0; continue; }
        // fabric: the cut edge follows the cloth (a bias edge gives), only a light pull to keep it smooth
        d.lo = 0;
        d.hi = 1e9;
        d.compliance = m.shearCompliance * 10;
    }
    for (Dist &t : m_tether) {
        double k = 1;
        if (!m.paper) {
            if (t.dir > 0) k = std::sqrt(1 + sinLock) * (1 + std::max(m.stretchWarp, m.stretchWeft));
            else k = 1 + (t.dir > -1.5 ? m.stretchWarp : m.stretchWeft);
        }
        t.hi = t.rest * k;
    }
    for (size_t k = 0; k < m_x.size(); ++k) {
        const double kg = m_mass[k] * 1e-4 * m.density; // cm^2 -> m^2
        m_w.resize(m_x.size());
        m_w[k] = kg > 0 ? 1.0 / kg : 0;
        (void)0;
    }
}

void Garment::setPins(const std::vector<PinInput> &pins)
{
    m_pins.clear();
    for (const PinInput &pi : pins) {
        int best = -1;
        double bd = 1e18;
        for (size_t k = 0; k < m_x.size(); ++k) {
            if (m_nodePiece[k] != pi.piece) continue;
            const double d = std::hypot(m_nodePattern[k].x - pi.at.x, m_nodePattern[k].y - pi.at.y);
            if (d < bd) { bd = d; best = int(k); }
        }
        if (best >= 0) m_pins.push_back({best, pi.to});
    }
}

Garment::Stats Garment::settle(double gravity, int maxSteps, double tol, double seamForceN)
{
    const auto t0 = std::chrono::steady_clock::now();
    const double dt = std::getenv("SIMDT") ? std::atof(std::getenv("SIMDT")) : 1.0 / 120;
    const double dt2 = dt * dt;
    const double maxLambda = seamForceN * 100 * dt2;   // N -> kg cm / s^2, times dt^2
    (void)maxLambda;
    const double sewSpeed = std::getenv("SEWSPD") ? std::atof(std::getenv("SEWSPD")) : 0.3;                    // cm per step a seam may close
    const int iterations = std::getenv("SIMIT") ? std::atoi(std::getenv("SIMIT")) : 6;
    const double momentum = std::getenv("SIMMOM") ? std::atof(std::getenv("SIMMOM")) : 0.5;
    Stats st;
    std::vector<double> w = m_w;
    for (const Pin &p : m_pins) w[p.node] = 0;
    const V3 g(0, -gravity, 0);
    std::vector<double> seamTarget(m_seam.size());
    std::vector<char> touching(m_x.size());
    std::vector<double> pushed(m_x.size());
    int quiet = 0;

    auto seamPoints = [&](const SeamPt &s, V3 &pa, V3 &pb) {
        pa = m_x[s.a0] * (1 - s.wa) + m_x[s.a1] * s.wa;
        pb = m_x[s.b0] * (1 - s.wb) + m_x[s.b1] * s.wb;
    };

    for (int step = 0; step < maxSteps; ++step) {
        for (size_t k = 0; k < m_x.size(); ++k) {
            m_prev[k] = m_x[k];
            if (w[k] == 0) continue;
            m_v[k] += g * dt;
            m_x[k] += m_v[k] * dt;
        }
        for (const Pin &p : m_pins) m_x[p.node] = p.to;
        for (size_t i = 0; i < m_seam.size(); ++i) {
            V3 pa, pb;
            seamPoints(m_seam[i], pa, pb);
            const double gap = (pb - pa).len();
            const bool dart = m_seamInput[m_seam[i].seam].pieceA == m_seamInput[m_seam[i].seam].pieceB;
            // sewn only when the edges really touch (zero ease); darts are closed first and always hold
            if (gap < 0.05 && (m_latch || dart)) m_seam[i].sewn = true;
            if (!m_seam[i].sewn && m_latch && gap < 3.0) {
                // a single point left out between sewn ones (or at the end of a sewn stretch) is the mesh, not the fit
                const bool hasPrev = i > 0 && m_seam[i - 1].seam == m_seam[i].seam;
                const bool hasNext = i + 1 < m_seam.size() && m_seam[i + 1].seam == m_seam[i].seam;
                const bool prevOk = !hasPrev || m_seam[i - 1].sewn;
                const bool nextOk = !hasNext || m_seam[i + 1].sewn;
                if (prevOk && nextOk && (hasPrev || hasNext)) m_seam[i].sewn = true;
            }
            seamTarget[i] = m_seam[i].sewn ? 0.0 : std::max(0.0, gap - sewSpeed);
            m_seam[i].lambda = 0;
        }
        std::fill(touching.begin(), touching.end(), 0);
        std::fill(pushed.begin(), pushed.end(), 0.0);

        for (int it = 0; it < iterations; ++it) {
            // seams: pulled closed, but never with more than the maximum force
            for (size_t i = 0; i < m_seam.size(); ++i) {
                SeamPt &s = m_seam[i];
                if (s.sewn) continue;
                V3 pa, pb;
                seamPoints(s, pa, pb);
                V3 n = pb - pa;
                const double len = n.len();
                const double C = len - seamTarget[i];
                if (C <= 0 || len < 1e-9) continue;
                n = n * (1.0 / len);
                const double wa0 = 1 - s.wa, wa1 = s.wa, wb0 = 1 - s.wb, wb1 = s.wb;
                const double ws = wa0 * wa0 * w[s.a0] + wa1 * wa1 * w[s.a1] + wb0 * wb0 * w[s.b0] + wb1 * wb1 * w[s.b1];
                if (ws == 0) continue;
                double dl = -C / ws;
                if (!s.sewn && s.lambda + dl < -maxLambda) dl = -maxLambda - s.lambda;
                if (dl >= 0) continue;
                s.lambda += dl;
                m_x[s.a0] -= n * (dl * wa0 * w[s.a0]);
                m_x[s.a1] -= n * (dl * wa1 * w[s.a1]);
                m_x[s.b0] += n * (dl * wb0 * w[s.b0]);
                m_x[s.b1] += n * (dl * wb1 * w[s.b1]);
            }
            // threads and shear band
            for (Dist &d : m_dist) {
                const double wa = w[d.a], wb = w[d.b], ws = wa + wb;
                if (ws == 0) continue;
                V3 n = m_x[d.b] - m_x[d.a];
                const double len = n.len();
                if (len < 1e-12) continue;
                n = n * (1.0 / len);
                double C, alpha;
                if (len < d.lo) { C = len - d.lo; alpha = 0; }
                else if (len > d.hi) { C = len - d.hi; alpha = 0; }
                else if (d.compliance > 0) { C = len - d.rest; alpha = d.compliance / dt2; }
                else continue;
                const double dl = -C / (ws + alpha);
                m_x[d.a] -= n * (dl * wa);
                m_x[d.b] += n * (dl * wb);
            }
            // the border ribbon
            static const bool noBorder = std::getenv("NOBORDER") != nullptr;
            for (Dist &d : m_border) {
                if (noBorder) break;
                const double wa = w[d.a], wb = w[d.b], ws = wa + wb;
                if (ws == 0) continue;
                V3 n = m_x[d.b] - m_x[d.a];
                const double len = n.len();
                if (len < 1e-12) continue;
                n = n * (1.0 / len);
                double C, alpha;
                if (len < d.lo) { C = len - d.lo; alpha = 0; }
                else if (len > d.hi) { C = len - d.hi; alpha = 0; }
                else if (d.compliance > 0) { C = len - d.rest; alpha = d.compliance / dt2; }
                else continue;
                const double dl = -C / (ws + alpha);
                m_x[d.a] -= n * (dl * wa);
                m_x[d.b] += n * (dl * wb);
            }
            // long-range limits
            for (const Dist &t : m_tether) {
                const double ws = w[t.a] + w[t.b];
                if (ws == 0) continue;
                V3 n = m_x[t.b] - m_x[t.a];
                const double len = n.len();
                if (len <= t.hi) continue;
                n = n * (1.0 / len);
                const double dl = -(len - t.hi) / ws;
                m_x[t.a] -= n * (dl * w[t.a]);
                m_x[t.b] += n * (dl * w[t.b]);
            }
            // bending: every inner edge wants the cloth flat
            {
                const double alphaBase = m_mat.bendCompliance / dt2;
                for (const Hinge &hg : m_hinge) {
                    V3 C;
                    double den = 0;
                    for (int q = 0; q < 4; ++q) { C += m_x[hg.i[q]] * hg.K[q]; den += w[hg.i[q]] * hg.K[q] * hg.K[q]; }
                    if (den == 0) continue;
                    const double alpha = alphaBase / hg.scale;
                    const double f = -1.0 / (den + alpha);
                    for (int q = 0; q < 4; ++q) m_x[hg.i[q]] += C * (f * w[hg.i[q]] * hg.K[q]);
                }
            }
            // the form
            static const bool noCol = std::getenv("SIMNOCOL") != nullptr;
            for (size_t k = 0; k < m_x.size() && !noCol; ++k) {
                if (w[k] == 0) continue;
                const V3 before = m_x[k];
                if (m_form.project(m_x[k], m_mat.thickness, nullptr)) {
                    touching[k] = 1;
                    pushed[k] += (m_x[k] - before).len();
                    if (std::getenv("SIMDBG") && (m_x[k] - before).len() > 2.0)
                        std::fprintf(stderr, "  big push %.2f: (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f)\n", (m_x[k] - before).len(), before.x, before.y, before.z, m_x[k].x, m_x[k].y, m_x[k].z);
                }
            }
            // sewn seams: both sides go to the same point, kept outside the form
            for (SeamPt &sp : m_seam) {
                if (!sp.sewn) continue;
                const double wa0 = 1 - sp.wa, wa1 = sp.wa, wb0 = 1 - sp.wb, wb1 = sp.wb;
                const V3 pa = m_x[sp.a0] * wa0 + m_x[sp.a1] * wa1;
                const V3 pb = m_x[sp.b0] * wb0 + m_x[sp.b1] * wb1;
                V3 target = (pa + pb) * 0.5;
                if (!noCol) m_form.project(target, m_mat.thickness, nullptr);
                auto pull = [&](int n0, int n1, double c0, double c1, const V3 &from) {
                    const V3 delta = target - from;
                    const double den = c0 * c0 * w[n0] + c1 * c1 * w[n1];
                    if (den <= 0) return;
                    m_x[n0] += delta * (c0 * w[n0] / den);
                    m_x[n1] += delta * (c1 * w[n1] / den);
                };
                pull(sp.a0, sp.a1, wa0, wa1, pa);
                pull(sp.b0, sp.b1, wb0, wb1, pb);
            }
        }
        // Coulomb friction (position based): sliding stops while it is within mu times the push of the form
        for (size_t k = 0; k < m_x.size(); ++k) {
            if (!touching[k]) continue;
            V3 q = m_x[k];
            V3 nrm;
            m_form.project(q, m_mat.thickness + 0.05, &nrm);
            if (nrm.dot(nrm) < 0.5) continue;
            const V3 dx = m_x[k] - m_prev[k];
            const V3 tangential = dx - nrm * nrm.dot(dx);
            const double tl = tangential.len();
            const double limit = m_mat.friction * std::max(pushed[k], 1e-9);
            if (tl <= limit) m_x[k] -= tangential;
            else m_x[k] -= tangential * (limit / tl);
        }

        {
            static const bool noCol = std::getenv("SIMNOCOL") != nullptr;
            // sewn seams: both sides go to the same point, kept outside the form
            for (SeamPt &sp : m_seam) {
                if (!sp.sewn) continue;
                const double wa0 = 1 - sp.wa, wa1 = sp.wa, wb0 = 1 - sp.wb, wb1 = sp.wb;
                const V3 pa = m_x[sp.a0] * wa0 + m_x[sp.a1] * wa1;
                const V3 pb = m_x[sp.b0] * wb0 + m_x[sp.b1] * wb1;
                V3 target = (pa + pb) * 0.5;
                if (!noCol) m_form.project(target, m_mat.thickness, nullptr);
                auto pull = [&](int n0, int n1, double c0, double c1, const V3 &from) {
                    const V3 delta = target - from;
                    const double den = c0 * c0 * w[n0] + c1 * c1 * w[n1];
                    if (den <= 0) return;
                    m_x[n0] += delta * (c0 * w[n0] / den);
                    m_x[n1] += delta * (c1 * w[n1] / den);
                };
                pull(sp.a0, sp.a1, wa0, wa1, pa);
                pull(sp.b0, sp.b1, wb0, wb1, pb);
            }
        }
        double maxMove = 0;
        for (size_t k = 0; k < m_x.size(); ++k) {
            const V3 dx = m_x[k] - m_prev[k];
            maxMove = std::max(maxMove, dx.len());
            m_v[k] = dx * (momentum / dt);
        }
        if (std::getenv("SIMDBG") && step % 20 == 0) {
            double strain = 0; int over = 0;
            for (const Dist &d : m_dist) if (d.kind == Thread && d.rest > 0.3) { double e = (m_x[d.b] - m_x[d.a]).len() / d.rest - 1; if (e > 0.05) ++over; strain = std::max(strain, e); }
            double my = 0; for (const V3 &x : m_x) my += x.y;
            int arg = 0; double mm = 0; for (size_t k = 0; k < m_x.size(); ++k) { double d = (m_x[k] - m_prev[k]).len(); if (d > mm) { mm = d; arg = int(k); } }
            std::fprintf(stderr, "[max node %d pat %.1f,%.1f at %.1f,%.1f,%.1f] ", arg, m_nodePattern[arg].x, m_nodePattern[arg].y, m_x[arg].x, m_x[arg].y, m_x[arg].z);
            std::fprintf(stderr, "step %d strain %.3f over5%% %d meanY %.2f move %.4f\n", step, strain, over, my / m_x.size(), maxMove);
            if (std::getenv("SIMDBG2") && step % 400 == 360) {
                std::vector<std::pair<double,int>> es;
                for (size_t i = 0; i < m_dist.size(); ++i) { const Dist &d = m_dist[i]; es.push_back({(m_x[d.b] - m_x[d.a]).len() / d.rest - 1, int(i)}); }
                std::sort(es.rbegin(), es.rend());
                for (int q = 0; q < 8; ++q) { const Dist &d = m_dist[es[q].second]; std::fprintf(stderr, "    e %.2f kind %d rest %.2f lo %.2f hi %.2f p%d (%.1f,%.1f)-(%.1f,%.1f)\n", es[q].first, d.kind, d.rest, d.lo, d.hi, m_nodePiece[d.a], m_nodePattern[d.a].x, m_nodePattern[d.a].y, m_nodePattern[d.b].x, m_nodePattern[d.b].y); }
            }
        }
        if (step % 20 == 0) { publish(); if (m_progress) m_progress(); }
        if (m_stop) { m_stop = false; break; }
        st.steps = step + 1;
        st.maxMove = maxMove;
        quiet = maxMove < tol ? quiet + 1 : 0;
        if (quiet > 30) break;
    }
    publish();
    if (m_progress) m_progress();
    st.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return st;
}

std::vector<std::pair<double, double>> Garment::seamProfile(int seam) const
{
    std::vector<std::pair<double, double>> out;
    const SeamInput &si = m_seamInput[seam];
    const Boundary &A = m_boundary[si.pieceA];
    double lenA = A.vertexParam[si.toA] - A.vertexParam[si.fromA];
    if (lenA < 0) lenA += A.length;
    std::vector<const SeamPt *> pts;
    for (const SeamPt &s : m_seam) if (s.seam == seam) pts.push_back(&s);
    for (size_t k = 0; k < pts.size(); ++k) {
        const SeamPt &s = *pts[k];
        const V3 pa = m_x[s.a0] * (1 - s.wa) + m_x[s.a1] * s.wa;
        const V3 pb = m_x[s.b0] * (1 - s.wb) + m_x[s.b1] * s.wb;
        out.push_back({lenA * k / std::max<size_t>(1, pts.size() - 1), s.sewn ? 0.0 : (pb - pa).len()});
    }
    return out;
}

std::vector<Garment::OpenSeam> Garment::openSeams() const
{
    const std::vector<V3> x = shownPositions();
    std::vector<OpenSeam> out(m_seamInput.size());
    for (size_t i = 0; i < out.size(); ++i) out[i].seam = int(i);
    if (x.size() != m_x.size()) return out;
    std::vector<int> total(out.size(), 0), sewn(out.size(), 0);
    for (const SeamPt &s : m_seam) { ++total[s.seam]; if (s.sewn) ++sewn[s.seam]; }
    for (size_t i = 0; i < out.size(); ++i) out[i].sewnFraction = total[i] ? double(sewn[i]) / total[i] : 0;
    for (const SeamPt &s : m_seam) {
        if (s.sewn) continue;
        const V3 pa = x[s.a0] * (1 - s.wa) + x[s.a1] * s.wa;
        const V3 pb = x[s.b0] * (1 - s.wb) + x[s.b1] * s.wb;
        const double gap = (pb - pa).len();
        if (gap < 0.1) continue;
        out[s.seam].pairs.push_back({pa, pb});
        out[s.seam].maxGap = std::max(out[s.seam].maxGap, gap);
    }
    return out;
}

double Garment::borderRoughness() const
{
    // mean difference (degrees) between the turning angle of the border in 3D and on the pattern
    double sum = 0; int n = 0;
    for (const Boundary &bd : m_boundary) {
        const int m = int(bd.nodes.size());
        for (int i = 0; i < m; ++i) {
            const int a = bd.nodes[(i + m - 1) % m], b = bd.nodes[i], c = bd.nodes[(i + 1) % m];
            const V3 u = m_x[b] - m_x[a], v = m_x[c] - m_x[b];
            const P2 pu{m_nodeUV[b].x - m_nodeUV[a].x, m_nodeUV[b].y - m_nodeUV[a].y}, pv{m_nodeUV[c].x - m_nodeUV[b].x, m_nodeUV[c].y - m_nodeUV[b].y};
            if (u.len() < 1e-6 || v.len() < 1e-6) continue;
            const double a3 = std::acos(std::max(-1.0, std::min(1.0, u.dot(v) / (u.len() * v.len()))));
            const double a2 = std::acos(std::max(-1.0, std::min(1.0, (pu.x * pv.x + pu.y * pv.y) / (std::hypot(pu.x, pu.y) * std::hypot(pv.x, pv.y)))));
            sum += std::fabs(a3 - a2) * 180 / kPi; ++n;
        }
    }
    return n ? sum / n : 0;
}

std::vector<double> Garment::seamGaps() const
{
    if (std::getenv("SEAMDBG")) {
        std::vector<int> nS(m_seamInput.size()), nT(m_seamInput.size()); std::vector<double> gS(m_seamInput.size());
        for (const SeamPt &s : m_seam) { nT[s.seam]++; if (s.sewn) { nS[s.seam]++; const V3 pa = m_x[s.a0] * (1 - s.wa) + m_x[s.a1] * s.wa; const V3 pb = m_x[s.b0] * (1 - s.wb) + m_x[s.b1] * s.wb; gS[s.seam] = std::max(gS[s.seam], (pb - pa).len()); } }
        { int idx = 0; for (const SeamPt &s : m_seam) { if (s.seam == 1) { const V3 pa = m_x[s.a0] * (1 - s.wa) + m_x[s.a1] * s.wa; const V3 pb = m_x[s.b0] * (1 - s.wb) + m_x[s.b1] * s.wb; std::fprintf(stderr, "%d%s:%.2f ", idx, s.sewn ? "" : "o", (pb - pa).len()); ++idx; } } std::fprintf(stderr, "\n"); }
        for (size_t i = 0; i < nS.size(); ++i) std::fprintf(stderr, "   [%s sewn %d/%d, worst sewn gap %.2f]\n", m_seamInput[i].name.c_str(), nS[i], nT[i], gS[i]);
    }
    std::vector<double> gaps(m_seamInput.size(), 0);
    for (const SeamPt &s : m_seam) {
        const V3 pa = m_x[s.a0] * (1 - s.wa) + m_x[s.a1] * s.wa;
        const V3 pb = m_x[s.b0] * (1 - s.wb) + m_x[s.b1] * s.wb;
        if (!s.sewn) gaps[s.seam] = std::max(gaps[s.seam], (pb - pa).len());
    }
    return gaps;
}

void Garment::triangles(std::vector<V3> &verts, std::vector<std::array<int, 3>> &tris, std::vector<int> &piece) const
{
    verts = m_x;
    tris = m_tris;
    piece = m_triPiece;
}

void Garment::writeObj(const std::string &path) const
{
    std::ofstream f(path);
    for (const V3 &v : m_x) f << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
    for (const auto &t : m_tris) f << "f " << t[0] + 1 << ' ' << t[1] + 1 << ' ' << t[2] + 1 << '\n';
}

} // namespace sim

// ============================================================================ demo bodice
namespace sim {

namespace {
using Half = std::vector<std::pair<std::string, P2>>;

void densify(Half &hf, const std::vector<std::string> &curve)
{
    std::vector<P2> pts;
    std::vector<size_t> at;
    for (const std::string &nm : curve)
        for (size_t i = 0; i < hf.size(); ++i)
            if (hf[i].first == nm) { pts.push_back(hf[i].second); at.push_back(i); }
    Half extra;
    const int sub = 6;
    for (size_t k = 0; k + 1 < pts.size(); ++k) {
        const P2 p0 = pts[k ? k - 1 : 0], p1 = pts[k], p2 = pts[k + 1], p3 = pts[k + 2 < pts.size() ? k + 2 : k + 1];
        for (int j = 1; j < sub; ++j) {
            const double t = double(j) / sub, t2 = t * t, t3 = t2 * t;
            auto cr = [&](double a, double b, double c, double d) {
                return 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
            };
            extra.push_back({std::string(), {cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y)}});
        }
        extra.push_back(hf[at[k + 1]]);
    }
    Half out(hf.begin(), hf.begin() + long(at.front()) + 1);
    out.insert(out.end(), extra.begin(), extra.end());
    out.insert(out.end(), hf.begin() + long(at.back()) + 1, hf.end());
    hf = out;
}

PieceInput mirrorFull(const std::string &name, const Half &h, std::map<std::string, int> &idx, double sx)
{
    PieceInput p;
    p.name = name;
    for (const auto &np : h) {
        if (!np.first.empty()) idx[np.first] = int(p.outline.size());
        p.outline.push_back({np.second.x * sx, np.second.y});
    }
    for (int i = int(h.size()) - 2; i >= 1; --i) {
        if (!h[size_t(i)].first.empty()) idx[h[size_t(i)].first + "'"] = int(p.outline.size());
        p.outline.push_back({-h[size_t(i)].second.x * sx, h[size_t(i)].second.y});
    }
    return p;
}
} // namespace

Demo demoBodice(double widthScale)
{
    Half front{{"CFN", {0, 6}}, {"fn1", {2.5, 5.8}}, {"fn2", {5.5, 3.5}}, {"SNP", {7.2, -2}}, {"ST", {18.9, 2.4}},
               {"fa1", {17.5, 9}}, {"fa2", {17.2, 14}}, {"fa3", {19, 18.5}}, {"UA", {24, 21}}, {"SW", {23, 40}},
               {"DR", {11.5, 43.5}}, {"AP", {9.5, 25}}, {"DL", {7.5, 43.5}}, {"CFW", {0, 44}}};
    Half back{{"CBN", {0, 0}}, {"bn1", {3.5, -0.3}}, {"SNP", {7, -2}}, {"ST", {18.9, 1.9}}, {"ba1", {18.3, 5}},
              {"ba2", {18, 10}}, {"ba3", {19.5, 15}}, {"UA", {23, 21}}, {"SW", {21.5, 40}}, {"DR", {11.5, 40}},
              {"AP", {10, 24}}, {"DL", {8.5, 40}}, {"CBW", {0, 40}}};
    densify(front, {"CFN", "fn1", "fn2", "SNP"});
    densify(front, {"ST", "fa1", "fa2", "fa3", "UA"});
    densify(back, {"CBN", "bn1", "SNP"});
    densify(back, {"ST", "ba1", "ba2", "ba3", "UA"});

    std::map<std::string, int> fi, bi;
    Demo d;
    PieceInput F = mirrorFull("Davanti", front, fi, widthScale);
    PieceInput B = mirrorFull("Dietro", back, bi, widthScale);
    F.angleDeg = 0; F.height = 141; F.topY = 0;
    B.angleDeg = 180; B.height = 141; B.topY = 0; B.mirrored = true;
    d.pieces = {F, B};
    d.seams = {
        {0, fi["SNP"], fi["ST"], 1, bi["SNP"], bi["ST"], false, "spalla dx"},
        {0, fi["ST'"], fi["SNP'"], 1, bi["ST'"], bi["SNP'"], false, "spalla sx"},
        {0, fi["UA"], fi["SW"], 1, bi["UA"], bi["SW"], false, "fianco dx"},
        {0, fi["SW'"], fi["UA'"], 1, bi["SW'"], bi["UA'"], false, "fianco sx"},
        {0, fi["DR"], fi["AP"], 0, fi["DL"], fi["AP"], true, "pince davanti dx"},
        {0, fi["DL'"], fi["AP'"], 0, fi["DR'"], fi["AP'"], true, "pince davanti sx"},
        {1, bi["DR"], bi["AP"], 1, bi["DL"], bi["AP"], true, "pince dietro dx"},
        {1, bi["DL'"], bi["AP'"], 1, bi["DR'"], bi["AP'"], true, "pince dietro sx"},
    };
    d.pins = {{0, P2{0, 10}}, {0, P2{0, 40}}, {1, P2{0, 4}}, {1, P2{0, 36}}};
    return d;
}

} // namespace sim
