/******************************************************************************
 **  @file   prova3d_grid.cpp
 **  @brief  Prova 3D: the tapes of the dress form and the fine numbered grid between them.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#include "prova3d_grid.h"

#include <QCoreApplication>
#include <QLocale>

#include <algorithm>
#include <cmath>

namespace prova3d
{

namespace
{
const double pi = 3.14159265358979323846;
const double gridBottom = 76.0;
const double gridTop = 142.5;

QString tr(const char *text) { return QCoreApplication::translate("Prova3dGrid", text); }

double wrapAngle(double a)
{
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}

QString number(double v)
{
    const double r = std::round(v * 2) / 2;   // half centimetres
    QString s = QLocale().toString(std::fabs(r), 'f', std::fabs(r - std::round(r)) > 0.01 ? 1 : 0);
    return (r > 0 ? QStringLiteral("+") : QStringLiteral("−")) + s;
}

// the meridian nearest to theta
const Tape &nearestMeridian(const QVector<Tape> &list, double theta)
{
    int best = 0;
    double bd = 1e9;
    for (int i = 0; i < list.size(); ++i)
    {
        const double d = std::fabs(wrapAngle(theta - list.at(i).value));
        if (d < bd) { bd = d; best = i; }
    }
    return list.at(best);
}

const Tape &nearestLevel(const QVector<Tape> &list, double y)
{
    int best = 0;
    double bd = 1e9;
    for (int i = 0; i < list.size(); ++i)
    {
        const double d = std::fabs(y - list.at(i).value);
        if (d < bd) { bd = d; best = i; }
    }
    return list.at(best);
}

// the angle reached going `arc` cm around the section at height y from angle t0
double angleAtArc(const sim::DressForm &form, double y, double t0, double arc)
{
    if (std::fabs(arc) < 1e-6) return t0;
    double lo = t0, hi = t0 + (arc > 0 ? pi / 2 : -pi / 2);
    for (int i = 0; i < 40; ++i)
    {
        const double mid = (lo + hi) / 2;
        const double a = form.arcLength(y, t0, mid);
        if ((arc > 0 && a < arc) || (arc < 0 && a > arc)) lo = mid;
        else hi = mid;
    }
    return (lo + hi) / 2;
}
} // namespace

QVector<Tape> levels()
{
    return {{tr("neck base"), 142.5}, {tr("bust"), 121.0}, {tr("under bust"), 112.0},
            {tr("waist"), 101.0}, {tr("high hip"), 92.0}, {tr("hip"), 82.0}};
}

QVector<Tape> meridians()
{
    return {{tr("centre front"), 0.0},
            {tr("princess front R"), 0.72}, {tr("princess front L"), -0.72},
            {tr("side R"), pi / 2}, {tr("side L"), -pi / 2},
            {tr("princess back R"), pi - 0.78}, {tr("princess back L"), -(pi - 0.78)},
            {tr("centre back"), pi}};
}

QVector<std::pair<QString, std::pair<double, double>>> landmarks()
{
    return {{tr("shoulder point R"), {pi / 2, 137.0}}, {tr("shoulder point L"), {-pi / 2, 137.0}},
            {tr("mid shoulder R"), {pi / 2, 139.6}}, {tr("mid shoulder L"), {-pi / 2, 139.6}},
            {tr("neck point R"), {pi / 2, 142.5}}, {tr("neck point L"), {-pi / 2, 142.5}}};
}

QString gridLabel(const sim::DressForm &form, double theta, double y)
{
    for (const auto &l : landmarks())
    {
        const sim::V3 a = form.surfacePoint(l.second.first, l.second.second);
        const sim::V3 b = form.surfacePoint(theta, y);
        if ((a - b).len() < 0.35) return l.first;
    }

    const QVector<Tape> ls = levels();
    const Tape level = nearestLevel(ls, y);
    const double dy = y - level.value;
    QString vertical = level.name;
    if (std::fabs(dy) >= 0.25) vertical += QStringLiteral(" ") + number(dy);

    const QVector<Tape> ms = meridians();
    const Tape m = nearestMeridian(ms, theta);
    double arc = form.arcLength(y, m.value, m.value + wrapAngle(theta - m.value));
    // + towards the back, - towards the front; on the centre lines + is the right side
    const bool centre = std::fabs(std::sin(m.value)) < 1e-6;
    if (!centre && m.value < 0) arc = -arc;
    QString around = m.name;
    if (std::fabs(arc) >= 0.25) around += QStringLiteral(" ") + number(arc);

    return vertical + QStringLiteral(" · ") + around + QStringLiteral(" cm");
}

void snapToGrid(const sim::DressForm &form, double &theta, double &y)
{
    for (const auto &l : landmarks())
    {
        const sim::V3 a = form.surfacePoint(l.second.first, l.second.second);
        const sim::V3 b = form.surfacePoint(theta, y);
        if ((a - b).len() < 0.9) { theta = l.second.first; y = l.second.second; return; }
    }
    const QVector<Tape> ls = levels();
    const Tape level = nearestLevel(ls, y);
    const double ys = level.value + std::round((y - level.value) / gridStep) * gridStep;
    if (std::fabs(y - ys) < 0.7) y = ys;

    const QVector<Tape> ms = meridians();
    const Tape m = nearestMeridian(ms, theta);
    const double arc = form.arcLength(y, m.value, m.value + wrapAngle(theta - m.value));
    const double snapped = std::round(arc / gridStep) * gridStep;
    if (std::fabs(arc - snapped) < 0.7) theta = wrapAngle(angleAtArc(form, y, m.value, snapped));
}

QVector<GridLine> gridLines(const sim::DressForm &form)
{
    QVector<GridLine> out;
    auto ring = [&](double y, bool tape)
    {
        GridLine g;
        g.tape = tape;
        for (int i = 0; i <= 144; ++i) g.points.push_back(form.surfacePoint(-pi + 2 * pi * i / 144, y));
        out << g;
    };
    auto upright = [&](double theta, bool tape, double top)
    {
        GridLine g;
        g.tape = tape;
        for (double y = gridBottom; y <= top + 1e-6; y += 0.5) g.points.push_back(form.surfacePoint(theta, y));
        out << g;
    };

    const QVector<Tape> ls = levels();
    for (const Tape &t : ls) ring(t.value, true);
    for (double y = 101.0 - 12 * gridStep; y <= 140.0; y += gridStep)
    {
        if (y < gridBottom) continue;
        bool onTape = false;
        for (const Tape &t : ls) if (std::fabs(t.value - y) < 0.3) onTape = true;
        if (!onTape) ring(y, false);
    }

    const QVector<Tape> ms = meridians();
    for (const Tape &t : ms)
        upright(t.value, true, std::fabs(std::fabs(t.value) - pi / 2) < 1e-6 ? gridTop : gridTop);
    // the fine uprights: every 2 cm around the bust, between each tape and the next one
    QVector<double> angles;
    for (const Tape &t : ms) angles << wrapAngle(t.value);
    std::sort(angles.begin(), angles.end());
    for (int i = 0; i < angles.size(); ++i)
    {
        const double a0 = angles.at(i);
        double a1 = i + 1 < angles.size() ? angles.at(i + 1) : angles.first() + 2 * pi;
        const double span = form.arcLength(121.0, a0, a1);
        for (double s = gridStep; s < span - gridStep * 0.5; s += gridStep)
            upright(wrapAngle(angleAtArc(form, 121.0, a0, s)), false, 140.0);
    }
    return out;
}

} // namespace prova3d
