// scenarios.cpp - qualification analytics
//  1. Exhaustive scenario enumeration (2^k outcomes of the k remaining fixtures)
//  2. Magic Number / Alive Number per team
//  3. NOVELTY: Match Criticality Index (which remaining game swings the race most)
//  4. NOVELTY: Elo-weighted Monte Carlo qualification probability
//  5. NOVELTY: NRR Chase Calculator (how big a win is needed to overtake a rival)
#include "tournament.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <numeric>

static const int MAX_EXHAUSTIVE = 18;   // 2^18 = 262,144 scenarios

// ------------------------------------------------------------------
ScenarioReport analyseScenarios(const Database& db) {
    ScenarioReport r;
    int n = (int)db.teams.size(), k = (int)db.fixtures.size();
    r.fixtures = k;
    if (k > MAX_EXHAUSTIVE) return r;           // feasible stays false
    r.feasible = true;
    r.scenarios = 1LL << k;

    auto st = computeStandings(db);
    std::vector<int> base(n), fa(k), fb(k);
    for (int i = 0; i < n; ++i) base[i] = st[i].points;
    r.ownLeft.assign(n, 0);
    for (int f = 0; f < k; ++f) {
        fa[f] = teamIndex(db, db.fixtures[f].a);
        fb[f] = teamIndex(db, db.fixtures[f].b);
        r.ownLeft[fa[f]]++; r.ownLeft[fb[f]]++;
    }

    std::vector<long long> safe(n, 0), nrrDep(n, 0), out(n, 0);
    std::vector<std::vector<char>> notSafe(n), notOut(n);
    for (int t = 0; t < n; ++t) {
        notSafe[t].assign(r.ownLeft[t] + 1, 0);
        notOut[t].assign(r.ownLeft[t] + 1, 0);
    }
    // sumQ[f][bit][team]: summed "qualification score" when fixture f ends with outcome bit
    std::vector<std::vector<std::vector<double>>> sumQ(
        k, std::vector<std::vector<double>>(2, std::vector<double>(n, 0.0)));

    std::vector<int> pts(n), wins(n);
    for (long long mask = 0; mask < r.scenarios; ++mask) {
        pts = base;
        std::fill(wins.begin(), wins.end(), 0);
        for (int f = 0; f < k; ++f) {
            int w = ((mask >> f) & 1) ? fb[f] : fa[f];
            pts[w] += db.cfg.winPts;
            wins[w]++;
        }
        for (int t = 0; t < n; ++t) {
            int better = 0, geq = 0;
            for (int o = 0; o < n; ++o) {
                if (o == t) continue;
                if (pts[o] > pts[t]) better++;
                if (pts[o] >= pts[t]) geq++;
            }
            int status;                                   // 2 safe, 1 NRR decides, 0 out
            if (geq + 1 <= db.cfg.spots)          status = 2;   // even losing every tie, still in
            else if (better + 1 > db.cfg.spots)   status = 0;   // even winning every tie, still out
            else                                  status = 1;
            if (status == 2) safe[t]++; else if (status == 1) nrrDep[t]++; else out[t]++;
            if (status != 2) notSafe[t][wins[t]] = 1;
            if (status != 0) notOut[t][wins[t]] = 1;
            double q = status / 2.0;
            for (int f = 0; f < k; ++f) sumQ[f][(mask >> f) & 1][t] += q;
        }
    }

    r.safePct.resize(n); r.nrrPct.resize(n); r.outPct.resize(n);
    r.magicWins.assign(n, -1); r.aliveWins.assign(n, -1);
    for (int t = 0; t < n; ++t) {
        r.safePct[t] = 100.0 * safe[t]   / r.scenarios;
        r.nrrPct[t]  = 100.0 * nrrDep[t] / r.scenarios;
        r.outPct[t]  = 100.0 * out[t]    / r.scenarios;
        // Magic number: smallest w such that EVERY scenario with >= w wins is safe
        for (int w = 0; w <= r.ownLeft[t]; ++w) {
            bool ok = true;
            for (int x = w; x <= r.ownLeft[t]; ++x) if (notSafe[t][x]) { ok = false; break; }
            if (ok) { r.magicWins[t] = w; break; }
        }
        // Alive number: smallest w for which some scenario still keeps the team in the race
        for (int w = 0; w <= r.ownLeft[t]; ++w)
            if (notOut[t][w]) { r.aliveWins[t] = w; break; }
    }

    // Match Criticality Index: total swing in qualification score across all teams
    // between the two possible results of a fixture. 0 = dead rubber, higher = bigger.
    double half = (double)(r.scenarios / 2);
    r.criticality.assign(k, 0.0);
    r.avgQualIfAWins.assign(k, 0.0);
    r.avgQualIfBWins.assign(k, 0.0);
    for (int f = 0; f < k; ++f) {
        double swing = 0, sa = 0, sb = 0;
        for (int t = 0; t < n; ++t) {
            double qa = sumQ[f][0][t] / half, qb = sumQ[f][1][t] / half;
            swing += std::fabs(qa - qb);
            if (t == fa[f]) { sa = qa - qb; }
            if (t == fb[f]) { sb = qb - qa; }
        }
        r.criticality[f] = swing;
        r.avgQualIfAWins[f] = sa;   // gain for team A from winning
        r.avgQualIfBWins[f] = sb;   // gain for team B from winning
    }
    return r;
}

