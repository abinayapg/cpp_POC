// main.cpp - menu-driven console front end
#include "tournament.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

static std::string ask(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}
static int askInt(const std::string& p) { return std::atoi(ask(p).c_str()); }

static void showFixtures(const Database& db) {
    std::printf("\nRemaining fixtures (%zu):\n", db.fixtures.size());
    for (size_t i = 0; i < db.fixtures.size(); ++i)
        std::printf("  %2zu. %s v %s\n", i + 1, db.fixtures[i].a.c_str(), db.fixtures[i].b.c_str());
}

static void addResult(Database& db, const std::string& path) {
    Match m;
    m.a = ask("Team A (batted first): ");
    m.b = ask("Team B: ");
    if (teamIndex(db, m.a) < 0 || teamIndex(db, m.b) < 0 || m.a == m.b) {
        std::printf("Unknown or identical teams.\n"); return;
    }
    std::string nr = ask("Abandoned / no result? (y/n): ");
    m.noResult = (nr == "y" || nr == "Y");
    if (!m.noResult) {
        m.ia.runs = askInt(m.a + " runs: ");
        m.ia.balls = oversToBalls(ask(m.a + " overs faced (e.g. 19.3): "));
        m.ia.wkts = askInt(m.a + " wickets lost: ");
        m.ib.runs = askInt(m.b + " runs: ");
        m.ib.balls = oversToBalls(ask(m.b + " overs faced (e.g. 18.4): "));
        m.ib.wkts = askInt(m.b + " wickets lost: ");
        int cap = db.cfg.maxOvers * 6;
        if (m.ia.balls < 0 || m.ib.balls < 0 || m.ia.balls > cap || m.ib.balls > cap) {
            std::printf("Invalid overs.\n"); return;
        }
    }
    m.id = db.matches.empty() ? 1 : db.matches.back().id + 1;
    db.matches.push_back(m);
    for (size_t i = 0; i < db.fixtures.size(); ++i) {          // retire the fixture
        auto& f = db.fixtures[i];
        if ((f.a == m.a && f.b == m.b) || (f.a == m.b && f.b == m.a)) {
            db.fixtures.erase(db.fixtures.begin() + i); break;
        }
    }
    saveDatabase(path, db) ? std::printf("Result saved as match #%d in %s\n", m.id, path.c_str())
                           : std::printf("Could not write %s\n", path.c_str());
}

static void chaseMenu(const Database& db) {
    int t = teamIndex(db, ask("Your team: "));
    int r = teamIndex(db, ask("Rival to overtake on NRR: "));
    int o = teamIndex(db, ask("Opposition in the upcoming match: "));
    if (t < 0 || r < 0 || o < 0 || t == o) { std::printf("Invalid team(s).\n"); return; }
    std::string mode = ask("Batting first or chasing? (b/c): ");
    bool bf = (mode == "b" || mode == "B");
    int score = askInt(bf ? "Runs you expect to score in a full innings: "
                          : "Runs the opposition scores in a full innings: ");
    ChaseResult c = nrrChase(db, t, r, o, bf, score);
    if (!c.possible) {
        std::printf("\nEven the most dominant win cannot lift %s above %s (best %+.3f vs %+.3f).\n",
                    db.teams[t].c_str(), db.teams[r].c_str(), c.nrrTeam, c.nrrRival);
    } else if (c.anyWinWorks) {
        std::printf("\nAny win lifts %s past %s (%+.3f vs %+.3f).\n", db.teams[t].c_str(),
                    db.teams[r].c_str(), c.nrrTeam, c.nrrRival);
    } else if (bf) {
        std::printf("\nScore %d, then restrict %s to %d or fewer  ->  NRR %+.3f vs %s %+.3f"
                    "  (win margin of at least %d runs).\n", score, db.teams[o].c_str(), c.limit,
                    c.nrrTeam, db.teams[r].c_str(), c.nrrRival, score - c.limit);
    } else {
        std::printf("\nChase %d inside %s overs  ->  NRR %+.3f vs %s %+.3f.\n", score + 1,
                    ballsToOvers(c.limit).c_str(), c.nrrTeam, db.teams[r].c_str(), c.nrrRival);
    }
}

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : "tournament.dat";
    Database db;
    if (!loadDatabase(path, db)) { std::printf("Cannot read database '%s'\n", path.c_str()); return 1; }

    ScenarioReport cache; bool stale = true;
    while (true) {
        std::printf("\n===== TOURNAMENT POINTS TABLE & NRR ENGINE =====\n"
                    " 1. Points table (with NRR)\n 2. Add match result\n"
                    " 3. Qualification scenarios + Magic Numbers\n"
                    " 4. Match Criticality Index (which fixture matters most)\n"
                    " 5. Monte Carlo qualification odds (Elo-weighted)\n"
                    " 6. NRR Chase Calculator\n 7. Remaining fixtures\n 0. Exit\n");
        std::string c = ask("Choice: ");
        if (c == "0" || !std::cin) break;
        if (c == "1") printStandings(db, rankStandings(computeStandings(db)));
        else if (c == "2") { addResult(db, path); stale = true; }
        else if (c == "3" || c == "4") {
            if (stale) { cache = analyseScenarios(db); stale = false; }
            c == "3" ? printScenarioReport(db, cache) : printCriticality(db, cache);
        }
        else if (c == "5") { int s = 20000; printMonteCarlo(db, monteCarlo(db, s, 2026), s); }
        else if (c == "6") chaseMenu(db);
        else if (c == "7") showFixtures(db);
    }
    return 0;
}
