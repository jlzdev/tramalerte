#include "depart.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace depart {

static int borner(long v, int min, int max) {
  if (v < min) return min;
  if (v > max) return max;
  return static_cast<int>(v);
}

Reglages normaliserReglages(long departMin) {
  Reglages r;
  r.departMin = borner(departMin, DEPART_MIN_MIN, DEPART_MIN_MAX);
  return r;
}

Verdict evaluer(const std::vector<Passage>& passages, int64_t nowMs, int64_t fetchedAtMs, const Reglages& r) {
  const double ecoule = std::max<double>(0, (nowMs - fetchedAtMs) / 1000.0);
  const double avance = r.departMin * 60.0;
  Verdict v;
  for (const Passage& p : passages) {
    Candidat c;
    c.passage = p;
    c.tramSec = p.secondes - ecoule;
    c.resteSec = c.tramSec - avance;
    if (c.resteSec < 0) c.etat = Etat::Rate;
    else if (c.resteSec < COURS_FENETRE_SEC) c.etat = Etat::Cours;
    else if (c.resteSec < COURS_FENETRE_SEC + PREPARE_FENETRE_SEC) c.etat = Etat::Prepare;
    else c.etat = Etat::Tranquille;
    c.departMs = nowMs + static_cast<int64_t>(c.resteSec * 1000);
    c.tramMs = nowMs + static_cast<int64_t>(c.tramSec * 1000);
    v.candidats.push_back(c);
  }
  std::stable_sort(v.candidats.begin(), v.candidats.end(),
                   [](const Candidat& a, const Candidat& b) { return a.tramSec < b.tramSec; });
  for (size_t i = 0; i < v.candidats.size(); i++) {
    if (v.candidats[i].etat != Etat::Rate) {
      v.cible = static_cast<int>(i);
      v.etat = v.candidats[i].etat;
      break;
    }
  }
  return v;
}

std::string fmtHM(int64_t ms) {
  time_t t = static_cast<time_t>(ms / 1000);
  struct tm local;
  localtime_r(&t, &local);
  std::string s = std::to_string(local.tm_hour) + "h";
  if (local.tm_min) {
    if (local.tm_min < 10) s += "0";
    s += std::to_string(local.tm_min);
  }
  return s;
}

std::string fmtDuree(double sec, bool precis) {
  long s = std::lround(std::max(0.0, sec));
  long m = s / 60;
  if (!precis) return std::to_string(m) + " min";
  if (m == 0) return std::to_string(s) + " s";
  long reste = s % 60;
  return std::to_string(m) + " min " + (reste < 10 ? "0" : "") + std::to_string(reste);
}

Libelles libelles(const Verdict& v, int64_t nowMs) {
  const Candidat* c = v.cibleOuNull();
  if (!c) return {"Pas de tram annoncé", "--", ""};
  const Passage& p = c->passage;
  const std::string theorique = p.fiable ? "" : " (horaire théorique)";
  const std::string tram = p.ligne + " vers " + p.destination;
  if (c->etat == Etat::Cours) {
    return {"Pars maintenant", fmtDuree(c->resteSec, true),
            "Tram " + tram + " dans " + fmtDuree(c->tramSec, true) + theorique + "."};
  }
  if (c->etat == Etat::Prepare) {
    return {"Prépare-toi", fmtDuree(c->resteSec, true),
            "Départ à " + fmtHM(c->departMs) + ", tram " + tram + " à " + fmtHM(c->tramMs) + theorique + "."};
  }
  return {"Tu as le temps", fmtDuree(c->resteSec),
          "Départ à " + fmtHM(std::max(nowMs, c->departMs)) + ", tram " + tram + " à " + fmtHM(c->tramMs) + theorique + "."};
}

}  // namespace depart
