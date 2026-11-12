// tournament.h - shared data model for the Points Table & NRR Engine
#pragma once
#include <string>
#include <vector>

// ---------- Core data model ----------
struct Config {
    int maxOvers = 20;   // overs per innings
    int spots    = 4;    // how many teams qualify
    int winPts   = 2;    // points for a win
    int tiePts   = 1;    // points each for a tie
    int nrPts    = 1;    // points each for no result
};

struct Innings {
    int runs  = 0;
    int balls = 0;       // legal balls faced (19.3 overs == 117)
    int wkts  = 0;       // wickets lost (10 == all out)
};

struct Match {
    int id = 0;
    std::string a, b;    // a = innings 'ia', b = innings 'ib'
    Innings ia, ib;
    bool noResult = false;
};

struct Fixture { std::string a, b; };   // a not-yet-played match

struct Database {
    Config cfg;
    std::vector<std::string> teams;
    std::vector<Match>   matches;
    std::vector<Fixture> fixtures;
};

struct Standing {
    std::string name;
    int played = 0, won = 0, lost = 0, tied = 0, nr = 0, points = 0;
    int forRuns = 0, forBalls = 0, agRuns = 0, agBalls = 0;
    double nrr = 0.0;
};

// ---------- database.cpp : .dat persistence ----------
bool        loadDatabase(const std::string& path, Database& db);
bool        saveDatabase(const std::string& path, const Database& db);
int         oversToBalls(const std::string& overs);   // "19.3" -> 117, -1 if invalid
std::string ballsToOvers(int balls);                  // 117 -> "19.3"
int         teamIndex(const Database& db, const std::string& name);

// ---------- engine.cpp : points table + NRR ----------
void  applyMatch(std::vector<Standing>& st, int a, int b, const Innings& ia,
                 const Innings& ib, bool noResult, const Config& cfg);
std::vector<Standing> computeStandings(const Database& db);       // db.teams order
std::vector<Standing> rankStandings(std::vector<Standing> st);    // sorted copy
void  printStandings(const Database& db, const std::vector<Standing>& ranked);
std::vector<double> computeElo(const Database& db);               // margin-aware Elo

// ---------- scenarios.cpp : qualification analytics ----------
struct ScenarioReport {
    bool feasible = false;                 // false if too many fixtures to enumerate
    int  fixtures = 0;
    long long scenarios = 0;
    std::vector<double> safePct, nrrPct, outPct;   // per team (db.teams order)
    std::vector<int>    ownLeft;                    // remaining games per team
    std::vector<int>    magicWins;                  // wins that GUARANTEE top-N (-1 = none)
    std::vector<int>    aliveWins;                  // wins needed to stay mathematically alive (-1 = dead)
    std::vector<double> criticality;                // Match Criticality Index per fixture
    std::vector<double> avgQualIfAWins, avgQualIfBWins; // per fixture: swing detail (sum over teams)
};
ScenarioReport analyseScenarios(const Database& db);
void printScenarioReport(const Database& db, const ScenarioReport& r);
void printCriticality(const Database& db, const ScenarioReport& r);

struct MCResult { double topN = 0, top1 = 0, avgPos = 0; };
std::vector<MCResult> monteCarlo(const Database& db, int sims, unsigned seed);
void printMonteCarlo(const Database& db, const std::vector<MCResult>& r, int sims);

struct ChaseResult {
    bool   possible = false;   // can a win ever lift T's NRR to rival's?
    bool   anyWinWorks = false;
    int    limit = 0;          // bat first: max opposition runs | chasing: max balls to win
    double nrrTeam = 0, nrrRival = 0;
};
ChaseResult nrrChase(const Database& db, int t, int rival, int opp, bool batFirst, int score);
