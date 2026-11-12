// engine.cpp - points table, Net Run Rate and Elo strength
#include "tournament.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

static void refreshNrr(Standing& s) {
    double rf = s.forBalls > 0 ? s.forRuns / (s.forBalls / 6.0) : 0.0;
    double ra = s.agBalls  > 0 ? s.agRuns  / (s.agBalls  / 6.0) : 0.0;
    s.nrr = rf - ra;
}

// ICC rule: an all-out innings is charged the FULL quota of overs.
static int chargedBalls(const Innings& in, const Config& cfg) {
    return in.wkts >= 10 ? cfg.maxOvers * 6 : in.balls;
}

void applyMatch(std::vector<Standing>& st, int a, int b, const Innings& ia,
                const Innings& ib, bool noResult, const Config& cfg) {
    Standing &A = st[a], &B = st[b];
    A.played++; B.played++;
    if (noResult) {                       // abandoned: shared points, NRR untouched
        A.nr++; B.nr++;
        A.points += cfg.nrPts; B.points += cfg.nrPts;
        return;
    }
    A.forRuns += ia.runs; A.forBalls += chargedBalls(ia, cfg);
    A.agRuns  += ib.runs; A.agBalls  += chargedBalls(ib, cfg);
    B.forRuns += ib.runs; B.forBalls += chargedBalls(ib, cfg);
    B.agRuns  += ia.runs; B.agBalls  += chargedBalls(ia, cfg);
    if (ia.runs > ib.runs)      { A.won++; B.lost++; A.points += cfg.winPts; }
    else if (ib.runs > ia.runs) { B.won++; A.lost++; B.points += cfg.winPts; }
    else { A.tied++; B.tied++; A.points += cfg.tiePts; B.points += cfg.tiePts; }
    refreshNrr(A); refreshNrr(B);
}

std::vector<Standing> computeStandings(const Database& db) {
    std::vector<Standing> st(db.teams.size());
    for (size_t i = 0; i < st.size(); ++i) st[i].name = db.teams[i];
    for (auto& m : db.matches)
        applyMatch(st, teamIndex(db, m.a), teamIndex(db, m.b), m.ia, m.ib, m.noResult, db.cfg);
    return st;
}

std::vector<Standing> rankStandings(std::vector<Standing> st) {
    std::stable_sort(st.begin(), st.end(), [](const Standing& x, const Standing& y) {
        if (x.points != y.points) return x.points > y.points;
        if (std::fabs(x.nrr - y.nrr) > 1e-9) return x.nrr > y.nrr;
        return x.won > y.won;
    });
    return st;
}

void printStandings(const Database& db, const std::vector<Standing>& r) {
    std::printf("\n %-3s %-6s %3s %3s %3s %3s %3s %4s %9s\n",
                "#", "Team", "P", "W", "L", "T", "NR", "Pts", "NRR");
    std::printf(" -------------------------------------------------\n");
    for (size_t i = 0; i < r.size(); ++i) {
        std::printf(" %-3zu %-6s %3d %3d %3d %3d %3d %4d %+9.3f\n", i + 1, r[i].name.c_str(),
                    r[i].played, r[i].won, r[i].lost, r[i].tied, r[i].nr, r[i].points, r[i].nrr);
        if ((int)i + 1 == db.cfg.spots && i + 1 < r.size())
            std::printf(" ----------------- qualification line ----------------\n");
    }
}

// Margin-aware Elo: big wins move ratings more. Used by the Monte Carlo module.
std::vector<double> computeElo(const Database& db) {
    std::vector<double> elo(db.teams.size(), 1500.0);
    for (auto& m : db.matches) {
        if (m.noResult) continue;
        int a = teamIndex(db, m.a), b = teamIndex(db, m.b);
        double ea = 1.0 / (1.0 + std::pow(10.0, (elo[b] - elo[a]) / 400.0));
        double sa = m.ia.runs > m.ib.runs ? 1.0 : (m.ia.runs < m.ib.runs ? 0.0 : 0.5);
        double margin = 1.0 + std::min(1.0, std::abs(m.ia.runs - m.ib.runs) / 60.0);
        double k = 24.0 * margin;
        elo[a] += k * (sa - ea);
        elo[b] -= k * (sa - ea);
    }
    return elo;
}