void printScenarioReport(const Database& db, const ScenarioReport& r) {
    if (!r.feasible) {
        std::printf("\n%d fixtures remain - too many to enumerate (limit %d). Use Monte Carlo.\n",
                    r.fixtures, MAX_EXHAUSTIVE);
        return;
    }
    std::printf("\nQualification scenarios: %lld outcomes of %d remaining fixtures (top %d qualify)\n",
                r.scenarios, r.fixtures, db.cfg.spots);
    std::printf("Safe = in on points alone | NRR = decided by NRR | Out = out on points alone\n\n");
    std::printf(" %-6s %5s %7s %7s %7s  %-8s %-8s  %s\n", "Team", "Left", "Safe%", "NRR%", "Out%",
                "Magic#", "Alive#", "Verdict");
    std::printf(" ------------------------------------------------------------------------\n");
    auto st = rankStandings(computeStandings(db));
    for (auto& s : st) {
        int t = teamIndex(db, s.name);
        std::string magic = r.magicWins[t] < 0 ? "-" : std::to_string(r.magicWins[t]) + " win(s)";
        std::string alive = r.aliveWins[t] < 0 ? "-" : std::to_string(r.aliveWins[t]) + " win(s)";
        const char* verdict;
        if (r.safePct[t] > 99.999)       verdict = "QUALIFIED";
        else if (r.outPct[t] > 99.999)   verdict = "ELIMINATED";
        else if (r.outPct[t] < 1e-9)     verdict = "Cannot be knocked out on points (NRR may decide)";
        else if (r.safePct[t] < 1e-9)    verdict = "Needs NRR luck - cannot be safe on points";
        else                             verdict = "In the hunt";
        std::printf(" %-6s %5d %6.1f%% %6.1f%% %6.1f%%  %-8s %-8s  %s\n", s.name.c_str(),
                    r.ownLeft[t], r.safePct[t], r.nrrPct[t], r.outPct[t],
                    magic.c_str(), alive.c_str(), verdict);
    }
    std::printf("\nMagic#: wins from own remaining games that GUARANTEE a top-%d finish on points,"
                "\n        whatever happens elsewhere.  Alive#: wins needed to keep a mathematical chance.\n",
                db.cfg.spots);
}

void printCriticality(const Database& db, const ScenarioReport& r) {
    if (!r.feasible) { std::printf("\nToo many fixtures for criticality analysis.\n"); return; }
    std::vector<int> idx(r.fixtures);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int x, int y) { return r.criticality[x] > r.criticality[y]; });
    double mx = r.criticality[idx[0]] > 0 ? r.criticality[idx[0]] : 1;
    std::printf("\nMATCH CRITICALITY INDEX (total swing in qualification odds across ALL teams)\n");
    std::printf("Gain = how much a win lifts that team's qualification score (0..1 scale)\n\n");
    std::printf(" %-4s %-13s %6s  %-22s  %s\n", "Rank", "Fixture", "MCI", "", "Win swing for each side");
    for (size_t i = 0; i < idx.size(); ++i) {
        int f = idx[i];
        int bar = (int)std::round(20.0 * r.criticality[f] / mx);
        std::printf(" %-4zu %-5s v %-5s %6.2f  %-22s  %s %+.2f | %s %+.2f\n", i + 1,
                    db.fixtures[f].a.c_str(), db.fixtures[f].b.c_str(), r.criticality[f],
                    std::string(bar, '#').c_str(), db.fixtures[f].a.c_str(), r.avgQualIfAWins[f],
                    db.fixtures[f].b.c_str(), r.avgQualIfBWins[f]);
    }
}

// ------------------------------------------------------------------
std::vector<MCResult> monteCarlo(const Database& db, int sims, unsigned seed) {
    int n = (int)db.teams.size(), k = (int)db.fixtures.size();
    auto st = computeStandings(db);
    auto elo = computeElo(db);
    std::vector<int> fa(k), fb(k);
    for (int f = 0; f < k; ++f) {
        fa[f] = teamIndex(db, db.fixtures[f].a);
        fb[f] = teamIndex(db, db.fixtures[f].b);
    }
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0), D(0.05, 0.60);
    std::vector<MCResult> res(n);
    std::vector<int> pts(n), order(n);
    std::vector<double> nrr(n);
    std::vector<long long> inTop(n, 0), first(n, 0);
    std::vector<double> posSum(n, 0.0);

    for (int s = 0; s < sims; ++s) {
        for (int i = 0; i < n; ++i) { pts[i] = st[i].points; nrr[i] = st[i].nrr; }
        for (int f = 0; f < k; ++f) {
            int a = fa[f], b = fb[f];
            double pa = 1.0 / (1.0 + std::pow(10.0, (elo[b] - elo[a]) / 400.0));
            double d = D(rng);                      // typical NRR swing of one match
            if (U(rng) < pa) { pts[a] += db.cfg.winPts; nrr[a] += d; nrr[b] -= d; }
            else             { pts[b] += db.cfg.winPts; nrr[b] += d; nrr[a] -= d; }
        }
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int x, int y) {
            if (pts[x] != pts[y]) return pts[x] > pts[y];
            return nrr[x] > nrr[y];
        });
        for (int pos = 0; pos < n; ++pos) {
            int t = order[pos];
            posSum[t] += pos + 1;
            if (pos < db.cfg.spots) inTop[t]++;
            if (pos == 0) first[t]++;
        }
    }
    for (int t = 0; t < n; ++t) {
        res[t].topN = 100.0 * inTop[t] / sims;
        res[t].top1 = 100.0 * first[t] / sims;
        res[t].avgPos = posSum[t] / sims;
    }
    return res;
}

void printMonteCarlo(const Database& db, const std::vector<MCResult>& r, int sims) {
    auto elo = computeElo(db);
    std::printf("\nELO-WEIGHTED MONTE CARLO (%d simulated seasons; strength from margin-aware Elo)\n\n", sims);
    std::printf(" %-6s %7s %10s %10s %9s\n", "Team", "Elo", "Top-N %", "Finish 1st%", "Avg pos");
    std::printf(" --------------------------------------------------\n");
    auto st = rankStandings(computeStandings(db));
    for (auto& s : st) {
        int t = teamIndex(db, s.name);
        std::printf(" %-6s %7.0f %9.1f%% %9.1f%% %9.2f\n", s.name.c_str(), elo[t], r[t].topN,
                    r[t].top1, r[t].avgPos);
    }
}

// ------------------------------------------------------------------
// NRR Chase Calculator: team t plays 'opp'. How emphatic must the win be for t's NRR
// to reach rival's NRR?  Bat first: t scores 'score' in full overs -> max runs opp may
// score.  Chasing: opp scores 'score' in full overs -> max balls t can take to win.
ChaseResult nrrChase(const Database& db, int t, int rival, int opp, bool batFirst, int score) {
    ChaseResult cr;
    auto base = computeStandings(db);
    int full = db.cfg.maxOvers * 6;

    auto test = [&](int limit, double& nt, double& nr) {
        auto st = base;
        Innings it, io;
        if (batFirst) { it = {score, full, 5}; io = {limit, full, 5}; }
        else          { io = {score, full, 5}; it = {score + 1, limit, 3}; }
        applyMatch(st, t, opp, it, io, false, db.cfg);
        nt = st[t].nrr; nr = st[rival].nrr;
        return nt >= nr;
    };

    double nt, nr;
    if (batFirst) {
        for (int runs = score - 1; runs >= 0; --runs) {
            if (test(runs, nt, nr)) {
                cr.possible = true; cr.limit = runs; cr.nrrTeam = nt; cr.nrrRival = nr;
                cr.anyWinWorks = (runs == score - 1);
                return cr;
            }
        }
    } else {
        for (int balls = full; balls >= 1; --balls) {
            if (test(balls, nt, nr)) {
                cr.possible = true; cr.limit = balls; cr.nrrTeam = nt; cr.nrrRival = nr;
                cr.anyWinWorks = (balls == full);
                return cr;
            }
        }
    }
    test(batFirst ? 0 : 1, nt, nr);          // report best-case numbers if impossible
    cr.nrrTeam = nt; cr.nrrRival = nr;
    return cr;
}
